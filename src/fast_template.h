//fast_template.h

#include <stdlib.h>
#include "fastcommon/common_define.h"
#include "fastcommon/hash.h"

#ifndef _FAST_TEMPLATE_H
#define _FAST_TEMPLATE_H

#define FAST_TEMPLATE_NODE_TYPE_STRING    1
#define FAST_TEMPLATE_NODE_TYPE_VARIABLE  2

typedef void * (*fast_template_alloc_func)(void *args, size_t size);
typedef void (*fast_template_free_func)(void *args, void *ptr);

typedef int (*fast_template_find_param_func)(void *args,
        const string_t *key, string_t *value);

typedef struct template_node {
    int type;
    string_t value;
} TemplateNode;

typedef struct template_node_array {
    TemplateNode *nodes;
    int count;
    int alloc;
} TemplateNodeArray;

typedef struct fast_template_context {
    char *filename;
    string_t file_content;
    TemplateNodeArray node_array;
    void *args;
    fast_template_alloc_func alloc_func;
    fast_template_free_func free_func;
} FastTemplateContext;


#ifdef __cplusplus
extern "C" {
#endif

extern string_t fast_template_empty_string;

int fast_template_init(FastTemplateContext *context,
        const char *filename, void *args,
        fast_template_alloc_func alloc_func,
        fast_template_free_func free_func);

void fast_template_destroy(FastTemplateContext *context);

int fast_template_render(FastTemplateContext *context,
        void *params, const int total_value_len, const bool text2html,
        fast_template_find_param_func find_func, string_t *output);

int find_value_from_kv_array(const key_value_array_t *params,
        const string_t *key, string_t *value);

static inline int fast_template_render_by_karray(FastTemplateContext *context,
        key_value_array_t *params, string_t *output)
{
    const key_value_pair_t *kv;
    const key_value_pair_t *kv_end;
    int total_value_len;

    total_value_len = 0;
    kv_end = params->kv_pairs + params->count;
    for (kv=params->kv_pairs; kv<kv_end; kv++) {
        total_value_len += 2 * kv->value.len;
    }

    return fast_template_render(context, params, total_value_len, true,
            (fast_template_find_param_func)find_value_from_kv_array, output);
}

static inline int fast_template_render_by_htable(FastTemplateContext *context,
        HashArray *params, string_t *output)
{
    return fast_template_render(context, params, params->item_count * 16,
            true, (fast_template_find_param_func)hash_find2, output);
}

static inline void fast_template_set_args(FastTemplateContext *context,
        void *args)
{
    context->args = args;
}

#ifdef __cplusplus
}
#endif

#endif
