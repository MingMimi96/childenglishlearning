/*
 * word_events — 学习事件总线（给游戏模块订阅）
 *
 * 基于 esp_event 默认事件循环。游戏侧订阅示例：
 *
 *   static void on_word_event(void *arg, esp_event_base_t base, int32_t id, void *data)
 *   {
 *       const word_event_data_t *e = data;
 *       if (id == WORD_EVENT_ADDED_TO_BOOK) { game_add_target_word(e->id); }
 *   }
 *   esp_event_handler_register(WORD_EVENT, ESP_EVENT_ANY_ID, on_word_event, NULL);
 *
 * 事件只携带主键与词形；完整数据通过 wordbook 查询。
 */
#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include "word_model.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(WORD_EVENT);

typedef enum {
    WORD_EVENT_VIEWED = 0,        /* 打开了单词详情页 */
    WORD_EVENT_LEARNED,           /* 完成跟读 / 练习，记为学会（onWordLearned） */
    WORD_EVENT_ADDED_TO_BOOK,     /* 加入单词本（onWordAddedToBook） */
    WORD_EVENT_REMOVED_FROM_BOOK,
    WORD_EVENT_EXERCISE_DONE,     /* 完成一次练习，score 有效 */
} word_event_id_t;

typedef struct {
    char    id[WORD_ID_MAX];
    char    word[WORD_TEXT_MAX];
    char    source[16];           /* "detail" / "exercise:<id>" / "game:<id>" 等 */
    int16_t score;                /* 0–100，仅 EXERCISE_DONE / LEARNED 有意义，否则 -1 */
} word_event_data_t;

/** 发布事件（数据会被 esp_event 复制，调用方无需保持） */
esp_err_t word_events_post(word_event_id_t id, const word_t *w, const char *source, int16_t score);

#ifdef __cplusplus
}
#endif
