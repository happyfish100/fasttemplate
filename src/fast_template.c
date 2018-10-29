
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include "fastcommon/logger.h"
#include "fastcommon/shared_func.h"
#include "fastcommon/fast_buffer.h"
#include "fast_template.h"

string_t fast_template_empty_string = {NULL, 0};

static int check_alloc_index_node_array(FastTemplateContext *context)
{
    int bytes;

    if (context->node_array.alloc > context->node_array.count) {
        return 0;
    }

    if (context->node_array.alloc == 0) {
        context->node_array.alloc = 2;
    } else {
        context->node_array.alloc *= 2;
    }

    bytes = sizeof(TemplateNode) * context->node_array.alloc;
    context->node_array.nodes = (TemplateNode *)realloc(
            context->node_array.nodes, bytes);
    if (context->node_array.nodes == NULL) {
        logError("file: "__FILE__", line: %d, "
                "realloc %d bytes fail", __LINE__, bytes);
        return ENOMEM;
    }

    return 0;
}


static int add_index_template_node(FastTemplateContext *context,
        const int type, string_t *value)
{
    int result;

    if (value->len == 0) {
        return 0;
    }

    if ((result=check_alloc_index_node_array(context)) != 0) {
        return result;
    }

    context->node_array.nodes[context->node_array.count].type = type;
    context->node_array.nodes[context->node_array.count].value = *value;
    context->node_array.count++;
    return 0;
}

static inline int add_index_template_node_ex(FastTemplateContext *context,
        const int type, char *str, const int len)
{
    string_t value;
    FC_SET_STRING_EX(value, str, len);
    return add_index_template_node(context, type, &value);
}

static bool is_variable(const string_t *value)
{
#define MAX_VARIABLE_LENGTH   64
    const char *p;
    const char *end;

    if (value->len == 0 || value->len > MAX_VARIABLE_LENGTH) {
        return false;
    }

    if (!(FC_IS_LETTER(*(value->str)) || *(value->str) == '_')) {
        return false;
    }

    end = value->str + value->len;
    for (p = value->str; p < end; p++) {
        if (!(FC_IS_LETTER(*p) || *p == '_' || FC_IS_DIGITAL(*p))) {
            return false;
        }
    }
    return true;
}

static int fast_template_include_file(FastTemplateContext *context,
        char *file_part, char *end, FastBuffer *buffer)
{
    int result;
    char *p;
    char *l;
    string_t filename;
    char buff[MAX_PATH_SIZE];
    char full_filename[MAX_PATH_SIZE];

    p = file_part;
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        p++;
    }

    l = end - 1;
    while (l >= file_part && (*l == ' ' || *l == '\t' || *l == '\r' || *l == '\n')) {
        l--;
    }

    if (*p == '\'' || *p == '"') {
        if (l == p) {
            logWarning("file: "__FILE__", line: %d, "
                    "expect closed quote char %c",
                    __LINE__, *p);
            return 0;
        }
        if (*l != *p) {
            logWarning("file: "__FILE__", line: %d, "
                    "expect closed quote char %c, but %c ocurs",
                    __LINE__, *p, *l);
            return 0;
        }
        filename.str = p + 1;
        filename.len = l - filename.str;
    } else {
        filename.str = p;
        filename.len = (l + 1) - filename.str;
    }

    if (filename.len == 0) {
        logWarning("file: "__FILE__", line: %d, "
                "filename is empty!", __LINE__);
        return 0;
    }

    snprintf(buff, sizeof(buff), "%.*s", filename.len, filename.str);
    resolve_path(context->filename, buff, full_filename, sizeof(full_filename));
    logInfo("full_filename: %s", full_filename);

    if ((result=fast_buffer_append_file(buffer, full_filename)) != 0) {
        if (result == ENOENT) {   //ignore if file not exist
            return 0;
        } else {
            return -1 * result;
        }
    }

    return 1;  //include file done
}

