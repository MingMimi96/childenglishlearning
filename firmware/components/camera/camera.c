/*
 * 摄像头：V4L2 采集 → PPA 缩放 → 预览回调 / 拍照回调
 * 参考 espressif/esp-bsp examples/display_camera_video（CC0）。
 */
#include "camera.h"

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "bsp/esp-bsp.h"
#include "esp_video_device.h"
#include "driver/ppa.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "linux/videodev2.h"

static const char *TAG = "camera";

#define CAM_BUF_COUNT   2
#define CAM_TASK_STACK  (4 * 1024)
#define CAM_TASK_PRIO   5

typedef struct {
    int       fd;
    bool      available;
    uint8_t  *cam_buf[CAM_BUF_COUNT];
    size_t    cam_buf_len;
    uint16_t  cam_w, cam_h;
    int       in_cm;
    bool      byte_swap;

    photo_t   preview[2];        /* 预览双缓冲 */
    uint8_t   preview_idx;
    uint16_t  preview_max_w, preview_max_h;

    photo_t   photo;             /* 最近一次拍照 */

    portMUX_TYPE        lock;
    camera_preview_cb_t preview_cb;
    void               *preview_user;
    camera_capture_cb_t capture_cb;
    void               *capture_user;
} camera_ctx_t;

static camera_ctx_t s_cam = {
    .fd = -1,
    .lock = portMUX_INITIALIZER_UNLOCKED,
};

bool camera_available(void)
{
    return s_cam.available;
}

const photo_t *camera_last_photo(void)
{
    return s_cam.photo.buf ? &s_cam.photo : NULL;
}

void camera_set_preview_cb(camera_preview_cb_t cb, void *user)
{
    portENTER_CRITICAL(&s_cam.lock);
    s_cam.preview_cb = cb;
    s_cam.preview_user = user;
    portEXIT_CRITICAL(&s_cam.lock);
}

/* 把相机原始帧缩放到 dst（等比适配 max_w×max_h），必要时重新分配 dst */
static esp_err_t scale_frame_into(const uint8_t *frame, photo_t *dst, uint16_t max_w, uint16_t max_h)
{
    float s = photo_fit_scale(s_cam.cam_w, s_cam.cam_h, max_w, max_h);
    uint16_t ow = (uint16_t)(s_cam.cam_w * s);
    uint16_t oh = (uint16_t)(s_cam.cam_h * s);
    if (!dst->buf || dst->w != ow || dst->h != oh) {
        photo_free(dst);
        esp_err_t err = photo_alloc(dst, ow, oh);
        if (err != ESP_OK) {
            return err;
        }
    }
    return photo_ppa_scale_raw(frame, s_cam.cam_w, s_cam.cam_h, s_cam.in_cm, s_cam.byte_swap,
                               0, 0, s_cam.cam_w, s_cam.cam_h, s, dst->buf, dst->size, ow, oh);
}

static void camera_task(void *arg)
{
    struct v4l2_buffer buf;
    while (1) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(s_cam.fd, VIDIOC_DQBUF, &buf) != 0) {
            ESP_LOGE(TAG, "DQBUF failed");
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        const uint8_t *frame = s_cam.cam_buf[buf.index];
        bool good = (buf.flags & V4L2_BUF_FLAG_ERROR) == 0;

        portENTER_CRITICAL(&s_cam.lock);
        camera_capture_cb_t cap_cb = s_cam.capture_cb;
        void *cap_user = s_cam.capture_user;
        camera_preview_cb_t pv_cb = s_cam.preview_cb;
        void *pv_user = s_cam.preview_user;
        portEXIT_CRITICAL(&s_cam.lock);

        if (good && cap_cb) {
            if (scale_frame_into(frame, &s_cam.photo, CAMERA_PHOTO_MAX_W, CAMERA_PHOTO_MAX_H) == ESP_OK) {
                portENTER_CRITICAL(&s_cam.lock);
                s_cam.capture_cb = NULL;
                portEXIT_CRITICAL(&s_cam.lock);
                cap_cb(&s_cam.photo, cap_user);
            }
        } else if (good && pv_cb) {
            photo_t *pv = &s_cam.preview[s_cam.preview_idx];
            if (scale_frame_into(frame, pv, s_cam.preview_max_w, s_cam.preview_max_h) == ESP_OK) {
                pv_cb(pv, pv_user);
                s_cam.preview_idx ^= 1;
            }
        }

        if (ioctl(s_cam.fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "QBUF failed");
        }
    }
}

