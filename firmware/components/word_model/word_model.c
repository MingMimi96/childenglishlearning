#include "word_model.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

/* UTF-8 安全拷贝：截断时不切断多字节字符 */
static void utf8_copy(char *dst, size_t dst_len, const char *src)
{
    if (!dst || dst_len == 0) {
        return;
    }
    dst[0] = '\0';
    if (!src) {
        return;
    }
    size_t n = strlen(src);
    if (n >= dst_len) {
        n = dst_len - 1;
        /* 回退到字符边界：continuation byte 形如 10xxxxxx */
        while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80) {
            n--;
        }
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static const char *jstr(const cJSON *obj, const char *key)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(it) ? it->valuestring : NULL;
}

static float jnum(const cJSON *obj, const char *key, float def)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(it) ? (float)it->valuedouble : def;
}

static float clamp01(float v)
{
    return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

void word_make_id(const char *word, char *out, size_t out_len)
{
    size_t j = 0;
    if (!out || out_len == 0) {
        return;
    }
    for (size_t i = 0; word && word[i] && j + 1 < out_len; i++) {
        unsigned char c = (unsigned char)word[i];
        if (c == ' ' || c == '-') {
            out[j++] = '_';
        } else {
            out[j++] = (char)tolower(c);
        }
    }
    out[j] = '\0';
}

void word_from_ref(const word_ref_t *ref, word_t *out)
{
    memset(out, 0, sizeof(*out));
    utf8_copy(out->word, sizeof(out->word), ref->word);
    utf8_copy(out->zh, sizeof(out->zh), ref->zh);
    word_make_id(out->word, out->id, sizeof(out->id));
    out->pos = WORD_POS_OTHER;
}

static word_pos_t parse_pos(const char *s)
{
    if (!s) {
        return WORD_POS_OTHER;
    }
    if (!strcmp(s, "noun")) {
        return WORD_POS_NOUN;
    }
    if (!strcmp(s, "adjective") || !strcmp(s, "adj")) {
        return WORD_POS_ADJ;
    }
    if (!strcmp(s, "verb")) {
        return WORD_POS_VERB;
    }
    return WORD_POS_OTHER;
}

static word_attr_type_t parse_attr_type(const char *s)
{
    if (!s) {
        return WORD_ATTR_NONE;
    }
    if (!strcmp(s, "color")) {
        return WORD_ATTR_COLOR;
    }
    if (!strcmp(s, "material")) {
        return WORD_ATTR_MATERIAL;
    }
    if (!strcmp(s, "shape")) {
        return WORD_ATTR_SHAPE;
    }
    if (!strcmp(s, "size")) {
        return WORD_ATTR_SIZE;
    }
    if (!strcmp(s, "none")) {
        return WORD_ATTR_NONE;
    }
    return WORD_ATTR_OTHER;
}

static bool parse_hex_color(const char *s, uint32_t *out)
{
    if (!s || s[0] != '#' || strlen(s) != 7) {
        return false;
    }
    char *end = NULL;
    unsigned long v = strtoul(s + 1, &end, 16);
    if (!end || *end != '\0') {
        return false;
    }
    *out = (uint32_t)v;
    return true;
}

static void parse_example(const cJSON *ex, word_t *out)
{
    if (!cJSON_IsObject(ex) || out->example_cnt >= WORD_EXAMPLES) {
        return;
    }
    const char *en = jstr(ex, "en");
    if (!en || !en[0]) {
        return;
    }
    word_example_t *dst = &out->examples[out->example_cnt++];
    utf8_copy(dst->en, sizeof(dst->en), en);
    utf8_copy(dst->zh, sizeof(dst->zh), jstr(ex, "zh"));
}

bool word_from_cjson(const void *cjson_obj, word_t *out)
{
    const cJSON *obj = (const cJSON *)cjson_obj;
    if (!cJSON_IsObject(obj) || !out) {
        return false;
    }
    const char *w = jstr(obj, "word");
    if (!w || !w[0]) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    utf8_copy(out->word, sizeof(out->word), w);
    word_make_id(out->word, out->id, sizeof(out->id));
    utf8_copy(out->zh, sizeof(out->zh), jstr(obj, "zh"));
    utf8_copy(out->phonetic, sizeof(out->phonetic), jstr(obj, "phonetic"));
    utf8_copy(out->icon_key, sizeof(out->icon_key), jstr(obj, "iconKey"));

    out->attr_type = parse_attr_type(jstr(obj, "type"));
    const char *pos = jstr(obj, "partOfSpeech");
    if (pos) {
        out->pos = parse_pos(pos);
    } else {
        /* 属性词默认形容词，其余默认名词 */
        out->pos = out->attr_type != WORD_ATTR_NONE ? WORD_POS_ADJ : WORD_POS_NOUN;
    }
    out->has_color = parse_hex_color(jstr(obj, "colorHex"), &out->color_rgb);

    const cJSON *syl = cJSON_GetObjectItemCaseSensitive(obj, "syllables");
    const cJSON *it;
    cJSON_ArrayForEach(it, syl) {
        if (out->syllable_cnt >= WORD_SYLLABLES) {
            break;
        }
        if (cJSON_IsString(it) && it->valuestring[0]) {
            utf8_copy(out->syllables[out->syllable_cnt++], WORD_SYLLABLE_MAX, it->valuestring);
        }
    }

    parse_example(cJSON_GetObjectItemCaseSensitive(obj, "example"), out);
    cJSON_ArrayForEach(it, cJSON_GetObjectItemCaseSensitive(obj, "examples")) {
        /* 跳过与单个 example 重复的句子 */
        const char *en = jstr(it, "en");
        bool dup = false;
        for (int i = 0; en && i < out->example_cnt; i++) {
            if (!strcmp(out->examples[i].en, en)) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            parse_example(it, out);
        }
    }

    cJSON_ArrayForEach(it, cJSON_GetObjectItemCaseSensitive(obj, "related")) {
        if (out->related_cnt >= WORD_RELATED) {
            break;
        }
        const char *rw = jstr(it, "word");
        if (rw && rw[0]) {
            word_ref_t *r = &out->related[out->related_cnt++];
            utf8_copy(r->word, sizeof(r->word), rw);
            utf8_copy(r->zh, sizeof(r->zh), jstr(it, "zh"));
        }
    }

    out->detail_level = (out->phonetic[0] && out->example_cnt > 0) ? 1 : 0;
    return true;
}

bool word_from_json(const char *json, word_t *out)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return false;
    }
    bool ok = word_from_cjson(root, out);
    cJSON_Delete(root);
    return ok;
}

