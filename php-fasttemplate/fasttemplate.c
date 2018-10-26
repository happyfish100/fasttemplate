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
#include "fasttemplate.h"

#define MAJOR_VERSION  1
#define MINOR_VERSION  0
#define PATCH_VERSION  0

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

PHP_MINIT_FUNCTION(fasttemplate)
{
    return SUCCESS;
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

ZEND_FUNCTION(fasttemplate_render)
{
	int argc;
    char *template_filename;
    zend_size_t filename_len;
	zval *params;
	HashTable *hash;
    string_t output;

	argc = ZEND_NUM_ARGS();
	if (argc < 2) {
		logError("file: "__FILE__", line: %d, "
			"fasttemplate_render need array parameter",
			__LINE__);
		RETURN_BOOL(false);
	}

	if (zend_parse_parameters(argc TSRMLS_CC, "sa", &template_filename,
                &filename_len, &params) == FAILURE)
    {
		logError("file: "__FILE__", line: %d, "
			"fasttemplate_render zend_parse_parameters fail!", __LINE__);
		RETURN_BOOL(false);
	}

	hash = Z_ARRVAL_P(params);

    output.str = NULL;
    output.len = 0;

    ZEND_RETURN_STRINGL(output.str, output.len, 1);
}