/* 无摄像头时：生成一张柔和渐变占位图，方便在开发板缺摄像头时跑通流程 */
static void placeholder_task(void *arg)
{
    const uint16_t w = 960, h = 540;
    if (!s_cam.photo.buf && photo_alloc(&s_cam.photo, w, h) != ESP_OK) {
        vTaskDelete(NULL);
        return;
    }
    uint16_t *px = (uint16_t *)s_cam.photo.buf;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t r = 255 - (y * 40 / h);
            uint8_t g = 236 - (x * 30 / w);
            uint8_t b = 200 + (y * 40 / h);
            /* 中间画一个圆角"杯子"色块，让 mock 的 bbox 有东西可指 */
            int cx = x - 300, cy = y - 270;
            if (cx > -150 && cx < 150 && cy > -140 && cy < 160) {
                r = 60; g = 60; b = 70;
                if (cy < -100) {
                    r = 245; g = 245; b = 245;
                }
            }
            px[y * w + x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
    }
    portENTER_CRITICAL(&s_cam.lock);
    camera_capture_cb_t cb = s_cam.capture_cb;
    void *user = s_cam.capture_user;
    s_cam.capture_cb = NULL;
    portEXIT_CRITICAL(&s_cam.lock);
    if (cb) {
        cb(&s_cam.photo, user);
    }
    vTaskDelete(NULL);
}

esp_err_t camera_request_capture(camera_capture_cb_t cb, void *user)
{
    if (!cb) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&s_cam.lock);
    bool busy = s_cam.capture_cb != NULL;
    if (!busy) {
        s_cam.capture_cb = cb;
        s_cam.capture_user = user;
    }
    portEXIT_CRITICAL(&s_cam.lock);
    if (busy) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_cam.available) {
        xTaskCreate(placeholder_task, "cam_placeholder", 3072, NULL, CAM_TASK_PRIO, NULL);
    }
    return ESP_OK;
}

static esp_err_t open_video(void)
{
    s_cam.fd = open(BSP_CAMERA_DEVICE, O_RDONLY);
    if (s_cam.fd < 0) {
        ESP_LOGE(TAG, "open %s failed", BSP_CAMERA_DEVICE);
        return ESP_FAIL;
    }
    struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
    if (ioctl(s_cam.fd, VIDIOC_G_FMT, &fmt) != 0) {
        return ESP_FAIL;
    }
    /* PPA SRM 在 IDF 5.5 不支持 YUV422 输入，统一要 RGB565 */
    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565 && fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565X) {
        struct v4l2_format want = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .fmt.pix.width = fmt.fmt.pix.width,
            .fmt.pix.height = fmt.fmt.pix.height,
            .fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565,
        };
        if (ioctl(s_cam.fd, VIDIOC_S_FMT, &want) != 0) {
            ESP_LOGE(TAG, "set RGB565 failed");
            return ESP_FAIL;
        }
        if (ioctl(s_cam.fd, VIDIOC_G_FMT, &fmt) != 0) {
            return ESP_FAIL;
        }
    }
    s_cam.cam_w = fmt.fmt.pix.width;
    s_cam.cam_h = fmt.fmt.pix.height;
    s_cam.in_cm = PPA_SRM_COLOR_MODE_RGB565;
    s_cam.byte_swap = fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB565X;
    ESP_LOGI(TAG, "sensor %ux%u %s", s_cam.cam_w, s_cam.cam_h, s_cam.byte_swap ? "RGB565X" : "RGB565");

    struct v4l2_requestbuffers req = {
        .count = CAM_BUF_COUNT,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };
    if (ioctl(s_cam.fd, VIDIOC_REQBUFS, &req) != 0) {
        return ESP_FAIL;
    }
    for (int i = 0; i < CAM_BUF_COUNT; i++) {
        struct v4l2_buffer buf = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
            .index = i,
        };
        if (ioctl(s_cam.fd, VIDIOC_QUERYBUF, &buf) != 0) {
            return ESP_FAIL;
        }
        s_cam.cam_buf[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, s_cam.fd, buf.m.offset);
        if (!s_cam.cam_buf[i]) {
            return ESP_FAIL;
        }
        s_cam.cam_buf_len = buf.length;
        if (ioctl(s_cam.fd, VIDIOC_QBUF, &buf) != 0) {
            return ESP_FAIL;
        }
    }
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(s_cam.fd, VIDIOC_STREAMON, &type) != 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t camera_init(uint16_t preview_max_w, uint16_t preview_max_h)
{
    s_cam.preview_max_w = preview_max_w;
    s_cam.preview_max_h = preview_max_h;

    esp_err_t err = photo_ppa_init();
    if (err != ESP_OK) {
        return err;
    }
    err = bsp_camera_start(NULL);
    if (err == ESP_OK) {
        err = open_video();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "camera unavailable (%s) — capture will use a placeholder image", esp_err_to_name(err));
        if (s_cam.fd >= 0) {
            close(s_cam.fd);
            s_cam.fd = -1;
        }
        s_cam.available = false;
        return err;
    }
    s_cam.available = true;
    xTaskCreatePinnedToCore(camera_task, "camera", CAM_TASK_STACK, NULL, CAM_TASK_PRIO, NULL, 1);
    return ESP_OK;
}
