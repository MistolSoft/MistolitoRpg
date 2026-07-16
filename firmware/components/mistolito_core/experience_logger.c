#include "experience_logger.h"
#include "storage_task.h"
#include "ppo_trainer.h"
#include "spi_bus.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "EPISODE";

static replay_transition_t *g_episode_buffer = NULL;
static uint8_t g_episode_turn_count = 0;
static bool g_episode_active = false;
static uint32_t g_total_episodes = 0;

void experience_logger_start_episode(void)
{
    if (g_episode_buffer == NULL) {
        g_episode_buffer = heap_caps_malloc(EPISODE_MAX_TURNS * sizeof(replay_transition_t), MALLOC_CAP_SPIRAM);
        if (g_episode_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate episode buffer in PSRAM");
            return;
        }
    }
    g_episode_turn_count = 0;
    g_episode_active = true;
    ESP_LOGI(TAG, "Episode started");
}

void experience_logger_log_step(float *state, uint8_t action, float reward, float *next_state, uint8_t done, float old_prob)
{
    if (!g_episode_active) return;
    if (g_episode_turn_count >= EPISODE_MAX_TURNS) {
        ESP_LOGW(TAG, "Episode buffer full, discarding step");
        return;
    }

    replay_transition_t *tr = &g_episode_buffer[g_episode_turn_count];
    memcpy(tr->state, state, sizeof(float) * PPO_STATE_SIZE);
    tr->action = action;
    tr->reward = reward;
    memcpy(tr->next_state, next_state, sizeof(float) * PPO_STATE_SIZE);
    tr->done = done;
    tr->old_prob = old_prob;

    g_episode_turn_count++;
}

void experience_logger_end_episode(bool victory, uint8_t enemy_level)
{
    if (!g_episode_active) return;
    g_episode_active = false;

    if (g_episode_turn_count == 0) {
        ESP_LOGW(TAG, "Episode empty, skipping save");
        return;
    }

    float total_reward = 0.0f;
    for (uint8_t i = 0; i < g_episode_turn_count; i++) {
        total_reward += g_episode_buffer[i].reward;
    }

    g_total_episodes++;

    char ep_dir[64];
    storage_get_episodes_dir(ep_dir, sizeof(ep_dir));
    char path[128];
    snprintf(path, sizeof(path), "%s/ep_%05lu.bin", ep_dir, (unsigned long)g_total_episodes);

    spi_bus_lock();

    FILE *f = fopen(path, "wb");
    if (f) {
        uint8_t header[4];
        header[0] = g_episode_turn_count;
        header[1] = victory ? 1 : 0;
        header[2] = enemy_level;
        header[3] = 0;
        fwrite(header, 1, 4, f);

        fwrite(g_episode_buffer, sizeof(replay_transition_t), g_episode_turn_count, f);
        fclose(f);

        ESP_LOGI(TAG, "Episode %lu saved: %d turns, reward=%.2f, %s, enemy Lv.%d",
                 (unsigned long)g_total_episodes, g_episode_turn_count, total_reward,
                 victory ? "VICTORY" : "DEFEAT", enemy_level);
    } else {
        ESP_LOGE(TAG, "Failed to save episode: %s", path);
    }

    spi_bus_unlock();

    heap_caps_free(g_episode_buffer);
    g_episode_buffer = NULL;
}

uint32_t experience_logger_get_total_episodes(void)
{
    return g_total_episodes;
}
