#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "fastcommon/logger.h"
#include "fast_template.h"

void * template_alloc_func(void *args, size_t size)
{
    return malloc(size);
}

void template_free_func(void *args, void *ptr)
{
    free(ptr);
}

int main(int argc, char *argv[])
{
    FastTemplateContext context;
    key_value_array_t kv_array;
    key_value_pair_t vars[16];
    const char *filename;
    string_t output;
    int result;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return EINVAL;
    }
    filename = argv[1];

    log_init();
    if ((result=fast_template_init(&context,
                    filename, NULL,
                    template_alloc_func,
                    template_free_func)) != 0)
    {
        return result;
    }

    FC_SET_STRING(vars[0].key, "question");
    FC_SET_STRING(vars[0].value, "this is a test.");
    FC_SET_STRING(vars[1].key, "server_ip");
    FC_SET_STRING(vars[1].value, "127.0.0.1");
    kv_array.kv_pairs = vars;
    kv_array.count = 2;

    if ((result=fast_template_render_by_karray(&context,
                    &kv_array, &output)) != 0)
    {
        printf("result: %d\n", result);
        return result;
    }

    printf("output: %.*s\n", output.len, output.str);
    free(output.str);
    fast_template_destroy(&context);
    return 0;
}
