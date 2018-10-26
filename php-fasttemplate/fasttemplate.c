#include "fastcommon/php7_ext_wrapper.h"
#include "ext/standard/info.h"
#include <zend_extensions.h>
#include <zend_exceptions.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include "fastcommon/common_define.h"
#include "fastcommon/logger.h"
#include "template_manager.h"
#include "fasttemplate.h"

#define MAJOR_VERSION  1
#define MINOR_VERSION  0
#define PATCH_VERSION  0

static TemplateManagerContext context;

#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 3)
const zend_fcall_info empty_fcall_info = { 0, NULL, NULL, NULL, NULL, 0, NULL, NULL, 0 };
#undef ZEND_BEGIN_ARG_INFO_EX
#define ZEND_BEGIN_ARG_INFO_EX(name, pass_rest_by_reference, return_reference, required_num_args) \
    static zend_arg_info name[] = {                                                               \
        { NULL, 0, NULL, 0, 0, 0, pass_rest_by_reference, return_reference, required_num_args },
#endif

// Every user visible function must have an entry in fasttemplate_functions[].
	zend_function_entry fasttemplate_functions[] = {
		ZEND_FE(fasttemplate_version, NULL)
		ZEND_FE(fasttemplate_render, NULL)
		{NULL, NULL, NULL}  /* Must be the last line */
	};

zend_module_entry fasttemplate_module_entry = {
	STANDARD_MODULE_HEADER,
	"fasttemplate",
	fasttemplate_functions,
	PHP_MINIT(fasttemplate),
	PHP_MSHUTDOWN(fasttemplate),
	NULL,//PHP_RINIT(fasttemplate),
	NULL,//PHP_RSHUTDOWN(fasttemplate),
	PHP_MINFO(fasttemplate),
	"1.00",
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_FASTTEMPLATE
	ZEND_GET_MODULE(fasttemplate)
#endif

static void * template_alloc_func(void *args, size_t size)
{
    return emalloc(size);
}

static void template_free_func(void *args, void *ptr)
{
    efree(ptr);
}

PHP_MINIT_FUNCTION(fasttemplate)
{
#define ITEM_NAME_INIT_CAPACITY "fasttemplate.init_capacity"
    zval zv;
    zval *zv_init_capacity;
    int init_capacity;

    log_try_init();

    zv_init_capacity = &zv;
    if (zend_get_configuration_directive_wrapper(ITEM_NAME_INIT_CAPACITY,
                sizeof(ITEM_NAME_INIT_CAPACITY), &zv_init_capacity) != SUCCESS)
    {
        init_capacity = 100;
        fprintf(stderr, "file: "__FILE__", line: %d, "
                "fasttemplate.ini does not have item "
                "\"%s\", set to %d!\n", __LINE__,
                ITEM_NAME_INIT_CAPACITY, init_capacity);
    } else {
        init_capacity = atoi(Z_STRVAL_P(zv_init_capacity));
        if (init_capacity <= 0) {
            init_capacity = 100;
            fprintf(stderr, "file: "__FILE__", line: %d, "
                    "fasttemplate.ini, item: \"%s\" is invalid, "
                    "set to %d!\n", __LINE__,
                    ITEM_NAME_INIT_CAPACITY, init_capacity);
        }
    }

    if (template_manager_init(&context, NULL, template_alloc_func,
                template_free_func, init_capacity) == 0)
    {
        return SUCCESS;
    } else {
        return FAILURE;
    }
}

PHP_MSHUTDOWN_FUNCTION(fasttemplate)
{
	return SUCCESS;
}

PHP_RINIT_FUNCTION(fasttemplate)
{
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(fasttemplate)
{
	return SUCCESS;
}

PHP_MINFO_FUNCTION(fasttemplate)
{
	char buffer[64];
	sprintf(buffer, "fasttemplate v%d.%02d support", 
		MAJOR_VERSION, MINOR_VERSION);

	php_info_print_table_start();
	php_info_print_table_header(2, buffer, "enabled");
	php_info_print_table_end();
}

/*
string fasttemplate_version()
return client library version
*/
ZEND_FUNCTION(fasttemplate_version)
{
	char szVersion[16];
	int len;

	len = sprintf(szVersion, "%d.%d.%d",
		MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);

	ZEND_RETURN_STRINGL(szVersion, len, 1);
}

static int hashtable_find(HashTable *ht, const string_t *key, string_t *value)
{
    zval *v;
    static char buff[32];

    if (zend_hash_find_wrapper(ht, key->str, key->len, &v) == SUCCESS) {
        if (ZEND_TYPE_OF(v) == IS_STRING) {
            value->str = Z_STRVAL_P(v);
            value->len = Z_STRLEN_P(v);
        } else if (ZEND_TYPE_OF(v) == IS_LONG) {
            value->str = buff;
            value->len = sprintf(value->str, "%ld", v->value.lval);
        } else if (ZEND_IS_BOOL(v)) {
            value->str = buff;
            if (v->value.lval) {
                value->len = sprintf(value->str, "true");
            } else {
                value->len = sprintf(value->str, "false");
            }
        } else if (ZEND_TYPE_OF(v) == IS_DOUBLE) {
            value->str = buff;
            value->len = sprintf(value->str, "%.2f", v->value.dval);
        } else if (ZEND_TYPE_OF(v) == IS_NULL) {
            value->str = buff;
            value->len = 0;
        } else {
            logError("file: "__FILE__", line: %d, "
                "key \"%s\" is invalid, zend type: %d!",
                __LINE__, key->str, ZEND_TYPE_OF(v));
            value->str = buff;
            value->len = 0;
        }
        return 0;
    } else {
        return ENOENT;
    }
}

ZEND_FUNCTION(fasttemplate_render)
{
	int argc;
    string_t template_filename;
    zend_size_t filename_len;
	zval *params;
	HashTable *ht;
    string_t output;
    int total_value_len;
#if PHP_MAJOR_VERSION >= 7
    zend_string *sz_data;
    bool use_heap_data;
#endif

	argc = ZEND_NUM_ARGS();
	if (argc < 2) {
		logError("file: "__FILE__", line: %d, "
			"fasttemplate_render need array parameter",
			__LINE__);
		RETURN_BOOL(false);
	}

	if (zend_parse_parameters(argc TSRMLS_CC, "sa", &template_filename.str,
                &filename_len, &params) == FAILURE)
    {
		logError("file: "__FILE__", line: %d, "
			"fasttemplate_render zend_parse_parameters fail!", __LINE__);
		RETURN_BOOL(false);
	}
    template_filename.len = filename_len;

	ht = Z_ARRVAL_P(params);
    total_value_len = zend_hash_num_elements(ht) * 64;

    logInfo("element count: %d", zend_hash_num_elements(ht));
    if (template_manager_render(&context, &template_filename, ht,
            total_value_len, (fast_template_find_param_func)hashtable_find,
            &output) != 0)
    {
        RETURN_BOOL(false);
    }

        ZEND_RETURN_STRINGL(output.str, output.len, 1);

#if PHP_MAJOR_VERSION < 7
    INIT_ZVAL(return_value);
    ZVAL_STRINGL(&return_value, output.str, output.len, 0);
#else
    ZSTR_ALLOCA_INIT(sz_data, output.str, output.len, use_heap_data);
    efree(output.str);
    RETVAL_NEW_STR(sz_data);
#endif

}
