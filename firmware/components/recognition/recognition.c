#include "recognition.h"

#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "recognition";

typedef struct {
    const photo_t        *photo;
    recognition_done_cb_t cb;
    void                 *user;
    uint32_t              seq;
} recog_job_t;

static QueueHandle_t                 s_queue;
static const recognition_provider_t *s_provider;
static volatile uint32_t             s_seq;        /* 当前有效请求号 */
static volatile bool                 s_busy;
static recog_result_t               *s_result;     /* PSRAM，约 20KB */

static void recog_task(void *arg)
{
    recog_job_t job;
    while (xQueueReceive(s_queue, &job, portMAX_DELAY) == pdTRUE) {
        memset(s_result, 0, sizeof(*s_result));
        esp_err_t err = s_provider->recognize(job.photo, s_result);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "provider %s failed: %s", s_provider->name, esp_err_to_name(err));
            s_result->status = RECOG_ERROR;
            s_result->count = 0;
        }
        ESP_LOGI(TAG, "result: %s, %u object(s)", recog_status_to_str(s_result->status), s_result->count);
        if (job.seq == s_seq && job.cb) {
            job.cb(s_result, job.user);
        } else {
            ESP_LOGI(TAG, "result dropped (cancelled)");
        }
        s_busy = false;
    }
}

esp_err_t recognition_init(void)
{
    if (s_queue) {
        return ESP_OK;
    }
#if CONFIG_RECOG_PROVIDER_MOCK
    s_provider = recognition_mock_provider();
#endif
    s_result = heap_caps_calloc(1, sizeof(recog_result_t), MALLOC_CAP_SPIRAM);
    s_queue = xQueueCreate(1, sizeof(recog_job_t));
    if (!s_result || !s_queue || !s_provider) {
        return ESP_ERR_NO_MEM;
    }
    BaseType_t ok = xTaskCreate(recog_task, "recognition", 8 * 1024, NULL, 4, NULL);
    ESP_LOGI(TAG, "provider: %s", s_provider->name);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t recognition_start(const photo_t *photo, recognition_done_cb_t cb, void *user)
{
    if (!s_queue) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_busy) {
        return ESP_ERR_INVALID_STATE;
    }
    s_busy = true;
    recog_job_t job = { .photo = photo, .cb = cb, .user = user, .seq = ++s_seq };
    if (xQueueSend(s_queue, &job, 0) != pdTRUE) {
        s_busy = false;
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

void recognition_cancel(void)
{
    s_seq++;
}
