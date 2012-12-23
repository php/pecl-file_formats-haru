dnl $Id$

PHP_ARG_WITH(haru, for Haru PDF support,
[  --with-haru             Include Haru PDF library support])

if test -z "$PHP_ZLIB_DIR"; then
  PHP_ARG_WITH(zlib-dir, for the location of libz,
  [  --with-zlib-dir[=DIR]     Haru: Set the path to libz install prefix], no, no)
fi

if test -z "$PHP_PNG_DIR"; then
  PHP_ARG_WITH(png-dir, for the location of libpng,
  [  --with-png-dir[=DIR]      Haru: Set the path to libpng install prefix], no, no)
fi

if test "$PHP_HARU" != "no"; then
  
  SEARCH_PATH="/usr/local/ /usr/"
  SEARCH_FOR="include/hpdf.h"
  if test "$PHP_HARU" = "yes"; then
	AC_MSG_CHECKING([for Haru in default path])
    for i in $SEARCH_PATH; do
      if test -r "$i/$SEARCH_FOR"; then
        HARU_DIR="$i"
        AC_MSG_RESULT(found in $i)
      fi
    done
	if test -z "$HARU_DIR"; then 
      AC_MSG_RESULT([not found])
      AC_MSG_ERROR([Please specify prefix for your Haru PDF library installation])
	fi
  elif test -r $PHP_HARU/$SEARCH_FOR; then
	HARU_DIR=$PHP_HARU
  fi
 
  if test -z "$HARU_DIR"; then 
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Could not find Haru PDF library in $PHP_HARU])
  fi
  
  PHP_ADD_INCLUDE($HARU_DIR/include)

  LIBNAME=hpdf
  LIBSYMBOL=HPDF_Stream_WriteToStreamWithDeflate

  if test "$PHP_ZLIB_DIR" != "no"; then
    PHP_ADD_LIBRARY_WITH_PATH(z, $PHP_ZLIB_DIR/$PHP_LIBDIR, HARU_SHARED_LIBADD)
    PHP_CHECK_LIBRARY($LIBNAME, $LIBSYMBOL, [], [
      AC_MSG_ERROR([Haru configure failed. Please check config.log for more information.])
    ], [
      -L$PHP_ZLIB_DIR/$PHP_LIBDIR -L$HARU_DIR/$PHP_LIBDIR 
    ])  
  else
    PHP_ADD_LIBRARY(z,, HARU_SHARED_LIBADD)
    PHP_CHECK_LIBRARY($LIBNAME, $LIBSYMBOL, [], [
      AC_MSG_ERROR([Please check if the prefix is correct and try adding --with-zlib-dir=<DIR>. See config.log for more information.])
    ], [
      -L$HARU_DIR/$PHP_LIBDIR
    ])   
  fi

  LIBNAME=hpdf
  LIBSYMBOL=HPDF_LoadPngImageFromFile

  if test "$PHP_PNG_DIR" != "no"; then
    PHP_ADD_LIBRARY_WITH_PATH(png, $PHP_PNG_DIR/$PHP_LIBDIR, HARU_SHARED_LIBADD)
    PHP_CHECK_LIBRARY($LIBNAME, $LIBSYMBOL, [], [
      AC_MSG_ERROR([Haru configure failed. Please check config.log for more information.])
    ], [
      -L$PHP_PNG_DIR/$PHP_LIBDIR -L$HARU_DIR/$PHP_LIBDIR 
    ])  
  else
    PHP_ADD_LIBRARY(png,, HARU_SHARED_LIBADD)
    PHP_CHECK_LIBRARY($LIBNAME, $LIBSYMBOL, [], [
      AC_MSG_ERROR([Try adding --with-png-dir=<DIR>. Please check config.log for more information.])
    ], [
     -L$HARU_DIR/$PHP_LIBDIR
    ])   
  fi

  PHP_ADD_LIBRARY_WITH_PATH(hpdf, $HARU_DIR/$PHP_LIBDIR, HARU_SHARED_LIBADD)

  PHP_SUBST(HARU_SHARED_LIBADD)
  PHP_NEW_EXTENSION(haru, haru.c, $ext_shared)
fi
