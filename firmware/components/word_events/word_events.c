#include "word_events.h"

#include <string.h>
#include "esp_log.h"

ESP_EVENT_DEFINE_BASE(WORD_EVENT);

static const char *TAG = "word_events";

esp_err_t word_events_post(word_event_id_t id, const word_t *w, const char *source, int16_t score)
{
    if (!w) {
        return ESP_ERR_INVALID_ARG;
    }
    word_event_data_t e = { .score = score };
    strlcpy(e.id, w->id, sizeof(e.id));
    strlcpy(e.word, w->word, sizeof(e.word));
    strlcpy(e.source, source ? source : "", sizeof(e.source));
    esp_err_t err = esp_event_post(WORD_EVENT, id, &e, sizeof(e), 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "post %d for '%s' failed: %s", (int)id, e.word, esp_err_to_name(err));
    }
    return err;
}
