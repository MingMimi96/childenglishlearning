/*
 * word_service — 单词补全查询（详情页用）
 *
 * 推荐词只有词形 + 中文，进入详情页时通过这里补全音标、例句、音节、相关词。
 * mock 模式查 shared/mock/words.json；第 3 步改为后端 GET /word/:w。
 */
#pragma once

#include "esp_err.h"
#include "word_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 查询回调：word 为 NULL 表示没查到；可能在任意任务中调用 */
typedef void (*word_service_cb_t)(const word_t *word, void *user);

esp_err_t word_service_init(void);

/** 异步查询；返回请求号（>0），用于在回调里核对 */
uint32_t word_service_lookup(const char *word, word_service_cb_t cb, void *user);

/** mock 词典同步查询（mock provider 内部也用） */
bool word_service_mock_find(const char *word, word_t *out);

#ifdef __cplusplus
}
#endif