static recog_status_t parse_status(const char *s)
{
    if (!s) {
        return RECOG_ERROR;
    }
    if (!strcmp(s, "ok")) {
        return RECOG_OK;
    }
    if (!strcmp(s, "blurry")) {
        return RECOG_BLURRY;
    }
    if (!strcmp(s, "nothing")) {
        return RECOG_NOTHING;
    }
    if (!strcmp(s, "unsafe")) {
        return RECOG_UNSAFE;
    }
    return RECOG_ERROR;
}

bool recog_result_from_json(const char *json, recog_result_t *out)
{
    memset(out, 0, sizeof(*out));
    out->status = RECOG_ERROR;

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return false;
    }
    out->status = parse_status(jstr(root, "status"));

    const cJSON *o;
    cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(root, "objects")) {
        if (out->count >= RECOG_MAX_OBJECTS) {
            break;
        }
        recog_object_t *dst = &out->objects[out->count];
        memset(dst, 0, sizeof(*dst));
        if (!word_from_cjson(o, &dst->word)) {
            continue;
        }
        const cJSON *bb = cJSON_GetObjectItemCaseSensitive(o, "bbox");
        dst->bbox.x = clamp01(jnum(bb, "x", 0.25f));
        dst->bbox.y = clamp01(jnum(bb, "y", 0.25f));
        dst->bbox.w = clamp01(jnum(bb, "w", 0.5f));
        dst->bbox.h = clamp01(jnum(bb, "h", 0.5f));

        const cJSON *a;
        cJSON_ArrayForEach(a, cJSON_GetObjectItemCaseSensitive(o, "attributes")) {
            if (dst->attr_cnt >= RECOG_MAX_ATTRS) {
                break;
            }
            if (word_from_cjson(a, &dst->attrs[dst->attr_cnt])) {
                if (dst->attrs[dst->attr_cnt].attr_type == WORD_ATTR_NONE) {
                    dst->attrs[dst->attr_cnt].attr_type = WORD_ATTR_OTHER;
                }
                dst->attr_cnt++;
            }
        }
        out->count++;
    }
    cJSON_Delete(root);

    /* status=ok 但没有物体，按"没有可识别物体"处理 */
    if (out->status == RECOG_OK && out->count == 0) {
        out->status = RECOG_NOTHING;
    }
    return true;
}

