#include "ppo_trainer.h"
#include "mistolito.h"
#include "storage_task.h"
#include "critic_features.h"
#include "value_head.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "spi_bus.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

static const char *TAG = "PPO";

#define PPO_MALLOC(size) heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define PPO_FREE(ptr) do { if (ptr) { heap_caps_free(ptr); ptr = NULL; } } while(0)

ppo_config_t ppo_config_default(void)
{
    return (ppo_config_t){
        .lr = 3e-4f,
        .gamma = 0.99f,
        .lambda = 0.95f,
        .clip_coef = 0.2f,
        .ent_coef = 0.01f,
        .vf_coef = 0.5f,
        .max_grad_norm = 0.5f,
        .momentum = 0.9f,
        .epochs = 4,
        .batch_size = 64,
    };
}

static void softmax(const float *input, float *output, int n)
{
    float max_val = input[0];
    for (int i = 1; i < n; i++) {
        if (input[i] > max_val) max_val = input[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        output[i] = expf(input[i] - max_val);
        sum += output[i];
    }
    for (int i = 0; i < n; i++) {
        output[i] /= sum;
    }
}

static float compute_entropy(const float *probs, int n)
{
    float ent = 0.0f;
    for (int i = 0; i < n; i++) {
        if (probs[i] > 1e-8f) {
            ent -= probs[i] * logf(probs[i]);
        }
    }
    return ent;
}

static void policy_forward(const policy_head_t *ph, const float *features,
                           uint8_t action, float *prob_out, float *log_prob_out)
{
    float logits[PPO_MAX_ACTIONS];
    for (int a = 0; a < ph->num_actions; a++) {
        float sum = ph->b[a];
        for (int f = 0; f < PPO_STATE_SIZE; f++) {
            sum += features[f] * ph->W[f * ph->num_actions + a];
        }
        logits[a] = sum;
    }

    float probs[PPO_MAX_ACTIONS];
    softmax(logits, probs, ph->num_actions);

    *prob_out = probs[action];
    *log_prob_out = logf(probs[action] + 1e-8f);
}

static esp_err_t load_all_chunks(replay_transition_t **out_transitions, uint32_t *out_total)
{
    replay_stats_t stats;
    if (!storage_replay_get_stats(&stats)) {
        ESP_LOGW(TAG, "load_all_chunks: failed to get stats");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "load_all_chunks: stats.total_chunks=%lu, stats.total_transitions=%lu",
             (unsigned long)stats.total_chunks, (unsigned long)stats.total_transitions);

    if (stats.total_chunks == 0 || stats.total_transitions == 0) {
        *out_transitions = NULL;
        *out_total = 0;
        return ESP_OK;
    }

    uint32_t total = stats.total_transitions;
    replay_transition_t *buf = PPO_MALLOC(total * sizeof(replay_transition_t));
    if (!buf) return ESP_ERR_NO_MEM;

    uint32_t idx = 0;
    for (uint32_t c = 0; c < stats.total_chunks; c++) {
        uint32_t count = 0;
        if (!storage_replay_read_chunk(c + 1, NULL, &count)) continue;

        if (count == 0) continue;

        replay_transition_t *chunk_buf = PPO_MALLOC(count * sizeof(replay_transition_t));
        if (!chunk_buf) continue;

        if (!storage_replay_read_chunk(c + 1, chunk_buf, &count)) {
            PPO_FREE(chunk_buf);
            continue;
        }

        uint32_t to_copy = count;
        if (idx + to_copy > total) to_copy = total - idx;
        memcpy(&buf[idx], chunk_buf, to_copy * sizeof(replay_transition_t));
        idx += to_copy;
        PPO_FREE(chunk_buf);

        if (idx >= total) break;
    }

    *out_transitions = buf;
    *out_total = idx;
    return ESP_OK;
}

esp_err_t ppo_train_policy(policy_head_t *ph, const ppo_config_t *config,
                           training_progress_t *progress)
{
    if (!ph || !ph->W || !ph->b) return ESP_ERR_INVALID_STATE;

    replay_transition_t *transitions = NULL;
    uint32_t total_transitions = 0;

    esp_err_t ret = load_all_chunks(&transitions, &total_transitions);
    if (ret != ESP_OK) return ret;

    if (total_transitions < (uint32_t)config->batch_size) {
        ESP_LOGW(TAG, "Not enough transitions: %lu < %d",
                 (unsigned long)total_transitions, config->batch_size);
        PPO_FREE(transitions);
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "Training PPO: %lu transitions, %d actions, %d epochs",
             (unsigned long)total_transitions, ph->num_actions, config->epochs);

    value_head_t vh;
    if (value_head_init(&vh) != ESP_OK) {
        PPO_FREE(transitions);
        return ESP_ERR_NO_MEM;
    }
    value_head_load(&vh);

    float *grad_w = PPO_MALLOC(PPO_STATE_SIZE * ph->num_actions * sizeof(float));
    float *grad_b = PPO_MALLOC(ph->num_actions * sizeof(float));
    float *vel_w = PPO_MALLOC(PPO_STATE_SIZE * ph->num_actions * sizeof(float));
    float *vel_b = PPO_MALLOC(ph->num_actions * sizeof(float));

    if (!grad_w || !grad_b || !vel_w || !vel_b) {
        value_head_deinit(&vh);
        PPO_FREE(transitions);
        PPO_FREE(grad_w); PPO_FREE(grad_b);
        PPO_FREE(vel_w); PPO_FREE(vel_b);
        return ESP_ERR_NO_MEM;
    }

    memset(vel_w, 0, PPO_STATE_SIZE * ph->num_actions * sizeof(float));
    memset(vel_b, 0, ph->num_actions * sizeof(float));

    float total_loss = 0.0f;
    int total_updates = 0;

    for (int epoch = 0; epoch < config->epochs; epoch++) {
        for (uint32_t start = 0; start + config->batch_size <= total_transitions;
             start += config->batch_size) {

            memset(grad_w, 0, PPO_STATE_SIZE * ph->num_actions * sizeof(float));
            memset(grad_b, 0, ph->num_actions * sizeof(float));

            float batch_loss = 0.0f;

            for (int i = 0; i < config->batch_size; i++) {
                replay_transition_t *t = &transitions[start + i];

                float state_local[PPO_STATE_SIZE];
                float next_local[PPO_STATE_SIZE];
                memcpy(state_local, t->state, sizeof(float) * PPO_STATE_SIZE);
                memcpy(next_local, t->next_state, sizeof(float) * PPO_STATE_SIZE);

                float prob_new, log_prob_new;
                policy_forward(ph, state_local, t->action, &prob_new, &log_prob_new);

                float val_state = 0.0f;
                float val_next_state = 0.0f;
                value_head_forward(&vh, state_local, &val_state);
                value_head_forward(&vh, next_local, &val_next_state);

                float advantage;
                if (t->done) {
                    advantage = t->reward - val_state;
                } else {
                    advantage = t->reward + config->gamma * val_next_state - val_state;
                }

                float ratio = prob_new / (prob_new + 1e-8f);
                float clipped = fmaxf(fminf(ratio, 1.0f + config->clip_coef),
                                      1.0f - config->clip_coef);

                float surr1 = ratio * advantage;
                float surr2 = clipped * advantage;
                float policy_loss = -fminf(surr1, surr2);

                float entropy = compute_entropy((float[]){prob_new}, 1);
                float loss = policy_loss - config->ent_coef * entropy;
                batch_loss += loss;

                for (int a = 0; a < ph->num_actions; a++) {
                    float dk = ((a == t->action) ? (prob_new - 1.0f) : prob_new) * advantage;
                    for (int f = 0; f < PPO_STATE_SIZE; f++) {
                        grad_w[f * ph->num_actions + a] += state_local[f] * dk;
                    }
                    grad_b[a] += dk;
                }
            }

            float scale = 1.0f / config->batch_size;
            for (int j = 0; j < PPO_STATE_SIZE * ph->num_actions; j++) {
                grad_w[j] *= scale;
            }
            for (int j = 0; j < ph->num_actions; j++) {
                grad_b[j] *= scale;
            }

            float grad_norm = 0.0f;
            for (int j = 0; j < PPO_STATE_SIZE * ph->num_actions; j++) {
                grad_norm += grad_w[j] * grad_w[j];
            }
            for (int j = 0; j < ph->num_actions; j++) {
                grad_norm += grad_b[j] * grad_b[j];
            }
            grad_norm = sqrtf(grad_norm + 1e-8f);

            if (grad_norm > config->max_grad_norm) {
                float s = config->max_grad_norm / grad_norm;
                for (int j = 0; j < PPO_STATE_SIZE * ph->num_actions; j++) {
                    grad_w[j] *= s;
                }
                for (int j = 0; j < ph->num_actions; j++) {
                    grad_b[j] *= s;
                }
            }

            for (int j = 0; j < PPO_STATE_SIZE * ph->num_actions; j++) {
                vel_w[j] = config->momentum * vel_w[j] - config->lr * grad_w[j];
                ph->W[j] += vel_w[j];
            }
            for (int j = 0; j < ph->num_actions; j++) {
                vel_b[j] = config->momentum * vel_b[j] - config->lr * grad_b[j];
                ph->b[j] += vel_b[j];
            }

            batch_loss /= config->batch_size;
            total_loss += batch_loss;
            total_updates++;
        }
    }

    value_head_deinit(&vh);
    PPO_FREE(transitions);
    PPO_FREE(grad_w);
    PPO_FREE(grad_b);
    PPO_FREE(vel_w);
    PPO_FREE(vel_b);

    float avg_loss = (total_updates > 0) ? total_loss / total_updates : 0.0f;

    if (progress) {
        progress->loss = avg_loss;
        progress->accepted = true;
    }

    ESP_LOGI(TAG, "Training done: %d updates, loss=%.4f", total_updates, avg_loss);
    return ESP_OK;
}

esp_err_t ppo_save_checkpoint(policy_head_t *ph, uint32_t epoch, float loss)
{
    char w_path[128];
    char b_path[128];
    char meta_path[128];

    snprintf(w_path, sizeof(w_path), PPO_CHECKPOINT_POLICY_W, (int)epoch);
    snprintf(b_path, sizeof(b_path), PPO_CHECKPOINT_POLICY_B, (int)epoch);
    snprintf(meta_path, sizeof(meta_path), PPO_CHECKPOINT_META, (int)epoch);

    spi_bus_lock();

    FILE *fw = fopen(w_path, "wb");
    if (!fw) { spi_bus_unlock(); return ESP_FAIL; }
    fwrite(ph->W, sizeof(float), 9 * ph->num_actions, fw);
    fclose(fw);

    FILE *fb = fopen(b_path, "wb");
    if (!fb) { spi_bus_unlock(); return ESP_FAIL; }
    fwrite(ph->b, sizeof(float), ph->num_actions, fb);
    fclose(fb);

    FILE *fm = fopen(meta_path, "wb");
    if (!fm) { spi_bus_unlock(); return ESP_FAIL; }
    ppo_checkpoint_meta_t meta = {
        .magic = PPO_CHECKPOINT_MAGIC,
        .version = PPO_CHECKPOINT_VERSION,
        .epoch = epoch,
        .loss = loss,
        .timestamp = 0,
        .checksum = 0,
        .num_actions = ph->num_actions,
    };
    fwrite(&meta, sizeof(meta), 1, fm);
    fclose(fm);

    spi_bus_unlock();

    ESP_LOGI(TAG, "Saved checkpoint epoch=%lu loss=%.4f", (unsigned long)epoch, loss);
    return ESP_OK;
}

esp_err_t ppo_load_checkpoint(policy_head_t *ph, uint32_t epoch)
{
    char w_path[128];
    char b_path[128];

    snprintf(w_path, sizeof(w_path), PPO_CHECKPOINT_POLICY_W, (int)epoch);
    snprintf(b_path, sizeof(b_path), PPO_CHECKPOINT_POLICY_B, (int)epoch);

    spi_bus_lock();

    FILE *fw = fopen(w_path, "rb");
    if (!fw) { spi_bus_unlock(); return ESP_ERR_NOT_FOUND; }
    fread(ph->W, sizeof(float), 9 * ph->num_actions, fw);
    fclose(fw);

    FILE *fb = fopen(b_path, "rb");
    if (!fb) { spi_bus_unlock(); return ESP_ERR_NOT_FOUND; }
    fread(ph->b, sizeof(float), ph->num_actions, fb);
    fclose(fb);

    spi_bus_unlock();

    ESP_LOGI(TAG, "Loaded checkpoint epoch=%lu", (unsigned long)epoch);
    return ESP_OK;
}

bool ppo_find_latest_checkpoint(uint32_t *epoch_out)
{
    spi_bus_lock();

    DIR *dir = opendir(PPO_CHECKPOINT_DIR);
    if (!dir) { spi_bus_unlock(); return false; }

    struct dirent *ent;
    uint32_t max_epoch = 0;
    bool found = false;

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == 'c') {
            uint32_t e;
            if (sscanf(ent->d_name, "checkpoint_%lu.meta", (unsigned long *)&e) == 1) {
                if (e > max_epoch) {
                    max_epoch = e;
                    found = true;
                }
            }
        }
    }
    closedir(dir);

    spi_bus_unlock();

    if (found && epoch_out) {
        *epoch_out = max_epoch;
    }
    return found;
}
