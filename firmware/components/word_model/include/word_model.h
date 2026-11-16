/*
 * word_model — 统一单词数据模型
 *
 * 识别结果、推荐词、单词本、游戏都使用 word_t。
 * JSON 结构见 shared/schema/word.schema.json 与 recognition.schema.json。
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WORD_TEXT_MAX      40   /* 英文词（可含空格） */
#define WORD_ID_MAX        40
#define WORD_ZH_MAX        48   /* UTF-8，约 15 个汉字 */
#define WORD_PHONETIC_MAX  48
#define WORD_SYLLABLE_MAX  16
#define WORD_SYLLABLES     6
#define WORD_EXAMPLE_MAX   160
#define WORD_EXAMPLES      3
#define WORD_RELATED       6
#define WORD_ICON_KEY_MAX  32

#define RECOG_MAX_OBJECTS  3
#define RECOG_MAX_ATTRS    3

typedef enum {
    WORD_POS_NOUN = 0,
    WORD_POS_ADJ,
    WORD_POS_VERB,
    WORD_POS_OTHER,
} word_pos_t;

typedef enum {
    WORD_ATTR_NONE = 0,
    WORD_ATTR_COLOR,
    WORD_ATTR_MATERIAL,
    WORD_ATTR_SHAPE,
    WORD_ATTR_SIZE,
    WORD_ATTR_OTHER,
} word_attr_type_t;

/* 轻量引用：推荐词只有词形 + 中文 */
typedef struct {
    char word[WORD_TEXT_MAX];
    char zh[WORD_ZH_MAX];
} word_ref_t;

typedef struct {
    char en[WORD_EXAMPLE_MAX];
    char zh[WORD_EXAMPLE_MAX];
} word_example_t;

typedef struct {
    char             id[WORD_ID_MAX];         /* 主键：小写、空格→'_'，如 "coffee_cup" */
    char             word[WORD_TEXT_MAX];
    char             zh[WORD_ZH_MAX];
    char             phonetic[WORD_PHONETIC_MAX];
    word_pos_t       pos;
    word_attr_type_t attr_type;
    bool             has_color;
    uint32_t         color_rgb;               /* 0xRRGGBB，仅 has_color 时有效 */
    char             syllables[WORD_SYLLABLES][WORD_SYLLABLE_MAX];
    uint8_t          syllable_cnt;
    word_example_t   examples[WORD_EXAMPLES];
    uint8_t          example_cnt;
    word_ref_t       related[WORD_RELATED];
    uint8_t          related_cnt;
    char             icon_key[WORD_ICON_KEY_MAX];
    uint8_t          detail_level;            /* 0 = 只有词形/中文，1 = 已补全音标与例句 */
} word_t;

typedef struct {
    float x, y, w, h;                         /* 归一化 0–1，原点左上 */
} word_bbox_t;

typedef struct {
    word_t      word;
    word_bbox_t bbox;
    word_t      attrs[RECOG_MAX_ATTRS];
    uint8_t     attr_cnt;
} recog_object_t;

typedef enum {
    RECOG_OK = 0,
    RECOG_BLURRY,     /* 看不清 */
    RECOG_NOTHING,    /* 没有可识别物体 */
    RECOG_UNSAFE,     /* 不适宜内容：只显示通用提示 */
    RECOG_ERROR,      /* 网络 / 服务错误 */
} recog_status_t;

typedef struct {
    recog_status_t status;
    recog_object_t objects[RECOG_MAX_OBJECTS];
    uint8_t        count;
} recog_result_t;

/** 由英文词生成主键（小写，空格/连字符→'_'） */
void word_make_id(const char *word, char *out, size_t out_len);

/** 用 word_ref 初始化一个 detail_level=0 的 word_t */
void word_from_ref(const word_ref_t *ref, word_t *out);

/** 解析单个 Word JSON 对象（cJSON*，以 void* 传入避免头文件依赖） */
bool word_from_cjson(const void *cjson_obj, word_t *out);

/** 解析 Word JSON 字符串 */
bool word_from_json(const char *json, word_t *out);

/** 解析 RecognitionResult JSON 字符串 */
bool recog_result_from_json(const char *json, recog_result_t *out);

/**
 * 合并补全数据：用 src 中更完整的字段填充 dst（例句、音节、相关词、音标等），
 * 不覆盖 dst 已有的颜色等识别信息。
 */
void word_merge_detail(word_t *dst, const word_t *src);

/** 例句中是否包含该词（不区分大小写），返回起始字节偏移，未找到返回 -1 */
int word_find_in_sentence(const char *sentence, const char *word, size_t *match_len);

const char *word_pos_to_str(word_pos_t pos);
const char *recog_status_to_str(recog_status_t st);

#ifdef __cplusplus
}
#endif
