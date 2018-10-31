//template_manager.h

#include <stdlib.h>
#include "fastcommon/common_define.h"
#include "fastcommon/hash.h"
#include "fast_template.h"

#ifndef _TEMPLATE_MANAGER_H
#define _TEMPLATE_MANAGER_H

typedef struct template_manager_context {
    /* key: template filename, value: FastTemplateContext * */
    HashArray template_htable;

    void *args;
    fast_template_alloc_func alloc_func;
    fast_template_free_func free_func;

} TemplateManagerContext;

#ifdef __cplusplus
extern "C" {
#endif

int template_manager_init(TemplateManagerContext *context, void *args,
        fast_template_alloc_func alloc_func,
        fast_template_free_func free_func, const int init_capacity);

void template_manager_destroy(TemplateManagerContext *context);

int template_manager_render(TemplateManagerContext *context,
        const string_t *template_filename,
        void *params, const int total_value_len, const bool text2html,
        fast_template_find_param_func find_func, string_t *output);

#ifdef __cplusplus
}
#endif

#endif
