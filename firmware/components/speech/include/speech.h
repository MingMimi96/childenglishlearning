/*
 * speech — 发音接口
 *
 * 第 2 步：仅打印日志（stub）。
 * 第 4 步：后端调用阿里云 TTS 生成音频，设备缓存到 SD 卡并经 ES8311 播放。
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SPEECH_NORMAL = 0,
    SPEECH_SLOW,
} speech_speed_t;

typedef enum {
    SPEECH_LANG_EN = 0,
    SPEECH_LANG_ZH,
} speech_lang_t;

/** 播放完成回调（可能在非 UI 线程调用） */
typedef void (*speech_done_cb_t)(void *user);

void speech_init(void);

/** 朗读一段文本；打断正在播放的内容 */
void speech_say(const char *text, speech_lang_t lang, speech_speed_t speed,
                speech_done_cb_t done, void *user);

void speech_stop(void);

bool speech_is_playing(void);

#ifdef __cplusplus
}
#endif
