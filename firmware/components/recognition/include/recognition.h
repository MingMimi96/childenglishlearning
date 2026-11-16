/*
 * recognition — 识别服务
 *
 * 对 UI 暴露统一的异步接口；底层 provider 可以是本地 mock 或后端 HTTP。
 */
#pragma once

#include "esp_err.h"
#include "photo.h"
#include "word_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 识别完成回调：在工作任务中调用，result 仅在回调期间有效（需要请自行拷贝） */
typedef void (*recognition_done_cb_t)(const recog_result_t *result, void *user);

/** provider 接口：同步实现，在识别工作任务中被调用 */
typedef struct {
    const char *name;
    esp_err_t (*recognize)(const photo_t *photo, recog_result_t *out);
} recognition_provider_t;

esp_err_t recognition_init(void);

/**
 * 开始识别一张照片（异步）。同一时间只处理一张，忙时返回 ESP_ERR_INVALID_STATE。
 * photo 在回调前必须保持有效。
 */
esp_err_t recognition_start(const photo_t *photo, recognition_done_cb_t cb, void *user);

/** 取消：之后到达的结果会被丢弃（不会回调） */
void recognition_cancel(void);

/* provider 实现 */
const recognition_provider_t *recognition_mock_provider(void);

#ifdef __cplusplus
}
#endif
