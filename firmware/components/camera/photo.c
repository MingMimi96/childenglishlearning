#include "photo.h"

#include <math.h>
#include <string.h>

#include "driver/ppa.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_private/esp_cache_private.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "photo";

static ppa_client_handle_t s_ppa;
static SemaphoreHandle_t   s_ppa_mutex;
static size_t              s_align = 64;

#define ALIGN_UP(n, a) (((n) + ((a) - 1)) & ~((a) - 1))

esp_err_t photo_ppa_init(void)
{
    if (s_ppa) {
        return ESP_OK;
    }
    size_t a = 0;
    if (esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &a) == ESP_OK && a > 0) {
        s_align = a;
    }
    s_ppa_mutex = xSemaphoreCreateMutex();
    ppa_client_config_t cfg = {
        .oper_type = PPA_OPERATION_SRM,
    };
    esp_err_t err = ppa_register_client(&cfg, &s_ppa);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PPA register failed: %s", esp_err_to_name(err));
    }
    return err;
}

size_t photo_align_size(size_t size)
{
    return ALIGN_UP(size, s_align);
}

esp_err_t photo_alloc(photo_t *p, uint16_t w, uint16_t h)
{
    memset(p, 0, sizeof(*p));
    size_t size = photo_align_size((size_t)w * h * 2);
    p->buf = heap_caps_aligned_calloc(s_align, 1, size, MALLOC_CAP_SPIRAM);
    if (!p->buf) {
        return ESP_ERR_NO_MEM;
    }
    p->w = w;
    p->h = h;
    p->size = size;
    return ESP_OK;
}

void photo_free(photo_t *p)
{
    if (p && p->buf) {
        heap_caps_free(p->buf);
    }
    if (p) {
        memset(p, 0, sizeof(*p));
    }
}

float photo_fit_scale(uint16_t in_w, uint16_t in_h, uint16_t max_w, uint16_t max_h)
{
    float sx = (float)max_w / in_w;
    float sy = (float)max_h / in_h;
    float s = sx < sy ? sx : sy;
    /* PPA 缩放因子按 1/16 量化，向下取整保证不越界 */
    s = floorf(s * 16.f) / 16.f;
    if (s < 1.f / 16.f) {
        s = 1.f / 16.f;
    }
    return s;
}

esp_err_t photo_ppa_scale_raw(const void *in, uint16_t in_w, uint16_t in_h, int in_cm, bool byte_swap,
                              uint16_t bx, uint16_t by, uint16_t bw, uint16_t bh,
                              float scale, void *out, size_t out_size, uint16_t out_w, uint16_t out_h)
{
    if (!s_ppa) {
        return ESP_ERR_INVALID_STATE;
    }
    ppa_srm_oper_config_t cfg = {
        .in.buffer = in,
        .in.pic_w = in_w,
        .in.pic_h = in_h,
        .in.block_w = bw,
        .in.block_h = bh,
        .in.block_offset_x = bx,
        .in.block_offset_y = by,
        .in.srm_cm = (ppa_srm_color_mode_t)in_cm,
        .in.yuv_range = PPA_COLOR_RANGE_FULL,
        .in.yuv_std = PPA_COLOR_CONV_STD_RGB_YUV_BT601,
        .out.buffer = out,
        .out.buffer_size = out_size,
        .out.pic_w = out_w,
        .out.pic_h = out_h,
        .out.block_offset_x = 0,
        .out.block_offset_y = 0,
        .out.srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
        .scale_x = scale,
        .scale_y = scale,
        .rgb_swap = 0,
        .byte_swap = byte_swap,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };
    xSemaphoreTake(s_ppa_mutex, portMAX_DELAY);
    esp_err_t err = ppa_do_scale_rotate_mirror(s_ppa, &cfg);
    xSemaphoreGive(s_ppa_mutex);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PPA SRM failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t photo_crop_scale(const photo_t *src, uint16_t sx, uint16_t sy, uint16_t sw, uint16_t sh,
                           uint16_t max_w, uint16_t max_h, photo_t *out)
{
    if (!src || !src->buf || !out || sw == 0 || sh == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (sx + sw > src->w) {
        sw = src->w - sx;
    }
    if (sy + sh > src->h) {
        sh = src->h - sy;
    }
    float s = photo_fit_scale(sw, sh, max_w, max_h);
    uint16_t ow = (uint16_t)(sw * s);
    uint16_t oh = (uint16_t)(sh * s);
    if (ow == 0 || oh == 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    esp_err_t err = photo_alloc(out, ow, oh);
    if (err != ESP_OK) {
        return err;
    }
    err = photo_ppa_scale_raw(src->buf, src->w, src->h, PPA_SRM_COLOR_MODE_RGB565, false,
                              sx, sy, sw, sh, s, out->buf, out->size, ow, oh);
    if (err != ESP_OK) {
        photo_free(out);
    }
    return err;
}

esp_err_t photo_scale_fit(const photo_t *src, uint16_t max_w, uint16_t max_h, photo_t *out)
{
    return photo_crop_scale(src, 0, 0, src->w, src->h, max_w, max_h, out);
}