void word_merge_detail(word_t *dst, const word_t *src)
{
    if (!dst || !src) {
        return;
    }
    if (!dst->zh[0]) {
        memcpy(dst->zh, src->zh, sizeof(dst->zh));
    }
    if (!dst->phonetic[0]) {
        memcpy(dst->phonetic, src->phonetic, sizeof(dst->phonetic));
    }
    if (dst->pos == WORD_POS_OTHER) {
        dst->pos = src->pos;
    }
    if (dst->attr_type == WORD_ATTR_NONE) {
        dst->attr_type = src->attr_type;
    }
    if (!dst->has_color && src->has_color) {
        dst->has_color = true;
        dst->color_rgb = src->color_rgb;
    }
    if (src->syllable_cnt > dst->syllable_cnt) {
        memcpy(dst->syllables, src->syllables, sizeof(dst->syllables));
        dst->syllable_cnt = src->syllable_cnt;
    }
    /* 例句：保留已有的，追加 src 中不重复的 */
    for (int i = 0; i < src->example_cnt && dst->example_cnt < WORD_EXAMPLES; i++) {
        bool dup = false;
        for (int j = 0; j < dst->example_cnt; j++) {
            if (!strcmp(dst->examples[j].en, src->examples[i].en)) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            dst->examples[dst->example_cnt++] = src->examples[i];
        }
    }
    if (src->related_cnt > dst->related_cnt) {
        memcpy(dst->related, src->related, sizeof(dst->related));
        dst->related_cnt = src->related_cnt;
    }
    if (!dst->icon_key[0]) {
        memcpy(dst->icon_key, src->icon_key, sizeof(dst->icon_key));
    }
    dst->detail_level = (dst->phonetic[0] && dst->example_cnt > 0) ? 1 : 0;
}

int word_find_in_sentence(const char *sentence, const char *word, size_t *match_len)
{
    if (!sentence || !word || !word[0]) {
        return -1;
    }
    size_t wl = strlen(word);
    for (size_t i = 0; sentence[i]; i++) {
        if (strncasecmp(sentence + i, word, wl) != 0) {
            continue;
        }
        /* 词边界：前一个字符不是字母 */
        if (i > 0 && isalpha((unsigned char)sentence[i - 1])) {
            continue;
        }
        size_t len = wl;
        /* 允许简单复数 / 第三人称：cup → cups */
        if (sentence[i + len] == 's' || sentence[i + len] == 'S') {
            len++;
        }
        if (isalpha((unsigned char)sentence[i + len])) {
            continue;
        }
        if (match_len) {
            *match_len = len;
        }
        return (int)i;
    }
    return -1;
}

const char *word_pos_to_str(word_pos_t pos)
{
    switch (pos) {
    case WORD_POS_NOUN: return "noun";
    case WORD_POS_ADJ:  return "adjective";
    case WORD_POS_VERB: return "verb";
    default:            return "other";
    }
}

const char *recog_status_to_str(recog_status_t st)
{
    switch (st) {
    case RECOG_OK:      return "ok";
    case RECOG_BLURRY:  return "blurry";
    case RECOG_NOTHING: return "nothing";
    case RECOG_UNSAFE:  return "unsafe";
    default:            return "error";
    }
}
