#include "word_service.h"

#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "word_service";

extern const char words_json_start[] asm("_binary_words_json_start");

static cJSON   *s_dict;          /* mock 词典（words 数组） */
static uint32_t s_req;

esp_err_t word_service_init(void)
{
    if (s_dict) {
        return ESP_OK;
    }
    cJSON *root = cJSON_Parse(words_json_start);
    if (!root) {
        ESP_LOGE(TAG, "mock words.json parse failed");
        return ESP_FAIL;
    }
    s_dict = root;
    ESP_LOGI(TAG, "mock dictionary: %d words",
             cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(root, "words")));
    return ESP_OK;
}

bool word_service_mock_find(const char *word, word_t *out)
{
    if (!s_dict || !word) {
        return false;
    }
    char want[WORD_ID_MAX];
    word_make_id(word, want, sizeof(want));
    const cJSON *it;
    cJSON_ArrayForEach(it, cJSON_GetObjectItemCaseSensitive(s_dict, "words")) {
        const cJSON *w = cJSON_GetObjectItemCaseSensitive(it, "word");
        if (!cJSON_IsString(w)) {
            continue;
        }
        char id[WORD_ID_MAX];
        word_make_id(w->valuestring, id, sizeof(id));
        if (!strcmp(id, want)) {
            return word_from_cjson(it, out);
        }
    }
    return false;
}

typedef struct {
    char              word[WORD_TEXT_MAX];
    word_service_cb_t cb;
    void             *user;
    esp_timer_handle_t timer;
} lookup_job_t;

static void lookup_fire(void *arg)
{
    lookup_job_t *job = arg;
    word_t *w = heap_caps_malloc(sizeof(word_t), MALLOC_CAP_SPIRAM);
    bool found = w && word_service_mock_find(job->word, w);
    job->cb(found ? w : NULL, job->user);
    free(w);
    esp_timer_delete(job->timer);
    free(job);
}

uint32_t word_service_lookup(const char *word, word_service_cb_t cb, void *user)
{
    if (!word || !cb) {
        return 0;
    }
    lookup_job_t *job = calloc(1, sizeof(*job));
    if (!job) {
        return 0;
    }
    strlcpy(job->word, word, sizeof(job->word));
    job->cb = cb;
    job->user = user;
    esp_timer_create_args_t args = {
        .callback = lookup_fire,
        .arg = job,
        .name = "word_lookup",
    };
    if (esp_timer_create(&args, &job->timer) != ESP_OK) {
        free(job);
        return 0;
    }
    /* 异步语义：调用方先拿到请求号，再收到回调 */
    esp_timer_start_once(job->timer, 30 * 1000);
    return ++s_req;
}
