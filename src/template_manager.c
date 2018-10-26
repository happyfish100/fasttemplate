
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include "fastcommon/logger.h"
#include "fastcommon/shared_func.h"
#include "template_manager.h"

int template_manager_init(TemplateManagerContext *context, void *args,
        fast_template_alloc_func alloc_func,
        fast_template_free_func free_func, const int init_capacity)
{
    int result;

    if ((result=hash_init(&context->template_htable, simple_hash,
                    init_capacity, 0.75)) != 0)
    {
        return result;
    }
    context->args = args;
    context->alloc_func = alloc_func;
    context->free_func = free_func;

    return 0;
}

void template_manager_destroy(TemplateManagerContext *context)
{
    hash_destroy(&context->template_htable);
}

int template_manager_render(TemplateManagerContext *context,
        const string_t *template_filename,
        void *params, const int total_value_len,
        fast_template_find_param_func find_func, string_t *output)
{
    int result;
    FastTemplateContext *template_context;

    template_context = (FastTemplateContext *)hash_find1(
            &context->template_htable, template_filename);
    if (template_context == NULL) {
        template_context = (FastTemplateContext *)malloc(
                sizeof(FastTemplateContext));
        if (template_context == NULL) {
            logError("file: "__FILE__", line: %d, "
                    "malloc %d bytes fail", __LINE__,
                    (int)sizeof(FastTemplateContext));
            return ENOMEM;
        }

        if ((result=fast_template_init(template_context,
                        template_filename->str, context->args,
                        context->alloc_func, context->free_func)) != 0)
        {
            return result;
        }

        if ((result=hash_insert_ex(&context->template_htable,
                    template_filename->str, template_filename->len,
                    template_context, 0, false)) < 0)
        {
            return -1 * result;
        }
    }

    return fast_template_render(template_context,
            params, total_value_len, find_func, output);
}
