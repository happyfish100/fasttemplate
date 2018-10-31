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

#define HASH_INSERT_STRING_KV(hash, key, value) \
    hash_insert_ex(hash, key, strlen(key), value, strlen(value), false)

int main(int argc, char *argv[])
{
    FastTemplateContext context;
    HashArray htable;
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
    if ((result=hash_init(&htable, simple_hash, 64, 1.0)) != 0) {
        return result;
    }

    if ((result=fast_template_init(&context,
                    filename, NULL,
                    template_alloc_func,
                    template_free_func, text2html)) != 0)
    {
        return result;
    }

    local_ip = (char *)get_first_local_ip();

    HASH_INSERT_STRING_KV(&htable, "question", "this is  a  <question>.");
    HASH_INSERT_STRING_KV(&htable, "server_ip", local_ip);
    HASH_INSERT_STRING_KV(&htable, "answer", "this is  an  <answer>.");

    if ((result=fast_template_render_by_htable(&context,
                    &htable, &output)) != 0)
    {
        printf("result: %d\n", result);
        return result;
    }

    printf("output: %.*s\n", output.len, output.str);
    free(output.str);
    fast_template_destroy(&context);
    hash_destroy(&htable);

    return 0;
}