static int do_preprocess(FastTemplateContext *context, int *include_count)
{
#define INCLUDE_MARK_START_STR "@include("
#define INCLUDE_MARK_START_LEN (sizeof(INCLUDE_MARK_START_STR) - 1)

    FastBuffer buffer;
    char *inc_start;
    char *inc_end;
    char *text_start;
    char *filename;
    char *p;
    int result;

    *include_count = 0;
    if ((inc_start=strstr(context->file_content.str,
                    INCLUDE_MARK_START_STR)) == NULL)
    {
        return 0;
    }

    if ((result=fast_buffer_init_ex(&buffer, context->file_content.len)) != 0) {
        return result;
    }

    text_start = context->file_content.str;
    while (inc_start != NULL) {
        filename = inc_start + INCLUDE_MARK_START_LEN;
        if ((inc_end=strchr(filename, ')')) == NULL) {
            break;
        }

        if ((result=fast_buffer_append_buff(&buffer, text_start,
                        inc_start - text_start)) != 0)
        {
            break;
        }

        result = fast_template_include_file(context,
                filename, inc_end, &buffer);
        if (result < 0) {   //error
            result *= -1;
            break;
        } else if (result > 0) {  //success include
            text_start = inc_end + 1;
            result = 0;
            (*include_count)++;
        } else {  //do NOT include, keep it
            text_start = inc_start;
        }

        p = inc_end + 1;
        inc_start = strstr(p, INCLUDE_MARK_START_STR);
    }

    if (result == 0) {
        if ((result=fast_buffer_append_buff(&buffer, text_start,
                        (context->file_content.str + context->file_content.len)
                        - text_start)) == 0)
        {
            free(context->file_content.str);
            context->file_content.str = fc_strdup(buffer.data, buffer.length);
            context->file_content.len = buffer.length;
        }
    }
    fast_buffer_destroy(&buffer);

    return result;
}

static int fast_template_preprocess(FastTemplateContext *context)
{
    int result;
    int include_count;

    do {
        if ((result=do_preprocess(context, &include_count)) != 0) {
            return result;
        }
    } while (include_count > 0);

    return 0;
}

static int fast_template_parse(FastTemplateContext *context)
{
#define VARIABLE_MARK_START_STR "${"
#define VARIABLE_MARK_START_LEN (sizeof(VARIABLE_MARK_START_STR) - 1)

    int result;
    char *p;
    char *end;
    char *var_start;
    char *var_end;
    char *text_start;
    string_t var;

    result = 0;
    text_start = p = context->file_content.str;
    end = context->file_content.str + context->file_content.len;
    while (p < end) {
        var_start = strstr(p, VARIABLE_MARK_START_STR);
        if (var_start == NULL) {
            break;
        }

        var.str = var_start + VARIABLE_MARK_START_LEN;
        var_end = strchr(var.str, '}');
        if (var_end == NULL) {
            break;
        }

        var.len = var_end - var.str;
        if (!is_variable(&var)) {
            p = var.str;
            continue;
        }

        if ((result=add_index_template_node_ex(context,
                        FAST_TEMPLATE_NODE_TYPE_STRING,
                        text_start, var_start - text_start)) != 0)
        {
            return result;
        }
        if ((result=add_index_template_node(context,
                        FAST_TEMPLATE_NODE_TYPE_VARIABLE, &var)) != 0)
        {
            return result;
        }

        text_start = p = var_end + 1;
    }

    return add_index_template_node_ex(context, FAST_TEMPLATE_NODE_TYPE_STRING,
            text_start, end - text_start);
}

static int fast_template_load(FastTemplateContext *context)
{
    int64_t file_size;
    int result;

    if ((result=getFileContent(context->filename, &context->file_content.str,
                    &file_size)) != 0)
    {
        return result;
    }
    context->file_content.len = file_size;

    if ((result=fast_template_preprocess(context)) != 0) {
        free(context->file_content.str);
        FC_SET_STRING_NULL(context->file_content);
        return result;
    }
    return fast_template_parse(context);
}

int fast_template_init(FastTemplateContext *context,
        const char *filename, void *args,
        fast_template_alloc_func alloc_func,
        fast_template_free_func free_func)
{
    memset(context, 0, sizeof(FastTemplateContext));
    context->args = args;
    context->alloc_func = alloc_func;
    context->free_func = free_func;
    context->filename = strdup(filename);
    return fast_template_load(context);
}

void fast_template_destroy(FastTemplateContext *context)
{
    if (context->file_content.str != NULL) {
        free(context->file_content.str);
        context->file_content.str = NULL;
    }

    if (context->node_array.nodes != NULL) {
        free(context->node_array.nodes);
        context->node_array.nodes = NULL;
    }
}

static int fast_template_alloc_output_buffer(FastTemplateContext *context,
        const int total_value_len, BufferInfo *buffer)
{
    buffer->length = 0;
    buffer->alloc_size = context->file_content.len + total_value_len;
    buffer->buff = (char *)context->alloc_func(context->args, buffer->alloc_size);
    if (buffer->buff == NULL) {
        logError("file: "__FILE__", line: %d, "
                "alloc %d bytes fail", __LINE__, buffer->alloc_size);
        return ENOMEM;
    }

    return 0;
}

