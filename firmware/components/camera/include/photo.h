/*
 * photo — RGB565 图片缓冲 + 硬件(PPA)缩放裁剪
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *buf;      /* RGB565 小端，cache 行对齐，位于 PSRAM */
    uint16_t w;
    uint16_t h;
    size_t   size;     /* 分配大小（≥ w*h*2，已按 cache 行对齐） */
} photo_t;

/** 分配一张 w×h 的空白图片 */
esp_err_t photo_alloc(photo_t *p, uint16_t w, uint16_t h);

void photo_free(photo_t *p);

/**
 * 从 src 中裁出 (sx,sy,sw,sh) 区域，等比缩放后放入不超过 max_w×max_h 的新图片。
 * 结果尺寸写入 out->w/out->h。使用 PPA 硬件。
 */
esp_err_t photo_crop_scale(const photo_t *src, uint16_t sx, uint16_t sy, uint16_t sw, uint16_t sh,
                           uint16_t max_w, uint16_t max_h, photo_t *out);

/** 等比缩放整张图片 */
esp_err_t photo_scale_fit(const photo_t *src, uint16_t max_w, uint16_t max_h, photo_t *out);

/* ---- 内部给 camera.c 用 ---- */
esp_err_t photo_ppa_init(void);
size_t    photo_align_size(size_t size);
esp_err_t photo_ppa_scale_raw(const void *in, uint16_t in_w, uint16_t in_h, int in_cm, bool byte_swap,
                              uint16_t bx, uint16_t by, uint16_t bw, uint16_t bh,
                              float scale, void *out, size_t out_size, uint16_t out_w, uint16_t out_h);
float     photo_fit_scale(uint16_t in_w, uint16_t in_h, uint16_t max_w, uint16_t max_h);

#ifdef __cplusplus
}
#endif
