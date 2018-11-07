#ifndef FASTTEMPLATE_H
#define FASTTEMPLATE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PHP_WIN32
#define PHP_FASTTEMPLATE_API __declspec(dllexport)
#else
#define PHP_FASTTEMPLATE_API
#endif

PHP_MINIT_FUNCTION(fasttemplate);
PHP_RINIT_FUNCTION(fasttemplate);
PHP_MSHUTDOWN_FUNCTION(fasttemplate);
PHP_RSHUTDOWN_FUNCTION(fasttemplate);
PHP_MINFO_FUNCTION(fasttemplate);

ZEND_FUNCTION(fasttemplate_version);
ZEND_FUNCTION(fasttemplate_render);
ZEND_FUNCTION(fasttemplate_text2html);

#ifdef __cplusplus
}
#endif

#endif	/* FASTTEMPLATE_H */