static int check_realloc_output_buffer(FastTemplateContext *context,
        BufferInfo *buffer, const int inc_size)
{
    char *new_buff;
    int expect_size;

    expect_size = buffer->length + inc_size;
    if (buffer->alloc_size >= expect_size) {
        return 0;
    }

    while (buffer->alloc_size < expect_size) {
        buffer->alloc_size *= 2;
    }
    logInfo("expect_size: %d, resize buff size to %d",
            expect_size, buffer->alloc_size);

    new_buff = (char *)context->alloc_func(context->args, buffer->alloc_size);
    if (new_buff == NULL) {
        logError("file: "__FILE__", line: %d, "
                "alloc %d bytes fail", __LINE__, buffer->alloc_size);
        return ENOMEM;
    }

    memcpy(new_buff, buffer->buff, buffer->length);
    context->free_func(context->args, buffer->buff);
    buffer->buff = new_buff;
    return 0;
}

static char *text_to_html(FastTemplateContext *context, const string_t *value,
        BufferInfo *buffer)
{
    const char *p;
    const char *end;
    char *dest;
    char *buff_end;
    bool space_count;

    space_count = 0;
    dest = buffer->buff + buffer->length;
    buff_end = buffer->buff + buffer->alloc_size;
    end = value->str + value->len;
    for (p=value->str; p<end; p++) {
        if (buff_end - dest < 8) {
            buffer->length = dest - buffer->buff;
            if (check_realloc_output_buffer(context, buffer, 8) != 0) {
                return NULL;
            }

            dest = buffer->buff + buffer->length;
            buff_end = buffer->buff + buffer->alloc_size;
        }
        switch (*p) {
            case '<':
                *dest++ = '&';
                *dest++ = 'l';
                *dest++ = 't';
                *dest++ = ';';
                break;
            case '>':
                *dest++ = '&';
                *dest++ = 'g';
                *dest++ = 't';
                *dest++ = ';';
                break;
            case '&':
                *dest++ = '&';
                *dest++ = 'a';
                *dest++ = 'm';
                *dest++ = 'p';
                *dest++ = ';';
                break;
            case '"':
                *dest++ = '&';
                *dest++ = 'q';
                *dest++ = 'u';
                *dest++ = 'o';
                *dest++ = 't';
                *dest++ = ';';
                break;
            case ' ':
                if (space_count == 0) {
                    *dest++ = *p;
                } else {
                    *dest++ = '&';
                    *dest++ = 'n';
                    *dest++ = 'b';
                    *dest++ = 's';
                    *dest++ = 'p';
                    *dest++ = ';';
                }
                break;
            case '\r':
                break;  //igore \r
            case '\n':
                *dest++ = '<';
                *dest++ = 'b';
                *dest++ = 'r';
                *dest++ = '>';
                break;
            default:
                *dest++ = *p;
                break;
        }
        if (*p == ' ') {
            space_count++;
        } else {
            space_count = 0;
        }
    }

    buffer->length = dest - buffer->buff;
    return dest;
}

int find_value_from_kv_array(const key_value_array_t *params,
        const string_t *key, string_t *value)
{
    const key_value_pair_t *kv;
    const key_value_pair_t *kv_end;

    kv_end = params->kv_pairs + params->count;
    for (kv=params->kv_pairs; kv<kv_end; kv++) {
        if (fc_string_equal(key, &kv->key)) {
            *value = kv->value;
            return 0;
        }
    }
    return ENOENT;
}

int fast_template_render(FastTemplateContext *context,
        void *params, const int total_value_len,
        fast_template_find_param_func find_func, string_t *output)
{
    int i;
    int result;
    BufferInfo buffer;
    string_t *value;
    string_t v;
    char *p;
    bool html_format;

    if ((result=fast_template_alloc_output_buffer(context,
                    total_value_len, &buffer)) != 0)
    {
        return result;
    }

    p = buffer.buff;
    for (i=0; i<context->node_array.count; i++) {
        if (context->node_array.nodes[i].type == FAST_TEMPLATE_NODE_TYPE_STRING) {
            value = &context->node_array.nodes[i].value;
            html_format = false;
        } else {
            value = &v;
            if (find_func(params, &context->node_array.nodes[i].value, value)
                    == 0)
            {
                html_format = true;
            } else {
                value = &fast_template_empty_string;
                html_format = false;
            }
        }

        if (value->len > 0) {
            if (html_format) {
                buffer.length = p - buffer.buff;
                p = text_to_html(context, value, &buffer);
                if (p == NULL) {
                    return ENOMEM;
                }
            } else {
                if ((p - buffer.buff) + value->len > buffer.alloc_size) {
                    buffer.length = p - buffer.buff;
                    if (check_realloc_output_buffer(context, &buffer,
                                value->len) != 0)
                    {
                        return ENOMEM;
                    }
                    p = buffer.buff + buffer.length;
                }
                memcpy(p, value->str, value->len);
                p += value->len;
            }
        }
    }
    buffer.length = p - buffer.buff;

    //logInfo("buffer length: %d, alloc_size: %d", buffer.length, buffer.alloc_size);
    output->str = buffer.buff;
    output->len = buffer.length;
    return 0;
}
