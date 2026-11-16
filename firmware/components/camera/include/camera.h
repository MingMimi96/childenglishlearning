/*
 * camera — 摄像头预览与拍照（ESP32-P4 MIPI-CSI，经 esp_video / V4L2）
 *
 * 不依赖 LVGL：预览帧通过回调交给 UI，由 UI 负责加锁并刷到画布。
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "photo.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 拍照尺寸上限（足够识别，也适合屏幕展示） */
#define CAMERA_PHOTO_MAX_W 1024
#define CAMERA_PHOTO_MAX_H 600

/**
 * 预览帧回调：在摄像头任务中调用。frame 是 camera 内部双缓冲之一，
 * 在下一次回调前保持有效。
 */
typedef void (*camera_preview_cb_t)(const photo_t *frame, void *user);

/** 拍照回调：在摄像头任务中调用。photo 由 camera 持有，直到下一次拍照。 */
typedef void (*camera_capture_cb_t)(const photo_t *photo, void *user);

/** 初始化摄像头。失败时 camera_available() 为 false，拍照会生成占位图（便于无摄像头调试）。 */
esp_err_t camera_init(uint16_t preview_max_w, uint16_t preview_max_h);

bool camera_available(void);

/** 设置 / 清除预览回调（NULL 表示暂停推送预览帧） */
void camera_set_preview_cb(camera_preview_cb_t cb, void *user);

/** 请求拍一张照片（异步）。忙时返回 ESP_ERR_INVALID_STATE。 */
esp_err_t camera_request_capture(camera_capture_cb_t cb, void *user);

/** 最近一次拍到的照片（可能为空） */
const photo_t *camera_last_photo(void);

#ifdef __cplusplus
}
#endif
