dnl config.m4 for extension fasttemplate

PHP_ARG_WITH(fasttemplate, wapper for libfasttemplate
[  --with-fasttemplate             Include fasttemplate for fast template])

if test "$PHP_FASTTEMPLATE" != "no"; then
  PHP_SUBST(FASTTEMPLATE_SHARED_LIBADD)

  if test -z "$ROOT"; then
	ROOT=/usr/local
  fi

  PHP_ADD_INCLUDE($ROOT/include/fastcommon)
  PHP_ADD_INCLUDE($ROOT/include/fasttemplate)

  PHP_ADD_LIBRARY_WITH_PATH(fastcommon, $ROOT/lib, FASTTEMPLATE_SHARED_LIBADD)
  PHP_ADD_LIBRARY_WITH_PATH(fasttemplate, $ROOT/lib, FASTTEMPLATE_SHARED_LIBADD)

  PHP_NEW_EXTENSION(fasttemplate, fasttemplate.c, $ext_shared)

  CFLAGS="$CFLAGS -Wall"
fi
