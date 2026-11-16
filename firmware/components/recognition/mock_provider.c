/*
 * 本地 mock provider：延时后返回 shared/mock/recognition_cup.json
 */
#include "recognition.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#if CONFIG_RECOG_PROVIDER_MOCK

extern const char recognition_cup_json_start[] asm("_binary_recognition_cup_json_start");

static esp_err_t mock_recognize(const photo_t *photo, recog_result_t *out)
{
    vTaskDelay(pdMS_TO_TICKS(CONFIG_RECOG_MOCK_DELAY_MS));

#if CONFIG_RECOG_MOCK_CYCLE_STATES
    static const recog_status_t cycle[] = { RECOG_OK, RECOG_BLURRY, RECOG_NOTHING, RECOG_UNSAFE, RECOG_ERROR };
    static unsigned n;
    recog_status_t st = cycle[n++ % (sizeof(cycle) / sizeof(cycle[0]))];
    if (st == RECOG_ERROR) {
        return ESP_ERR_TIMEOUT;          /* 模拟网络失败 */
    }
    if (st != RECOG_OK) {
        out->status = st;
        out->count = 0;
        return ESP_OK;
    }
#endif
    if (!recog_result_from_json(recognition_cup_json_start, out)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static const recognition_provider_t s_mock = {
    .name = "mock",
    .recognize = mock_recognize,
};

const recognition_provider_t *recognition_mock_provider(void)
{
    return &s_mock;
}

#endif
