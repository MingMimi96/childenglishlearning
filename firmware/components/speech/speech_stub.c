#include "speech.h"

#include "esp_log.h"

static const char *TAG = "speech";

void speech_init(void)
{
    ESP_LOGI(TAG, "speech stub (TTS arrives in step 4)");
}

void speech_say(const char *text, speech_lang_t lang, speech_speed_t speed,
                speech_done_cb_t done, void *user)
{
    ESP_LOGI(TAG, "say [%s%s]: %s", lang == SPEECH_LANG_EN ? "en" : "zh",
             speed == SPEECH_SLOW ? ",slow" : "", text ? text : "");
    if (done) {
        done(user);
    }
}

void speech_stop(void) {}

bool speech_is_playing(void)
{
    return false;
}
