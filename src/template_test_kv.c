#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "fastcommon/logger.h"
#include "fastcommon/ini_file_reader.h"
#include "fastcommon/local_ip_func.h"
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
    bool text2html;
    char *local_ip;
    string_t output;
    int result;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename> [text2html]\n"
                "\t text2html: 1 or true for text to html by default\n\n",
                argv[0]);
        return EINVAL;
    }
    filename = argv[1];
    if (argc > 2) {
        text2html = FAST_INI_STRING_IS_TRUE(argv[2]);
    } else {
        text2html = true;
    }

    log_init();
    if ((result=fast_template_init(&context,
                    filename, NULL,
                    template_alloc_func,
                    template_free_func, text2html, true)) != 0)
    {
        return result;
    }

    printf("template_file_modified: %d, template files: %d\n",
            fast_template_file_modified(&context),
            context.fileinfo_array.count);

    local_ip = (char *)get_first_local_ip();
    FC_SET_STRING(vars[0].key, "question");
    FC_SET_STRING(vars[0].value, "this is  a  <question>.");
    FC_SET_STRING(vars[1].key, "answer");
    FC_SET_STRING(vars[1].value, "this is  an  <answer>.");
    FC_SET_STRING(vars[2].key, "server_ip");
    FC_SET_STRING(vars[2].value, local_ip);
    kv_array.kv_pairs = vars;
    kv_array.count = 3;

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
