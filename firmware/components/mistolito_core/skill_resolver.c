#include "skill_resolver.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "skill_resolver";

uint8_t skill_resolver_count_for_intent(const pet_t *pet, uint8_t intent)
{
    if (pet == NULL || intent >= 6) return 0;

    uint8_t count = 0;
    for (int i = 0; i < pet->skill_count; i++) {
        if (pet->skills[i].intent_type == intent) {
            count++;
        }
    }
    return count;
}

bool skill_resolver_has_skill_for_intent(const pet_t *pet, uint8_t intent)
{
    return skill_resolver_count_for_intent(pet, intent) > 0;
}

uint8_t skill_resolver_resolve(const pet_t *pet, uint8_t intent)
{
    if (pet == NULL || intent >= 6) {
        ESP_LOGE(TAG, "resolve: invalid args pet=%p intent=%d", (void *)pet, intent);
        return 0xFF;
    }

    uint8_t candidates[MAX_SKILLS];
    uint8_t count = 0;

    for (int i = 0; i < pet->skill_count; i++) {
        if (pet->skills[i].intent_type == intent) {
            candidates[count++] = pet->skills[i].skill_id;
        }
    }

    if (count == 0) {
        return 0xFF;
    }

    return candidates[esp_random() % count];
}
