
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "fastcommon/logger.h"
#include "fastcommon/shared_func.h"
#include "fastcommon/sched_thread.h"
#include "fast_template.h"

string_t fast_template_empty_string = {NULL, 0};

static int fast_template_alloc_output_buffer(FastTemplateMemoryManager *
        memory_manager, const int alloc_size);

static int check_alloc_node_array(FastTemplateContext *context)
{
    int bytes;

    if (context->node_array.alloc > context->node_array.count) {
        return 0;
    }

    if (context->node_array.alloc == 0) {
        context->node_array.alloc = 32;
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

static int check_alloc_fileinfo_array(FastTemplateContext *context)
{
    int bytes;

    if (context->fileinfo_array.alloc > context->fileinfo_array.count) {
        return 0;
    }

    if (context->fileinfo_array.alloc == 0) {
        context->fileinfo_array.alloc = 8;
    } else {
        context->fileinfo_array.alloc *= 2;
    }

    bytes = sizeof(TemplateFileInfo) * context->fileinfo_array.alloc;
    context->fileinfo_array.files = (TemplateFileInfo *)realloc(
            context->fileinfo_array.files, bytes);
    if (context->fileinfo_array.files == NULL) {
        logError("file: "__FILE__", line: %d, "
                "realloc %d bytes fail", __LINE__, bytes);
        return ENOMEM;
    }

    return 0;
}

static int template_add_node(FastTemplateContext *context,
        const int type, string_t *value)
{
    int result;

    if (value->len == 0) {
        return 0;
    }

    if ((result=check_alloc_node_array(context)) != 0) {
        return result;
    }

    context->node_array.nodes[context->node_array.count].type = type;
    context->node_array.nodes[context->node_array.count].value = *value;
    context->node_array.count++;
    return 0;

}
static int template_add_file(FastTemplateContext *context,
        const char *filename)
{
    int result;
    TemplateFileInfo *fi;
    char *dup_filename;
    struct stat stat_buf;

    if ((result=check_alloc_fileinfo_array(context)) != 0) {
        return result;
    }

    if (stat(filename, &stat_buf) != 0) {
        result = errno != 0 ? errno : ENOENT;
        logError("file: "__FILE__", line: %d, "
                "stat file %s fail, errno: %d, error info: %s",
                __LINE__, filename, result, strerror(result));
        return result;
    }

    fi = context->fileinfo_array.files + context->fileinfo_array.count;
    dup_filename = strdup(filename);
    if (dup_filename == NULL) {
        logError("file: "__FILE__", line: %d, "
                "strdup %s fail", __LINE__, filename);
        return ENOMEM;
    }

    fi->filename = dup_filename;
    fi->last_modified = stat_buf.st_mtime;
    context->fileinfo_array.count++;
    return 0;
}

static inline int template_add_node_ex(FastTemplateContext *context,
        const int type, char *str, const int len)
{
    string_t value;
    FC_SET_STRING_EX(value, str, len);
    return template_add_node(context, type, &value);
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

    if ((result=fast_buffer_append_file(buffer, full_filename)) != 0) {
        if (result == ENOENT) {   //ignore if file not exist
            return 0;
        } else {
            return -1 * result;
        }
    }

    if (context->check_file_mtime) {
        if ((result=template_add_file(context, full_filename)) != 0) {
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
            context->file_content.str = fc_strdup1(buffer.data, buffer.length);
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

        if ((result=template_add_node_ex(context,
                        FAST_TEMPLATE_NODE_TYPE_STRING,
                        text_start, var_start - text_start)) != 0)
        {
            return result;
        }
        if ((result=template_add_node(context,
                        FAST_TEMPLATE_NODE_TYPE_VARIABLE, &var)) != 0)
        {
            return result;
        }

        text_start = p = var_end + 1;
    }

    return template_add_node_ex(context, FAST_TEMPLATE_NODE_TYPE_STRING,
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

int fast_template_memory_manager_init(
        FastTemplateMemoryManager *memory_manager,
        void *args, fast_template_alloc_func alloc_func,
        fast_template_free_func free_func)
{
    memory_manager->args = args;
    memory_manager->alloc_func = alloc_func;
    memory_manager->free_func = free_func;

    return fast_template_alloc_output_buffer(memory_manager, 4096);
}

int fast_template_init(FastTemplateContext *context,
        const char *filename, void *args,
        fast_template_alloc_func alloc_func,
        fast_template_free_func free_func,
        const bool text2html, const bool check_file_mtime)
{
    int result;

    memset(context, 0, sizeof(FastTemplateContext));
    if ((result=template_add_file(context, filename)) != 0) {
        return result;
    }

    fast_template_memory_manager_init(&context->memory_manager,
            args, alloc_func, free_func);
    context->filename = context->fileinfo_array.files[0].filename;
    context->text2html = text2html;
    context->check_file_mtime = check_file_mtime;
    result = fast_template_load(context);
    if (context->check_file_mtime) {
        context->last_check_file_time = get_current_time();
    }
    return result;
}

void fast_template_destroy(FastTemplateContext *context)
{
    if (context->fileinfo_array.files != NULL) {
        TemplateFileInfo *fi;
        TemplateFileInfo *end;

        end = context->fileinfo_array.files + context->fileinfo_array.count;
        for (fi=context->fileinfo_array.files; fi<end; fi++) {
            free(fi->filename);
        }
        free(context->fileinfo_array.files);
        context->fileinfo_array.files = NULL;
    }
    if (context->file_content.str != NULL) {
        free(context->file_content.str);
        context->file_content.str = NULL;
    }

    if (context->node_array.nodes != NULL) {
        free(context->node_array.nodes);
        context->node_array.nodes = NULL;
    }
}

static int fast_template_alloc_output_buffer(FastTemplateMemoryManager *
        memory_manager, const int alloc_size)
{
    BufferInfo *buffer;
    buffer = &memory_manager->buffer;

    buffer->length = 0;
    buffer->alloc_size = alloc_size;
    buffer->buff = (char *)memory_manager->alloc_func(
            memory_manager->args, buffer->alloc_size);
    if (buffer->buff == NULL) {
        logError("file: "__FILE__", line: %d, "
                "alloc %d bytes fail", __LINE__, buffer->alloc_size);
        return ENOMEM;
    }

    return 0;
}

static int check_realloc_buffer(FastTemplateMemoryManager *
        memory_manager, const int inc_size)
{
    BufferInfo *buffer;
    char *new_buff;
    int expect_size;
    int alloc_size;

    buffer = &memory_manager->buffer;

    expect_size = buffer->length + inc_size;
    if (buffer->alloc_size >= expect_size) {
        return 0;
    }

    alloc_size = buffer->alloc_size;
    while (alloc_size < expect_size) {
        alloc_size *= 2;
    }
    logInfo("expect_size: %d, resize buff size from %d to %d",
            expect_size, buffer->alloc_size, alloc_size);

    new_buff = (char *)memory_manager->alloc_func(memory_manager->args,
            alloc_size);
    if (new_buff == NULL) {
        logError("file: "__FILE__", line: %d, "
                "alloc %d bytes fail", __LINE__, alloc_size);
        return ENOMEM;
    }

    memcpy(new_buff, buffer->buff, buffer->length);
    memory_manager->free_func(memory_manager->args, buffer->buff);
    buffer->buff = new_buff;
    buffer->alloc_size = alloc_size;
    return 0;
}

int fast_template_reset_realloc_buffer(FastTemplateMemoryManager *
        memory_manager, const int alloc_size)
{
    memory_manager->buffer.length = 0;
    return check_realloc_buffer(memory_manager, alloc_size);
}

char *fast_template_text2html(FastTemplateMemoryManager *
        memory_manager, const string_t *value, string_t *output)
{
    BufferInfo *buffer;
    const char *p;
    const char *end;
    char *dest;
    char *buff_end;
    int space_count;

    buffer = &memory_manager->buffer;
    if (output != NULL) {
        buffer->length = 0;  //reset buffer length
    }

    space_count = 0;
    dest = buffer->buff + buffer->length;
    buff_end = buffer->buff + buffer->alloc_size;
    end = value->str + value->len;
    for (p=value->str; p<end; p++) {
        if (buff_end - dest < 8) {
            buffer->length = dest - buffer->buff;
            if (check_realloc_buffer(memory_manager, 8) != 0) {
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
    if (output != NULL) {
        output->str = buffer->buff;
        output->len = buffer->length;
    }
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
        void *params, const int total_value_len, const bool text2html,
        fast_template_find_param_func find_func, string_t *output)
{
    BufferInfo *buffer;
    int i;
    int result;
    int alloc_size;
    string_t *value;
    string_t v;
    char *p;
    bool html_format;

    buffer = &context->memory_manager.buffer;
    buffer->length = 0;  //reset buffer length

    alloc_size = context->file_content.len + total_value_len;
    if ((result=check_realloc_buffer(&context->memory_manager,
                    alloc_size)) != 0)
    {
        return result;
    }

    p = buffer->buff;
    for (i=0; i<context->node_array.count; i++) {
        if (context->node_array.nodes[i].type == FAST_TEMPLATE_NODE_TYPE_STRING) {
            value = &context->node_array.nodes[i].value;
            html_format = false;
        } else {
            value = &v;
            if (find_func(params, &context->node_array.nodes[i].value, value)
                    == 0)
            {
                html_format = text2html;
            } else {
                value = &fast_template_empty_string;
                html_format = false;
            }
        }

        if (value->len > 0) {
            if (html_format) {
                buffer->length = p - buffer->buff;
                p = fast_template_text2html(&context->memory_manager,
                        value, NULL);
                if (p == NULL) {
                    return ENOMEM;
                }
            } else {
                if ((p - buffer->buff) + value->len > buffer->alloc_size) {
                    buffer->length = p - buffer->buff;
                    if (check_realloc_buffer(&context->memory_manager,
                                value->len) != 0)
                    {
                        return ENOMEM;
                    }
                    p = buffer->buff + buffer->length;
                }
                memcpy(p, value->str, value->len);
                p += value->len;
            }
        }
    }
    buffer->length = p - buffer->buff;

    //logInfo("buffer length: %d, alloc_size: %d", buffer->length, buffer->alloc_size);
    output->str = buffer->buff;
    output->len = buffer->length;
    return 0;
}

bool fast_template_file_modified(FastTemplateContext *context)
{
    TemplateFileInfo *fi;
    TemplateFileInfo *end;
    struct stat stat_buf;

    end = context->fileinfo_array.files + context->fileinfo_array.count;
    for (fi=context->fileinfo_array.files; fi<end; fi++) {
        if (stat(fi->filename, &stat_buf) != 0) {
            logError("file: "__FILE__", line: %d, "
                    "stat file %s fail, errno: %d, error info: %s",
                    __LINE__, fi->filename, errno, strerror(errno));
            continue;
        }

        if (fi->last_modified != stat_buf.st_mtime) {
            logInfo("file: "__FILE__", line: %d, "
                    "file %s modified", __LINE__, fi->filename);
            return true;
        }
    }

    return false;
}
