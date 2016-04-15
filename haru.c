/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2008 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Antony Dovgal <tony2001@php.net>                             |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_exceptions.h"
#include "php_haru.h"
#include <hpdf.h>

#define PHP_HARU_BUF_SIZE 32768

#ifdef Z_SET_REFCOUNT_P
# define HARU_SET_REFCOUNT_AND_IS_REF(z) \
	Z_SET_REFCOUNT_P(z, 1); \
	Z_SET_ISREF_P(z);
#else
# define HARU_SET_REFCOUNT_AND_IS_REF(z) \
	z->refcount = 1; \
	z->is_ref = 1;
#endif

/* {{{ structs and static vars */
static zend_class_entry *ce_haruexception;
static zend_class_entry *ce_harudoc;
static zend_class_entry *ce_harupage;
static zend_class_entry *ce_harufont;
static zend_class_entry *ce_haruimage;
static zend_class_entry *ce_harudestination;
static zend_class_entry *ce_haruannotation;
static zend_class_entry *ce_haruencoder;
static zend_class_entry *ce_haruoutline;

static zend_object_handlers php_harudoc_handlers;
static zend_object_handlers php_harupage_handlers;
static zend_object_handlers php_harufont_handlers;
static zend_object_handlers php_haruimage_handlers;
static zend_object_handlers php_harudestination_handlers;
static zend_object_handlers php_haruannotation_handlers;
static zend_object_handlers php_haruencoder_handlers;
static zend_object_handlers php_haruoutline_handlers;

typedef struct _php_harudoc {
	zend_object std;
	HPDF_Doc h;
} php_harudoc;

typedef struct _php_harupage {
	zend_object std;
	zval doc;
	HPDF_Page h;
} php_harupage;

typedef struct _php_harufont {
	zend_object std;
	zval doc;
	HPDF_Font h;
} php_harufont;

typedef struct _php_haruimage {
	zend_object std;
	zval doc;
	HPDF_Image h;
	char *filename;
} php_haruimage;

typedef struct _php_harudestination {
	zend_object std;
	zval page;
	HPDF_Destination h;
} php_harudestination;

typedef struct _php_haruannotation {
	zend_object std;
	zval page;
	HPDF_Annotation h;
} php_haruannotation;

typedef struct _php_haruencoder {
	zend_object std;
	zval doc;
	HPDF_Encoder h;
} php_haruencoder;

typedef struct _php_haruoutline {
	zend_object std;
	zval doc;
	HPDF_Outline h;
} php_haruoutline;

/* }}} */

/* macros {{{ */

#if PHP_API_VERSION < 20100412
#define HARU_CHECK_FILE(filename)																\
	do {																						\
		php_set_error_handling(EH_THROW, ce_haruexception TSRMLS_CC);							\
		if (PG(safe_mode) && (!php_checkuid(filename, NULL, CHECKUID_CHECK_FILE_AND_DIR))) {	\
			php_set_error_handling(EH_NORMAL, NULL TSRMLS_CC);									\
			return;																				\
		}																						\
		if (php_check_open_basedir(filename TSRMLS_CC)) {										\
			php_set_error_handling(EH_NORMAL, NULL TSRMLS_CC);									\
			return;																				\
		}																						\
		php_set_error_handling(EH_NORMAL, NULL TSRMLS_CC);										\
	} while(0)
#else
#define HARU_CHECK_FILE(filename)																\
	do {																						\
		zend_replace_error_handling(EH_THROW, ce_haruexception, NULL TSRMLS_CC);							\
		if (php_check_open_basedir(filename TSRMLS_CC)) {										\
			zend_replace_error_handling(EH_NORMAL, NULL, NULL TSRMLS_CC);								\
			return;																				\
		}																						\
		zend_replace_error_handling(EH_NORMAL, NULL, NULL TSRMLS_CC);									\
	} while(0)
#endif

#define PHP_HARU_NULL_CHECK(ret, message)										\
	do {																		\
		if (!ret) {																\
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, message);	\
			return;																\
		}																		\
	} while(0)

/* }}} */

/* constructors and destructors {{{ */

static void php_harudoc_dtor(void *object TSRMLS_DC) /* {{{ */
{
	php_harudoc *doc = (php_harudoc *)object;

	if (doc->h) {
		HPDF_Free(doc->h);
		doc->h = NULL;
	}

	zend_object_std_dtor(&doc->std TSRMLS_CC);
	efree(doc);
}

/* }}} */

static zend_object_value php_harudoc_new(zend_class_entry *ce TSRMLS_DC) /* {{{ */
{
	php_harudoc *doc;
	zend_object_value retval;
#if ZEND_MODULE_API_NO  < 20100409
	zval *tmp;
#endif

	doc = ecalloc(1, sizeof(*doc));
	zend_object_std_init(&doc->std, ce TSRMLS_CC);

#if ZEND_MODULE_API_NO  >= 20100409
	object_properties_init(&doc->std, ce);
#else
	zend_hash_copy(doc->std.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));
#endif

	retval.handle = zend_objects_store_put(doc, (zend_objects_store_dtor_t)zend_objects_destroy_object, php_harudoc_dtor, NULL TSRMLS_CC);
	retval.handlers = &php_harudoc_handlers;

	return retval;
}

/* }}} */

static void php_harupage_dtor(void *object TSRMLS_DC) /* {{{ */
{
	php_harupage *page = (php_harupage *)object;

	if (page->h) {
		page->h = NULL;
	}

	zend_objects_store_del_ref(&page->doc TSRMLS_CC);

	zend_object_std_dtor(&page->std TSRMLS_CC);
	efree(page);
}

/* }}} */

static zend_object_value php_harupage_new(zend_class_entry *ce TSRMLS_DC) /* {{{ */
{
	php_harupage *page;
	zend_object_value retval;
#if ZEND_MODULE_API_NO  < 20100409
	zval *tmp;
#endif

	page = ecalloc(1, sizeof(*page));
	zend_object_std_init(&page->std, ce TSRMLS_CC);

#if ZEND_MODULE_API_NO  >= 20100409
	object_properties_init(&page->std, ce);
#else
	zend_hash_copy(page->std.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));
#endif

	retval.handle = zend_objects_store_put(page, (zend_objects_store_dtor_t)zend_objects_destroy_object, php_harupage_dtor, NULL TSRMLS_CC);
	retval.handlers = &php_harupage_handlers;
	return retval;
}

/* }}} */

static void php_harufont_dtor(void *object TSRMLS_DC) /* {{{ */
{
	php_harufont *font = (php_harufont *)object;

	if (font->h) {
		font->h = NULL;
	}

	zend_objects_store_del_ref(&font->doc TSRMLS_CC);

	zend_object_std_dtor(&font->std TSRMLS_CC);
	efree(font);
}

/* }}} */

static zend_object_value php_harufont_new(zend_class_entry *ce TSRMLS_DC) /* {{{ */
{
	php_harufont *font;
	zend_object_value retval;
#if ZEND_MODULE_API_NO  < 20100409
	zval *tmp;
#endif

	font = ecalloc(1, sizeof(*font));
	zend_object_std_init(&font->std, ce TSRMLS_CC);

#if ZEND_MODULE_API_NO  >= 20100409
	object_properties_init(&font->std, ce);
#else
	zend_hash_copy(font->std.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));
#endif

	retval.handle = zend_objects_store_put(font, (zend_objects_store_dtor_t)zend_objects_destroy_object, php_harufont_dtor, NULL TSRMLS_CC);
	retval.handlers = &php_harufont_handlers;
	return retval;
}

/* }}} */

static void php_haruimage_dtor(void *object TSRMLS_DC) /* {{{ */
{
	php_haruimage *image = (php_haruimage *)object;

	if (image->h) {
		image->h = NULL;
	}

	if (image->filename) {
		efree(image->filename);
		image->filename = NULL;
	}

	zend_objects_store_del_ref(&image->doc TSRMLS_CC);

	zend_object_std_dtor(&image->std TSRMLS_CC);
	efree(image);
}

/* }}} */

static zend_object_value php_haruimage_new(zend_class_entry *ce TSRMLS_DC) /* {{{ */
{
	php_haruimage *image;
	zend_object_value retval;
#if ZEND_MODULE_API_NO  < 20100409
	zval *tmp;
#endif

	image = ecalloc(1, sizeof(*image));
	zend_object_std_init(&image->std, ce TSRMLS_CC);

#if ZEND_MODULE_API_NO  >= 20100409
	object_properties_init(&image->std, ce);
#else
	zend_hash_copy(image->std.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));
#endif

	retval.handle = zend_objects_store_put(image, (zend_objects_store_dtor_t)zend_objects_destroy_object, php_haruimage_dtor, NULL TSRMLS_CC);
	retval.handlers = &php_haruimage_handlers;
	return retval;
}

/* }}} */

static void php_harudestination_dtor(void *object TSRMLS_DC) /* {{{ */
{
	php_harudestination *destination = (php_harudestination *)object;

	if (destination->h) {
		destination->h = NULL;
	}

	zend_objects_store_del_ref(&destination->page TSRMLS_CC);

	zend_object_std_dtor(&destination->std TSRMLS_CC);
	efree(destination);
}

/* }}} */

static zend_object_value php_harudestination_new(zend_class_entry *ce TSRMLS_DC) /* {{{ */
{
	php_harudestination *destination;
	zend_object_value retval;
#if ZEND_MODULE_API_NO  < 20100409
	zval *tmp;
#endif

	destination = ecalloc(1, sizeof(*destination));
	zend_object_std_init(&destination->std, ce TSRMLS_CC);

#if ZEND_MODULE_API_NO  >= 20100409
	object_properties_init(&destination->std, ce);
#else
	zend_hash_copy(destination->std.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));
#endif

	retval.handle = zend_objects_store_put(destination, (zend_objects_store_dtor_t)zend_objects_destroy_object, php_harudestination_dtor, NULL TSRMLS_CC);
	retval.handlers = &php_harudestination_handlers;
	return retval;
}

/* }}} */

static void php_haruannotation_dtor(void *object TSRMLS_DC) /* {{{ */
{
	php_haruannotation *annotation = (php_haruannotation *)object;

	if (annotation->h) {
		annotation->h = NULL;
	}

	zend_objects_store_del_ref(&annotation->page TSRMLS_CC);

	zend_object_std_dtor(&annotation->std TSRMLS_CC);
	efree(annotation);
}

/* }}} */

static zend_object_value php_haruannotation_new(zend_class_entry *ce TSRMLS_DC) /* {{{ */
{
	php_haruannotation *annotation;
	zend_object_value retval;
#if ZEND_MODULE_API_NO  < 20100409
	zval *tmp;
#endif

	annotation = ecalloc(1, sizeof(*annotation));
	zend_object_std_init(&annotation->std, ce TSRMLS_CC);

#if ZEND_MODULE_API_NO  >= 20100409
	object_properties_init(&annotation->std, ce);
#else
	zend_hash_copy(annotation->std.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));
#endif

	retval.handle = zend_objects_store_put(annotation, (zend_objects_store_dtor_t)zend_objects_destroy_object, php_haruannotation_dtor, NULL TSRMLS_CC);
	retval.handlers = &php_haruannotation_handlers;
	return retval;
}

/* }}} */

static void php_haruencoder_dtor(void *object TSRMLS_DC) /* {{{ */
{
	php_haruencoder *encoder = (php_haruencoder *)object;

	if (encoder->h) {
		encoder->h = NULL;
	}

	zend_objects_store_del_ref(&encoder->doc TSRMLS_CC);

	zend_object_std_dtor(&encoder->std TSRMLS_CC);
	efree(encoder);
}

/* }}} */

static zend_object_value php_haruencoder_new(zend_class_entry *ce TSRMLS_DC) /* {{{ */
{
	php_haruencoder *encoder;
	zend_object_value retval;
#if ZEND_MODULE_API_NO  < 20100409
	zval *tmp;
#endif

	encoder = ecalloc(1, sizeof(*encoder));
	zend_object_std_init(&encoder->std, ce TSRMLS_CC);

#if ZEND_MODULE_API_NO  >= 20100409
	object_properties_init(&encoder->std, ce);
#else
	zend_hash_copy(encoder->std.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));
#endif

	retval.handle = zend_objects_store_put(encoder, (zend_objects_store_dtor_t)zend_objects_destroy_object, php_haruencoder_dtor, NULL TSRMLS_CC);
	retval.handlers = &php_haruencoder_handlers;
	return retval;
}

/* }}} */

static void php_haruoutline_dtor(void *object TSRMLS_DC) /* {{{ */
{
	php_haruoutline *outline = (php_haruoutline *)object;

	if (outline->h) {
		outline->h = NULL;
	}

	zend_objects_store_del_ref(&outline->doc TSRMLS_CC);

	zend_object_std_dtor(&outline->std TSRMLS_CC);
	efree(outline);
}

/* }}} */

static zend_object_value php_haruoutline_new(zend_class_entry *ce TSRMLS_DC) /* {{{ */
{
	php_haruoutline *outline;
	zend_object_value retval;
#if ZEND_MODULE_API_NO  < 20100409
	zval *tmp;
#endif

	outline = ecalloc(1, sizeof(*outline));
	zend_object_std_init(&outline->std, ce TSRMLS_CC);

#if ZEND_MODULE_API_NO  >= 20100409
	object_properties_init(&outline->std, ce);
#else
	zend_hash_copy(outline->std.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));
#endif

	retval.handle = zend_objects_store_put(outline, (zend_objects_store_dtor_t)zend_objects_destroy_object, php_haruoutline_dtor, NULL TSRMLS_CC);
	retval.handlers = &php_haruoutline_handlers;
	return retval;
}
/* }}} */

/* }}} */

/* internal utilities {{{ */

static int php_haru_status_to_errmsg(HPDF_STATUS status, char **msg) /* {{{ */
{
	if (status == HPDF_OK) {
		*msg = estrdup("No error");
		return 0;
	}
	switch(status) {
		case HPDF_ARRAY_COUNT_ERR:
		case HPDF_ARRAY_ITEM_NOT_FOUND:
		case HPDF_ARRAY_ITEM_UNEXPECTED_TYPE:
		case HPDF_DICT_ITEM_NOT_FOUND:
		case HPDF_DICT_ITEM_UNEXPECTED_TYPE:
		case HPDF_DICT_STREAM_LENGTH_NOT_FOUND:
		case HPDF_DOC_INVALID_OBJECT:
		case HPDF_ERR_UNKNOWN_CLASS:
		case HPDF_INVALID_FONTDEF_TYPE:
		case HPDF_INVALID_OBJ_ID:
		case HPDF_INVALID_STREAM:
		case HPDF_ITEM_NOT_FOUND:
		case HPDF_NAME_INVALID_VALUE:
		case HPDF_NAME_OUT_OF_RANGE:
		case HPDF_PAGES_MISSING_KIDS_ENTRY:
		case HPDF_PAGE_CANNOT_FIND_OBJECT:
		case HPDF_PAGE_CANNOT_GET_ROOT_PAGES:
		case HPDF_PAGE_CANNOT_SET_PARENT:
		case HPDF_PAGE_INVALID_INDEX:
		case HPDF_STREAM_READLN_CONTINUE:
		case HPDF_UNSUPPORTED_FONT_TYPE:
		case HPDF_XREF_COUNT_ERR:
			*msg = estrdup("libharu internal error. The consistency of the data was lost");
			break;
		case HPDF_BINARY_LENGTH_ERR:
			*msg = estrdup("The length of the data exceeds HPDF_LIMIT_MAX_STRING_LEN");
			break;
		case HPDF_CANNOT_GET_PALLET:
			*msg = estrdup("Cannot get a pallet data from PNG image");
			break;
		case HPDF_DICT_COUNT_ERR:
			*msg = estrdup("The count of elements of a dictionary exceeds HPDF_LIMIT_MAX_DICT_ELEMENT");
			break;
		case HPDF_DOC_ENCRYPTDICT_NOT_FOUND:
			*msg = estrdup("Cannot set permissions and encryption mode before a password is set");
			break;
		case HPDF_DUPLICATE_REGISTRATION:
			*msg = estrdup("Tried to register a font that has been registered");
			break;
		case HPDF_EXCEED_JWW_CODE_NUM_LIMIT:
			*msg = estrdup("Cannot register a character to the Japanese word wrap characters list");
			break;
		case HPDF_ENCRYPT_INVALID_PASSWORD:
			*msg = estrdup("Tried to set the owner password to NULL or the owner password and user password are the same");
			break;
		case HPDF_EXCEED_GSTATE_LIMIT:
			*msg = estrdup("The depth of the stack exceeded HPDF_LIMIT_MAX_GSTATE");
			break;
		case HPDF_FAILD_TO_ALLOC_MEM:
			*msg = estrdup("Memory allocation failed");
			break;
		case HPDF_FILE_IO_ERROR:
			*msg = estrdup("File processing failed");
			break;
		case HPDF_FILE_OPEN_ERROR:
			*msg = estrdup("Cannot open a file");
			break;
		case HPDF_FONT_EXISTS:
			*msg = estrdup("Tried to load a font that has been registered");
			break;
		case HPDF_FONT_INVALID_WIDTHS_TABLE:
			*msg = estrdup("The format of a font-file is invalid");
			break;
		case HPDF_INVALID_AFM_HEADER:
			*msg = estrdup("Cannot recognize a header of an afm file");
			break;
		case HPDF_INVALID_ANNOTATION:
			*msg = estrdup("The specified annotation handle is invalid");
			break;
		case HPDF_INVALID_BIT_PER_COMPONENT:
			*msg = estrdup("Bit-per-component of a image which was set as mask-image is invalid");
			break;
		case HPDF_INVALID_CHAR_MATRICS_DATA:
			*msg = estrdup("Cannot recognize char-matrics-data of an afm file");
			break;
		case HPDF_INVALID_COLOR_SPACE:
			*msg = estrdup("The color_space parameter is invalid, or color-space of the image which was set as mask-image is invalid or the function which is invalid in the present color-space was invoked");
			break;
		case HPDF_INVALID_COMPRESSION_MODE:
			*msg = estrdup("Invalid compression mode specified");
			break;
		case HPDF_INVALID_DATE_TIME:
			*msg = estrdup("An invalid date-time value was set");
			break;
		case HPDF_INVALID_DESTINATION:
			*msg = estrdup("An invalid annotation handle was set");
			break;
		case HPDF_INVALID_DOCUMENT:
			*msg = estrdup("An invalid document handle is set");
			break;
		case HPDF_INVALID_DOCUMENT_STATE:
			*msg = estrdup("The function which is invalid in the present state was invoked");
			break;
		case HPDF_INVALID_ENCODER:
			*msg = estrdup("An invalid encoder handle is set");
			break;
		case HPDF_INVALID_ENCODER_TYPE:
			*msg = estrdup("A combination between font and encoder is wrong");
			break;
		case HPDF_INVALID_ENCODING_NAME:
			*msg = estrdup("An invalid encoding name is specified");
			break;
		case HPDF_INVALID_ENCRYPT_KEY_LEN:
			*msg = estrdup("The length of the key of encryption is invalid");
			break;
		case HPDF_INVALID_FONTDEF_DATA:
			*msg = estrdup("An invalid font handle was set or the font format is unsupported");
			break;
		case HPDF_INVALID_FONT_NAME:
			*msg = estrdup("A font which has the specified name is not found");
			break;
		case HPDF_INVALID_IMAGE:
		case HPDF_INVALID_JPEG_DATA:
			*msg = estrdup("Unsupported or invalid image format");
			break;
		case HPDF_INVALID_N_DATA:
			*msg = estrdup("Cannot read a postscript-name from an afm file");
			break;
		case HPDF_INVALID_OBJECT:
			*msg = estrdup("An invalid object is set");
			break;
		case HPDF_INVALID_OPERATION:
			*msg = estrdup("Invalid operation, cannot perform the requested action");
			break;
		case HPDF_INVALID_OUTLINE:
			*msg = estrdup("An invalid outline-handle was specified");
			break;
		case HPDF_INVALID_PAGE:
			*msg = estrdup("An invalid page-handle was specified");
			break;
		case HPDF_INVALID_PAGES:
			*msg = estrdup("An invalid pages-handle was specified");
			break;
		case HPDF_INVALID_PARAMETER:
			*msg = estrdup("An invalid value is set");
			break;
		case HPDF_INVALID_PNG_IMAGE:
			*msg = estrdup("Invalid PNG image format");
			break;
		case HPDF_MISSING_FILE_NAME_ENTRY:
			*msg = estrdup("libharu internal error. The _FILE_NAME entry for delayed loading is missing");
			break;
		case HPDF_INVALID_TTC_FILE:
			*msg = estrdup("Invalid .TTC file format");
			break;
		case HPDF_INVALID_TTC_INDEX:
			*msg = estrdup("The index parameter exceeds the number of included fonts");
			break;
		case HPDF_INVALID_WX_DATA:
			*msg = estrdup("Cannot read a width-data from an afm file");
			break;
		case HPDF_LIBPNG_ERROR:
			*msg = estrdup("An error has returned from PNGLIB while loading an image");
			break;
		case HPDF_PAGE_CANNOT_RESTORE_GSTATE:
			*msg = estrdup("There are no graphics-states to be restored");
			break;
		case HPDF_PAGE_FONT_NOT_FOUND:
			*msg = estrdup("The current font is not set");
			break;
		case HPDF_PAGE_INVALID_FONT:
			*msg = estrdup("An invalid font-handle was specified");
			break;
		case HPDF_PAGE_INVALID_FONT_SIZE:
			*msg = estrdup("An invalid font-size was set");
			break;
		case HPDF_PAGE_INVALID_GMODE:
			*msg = estrdup("Invalid graphics mode");
			break;
		case HPDF_PAGE_INVALID_ROTATE_VALUE:
			*msg = estrdup("The specified value is not a multiple of 90");
			break;
		case HPDF_PAGE_INVALID_SIZE:
			*msg = estrdup("An invalid page-size was set");
			break;
		case HPDF_PAGE_INVALID_XOBJECT:
			*msg = estrdup("An invalid image-handle was set");
			break;
		case HPDF_PAGE_OUT_OF_RANGE:
			*msg = estrdup("The specified value is out of range");
			break;
		case HPDF_REAL_OUT_OF_RANGE:
			*msg = estrdup("The specified value is out of range");
			break;
		case HPDF_STREAM_EOF:
			*msg = estrdup("Unexpected EOF marker was detected");
			break;
		case HPDF_STRING_OUT_OF_RANGE:
			*msg = estrdup("The length of the specified text is too big");
			break;
		case HPDF_THIS_FUNC_WAS_SKIPPED:
			*msg = estrdup("The execution of a function was skipped because of other errors");
			break;
		case HPDF_TTF_CANNOT_EMBEDDING_FONT:
			*msg = estrdup("This font cannot be embedded (restricted by license)");
			break;
		case HPDF_TTF_INVALID_CMAP:
			*msg = estrdup("Unsupported or invalid ttf format (cannot find unicode cmap)");
			break;
		case HPDF_TTF_INVALID_FOMAT:
			*msg = estrdup("Unsupported or invalid ttf format");
			break;
		case HPDF_TTF_MISSING_TABLE:
			*msg = estrdup("Unsupported or invalid ttf format (cannot find a necessary table)");
			break;
		case HPDF_UNSUPPORTED_FUNC:
			*msg = estrdup("The library is not configured to use PNGLIB or internal error occurred");
			break;
		case HPDF_UNSUPPORTED_JPEG_FORMAT:
			*msg = estrdup("Unsupported or invalid JPEG format");
			break;
		case HPDF_UNSUPPORTED_TYPE1_FONT:
			*msg = estrdup("Failed to parse .PFB file");
			break;
		case HPDF_ZLIB_ERROR:
			*msg = estrdup("An error has occurred while executing a function of Zlib");
			break;
		case HPDF_INVALID_PAGE_INDEX:
			*msg = estrdup("An error returned from Zlib");
			break;
		case HPDF_INVALID_URI:
			*msg = estrdup("An invalid URI was set");
			break;
		case HPDF_ANNOT_INVALID_ICON:
			*msg = estrdup("An invalid icon was set");
			break;
		case HPDF_ANNOT_INVALID_BORDER_STYLE:
			*msg = estrdup("An invalid border-style was set");
			break;
		case HPDF_PAGE_INVALID_DIRECTION:
			*msg = estrdup("An invalid page-direction was set");
			break;
		case HPDF_INVALID_FONT:
			*msg = estrdup("An invalid font-handle was specified");
			break;
		case HPDF_PAGE_INSUFFICIENT_SPACE:
			*msg = estrdup("Insufficient space for text");
			break;
		default:
			*msg = estrdup("Unknown error occurred, please report");
			break;
	}
	return 1;
}

/* }}} */

static int php_haru_status_to_exception(HPDF_STATUS status TSRMLS_DC) /* {{{ */
{
	if (status != HPDF_OK) {
		char *msg;
		php_haru_status_to_errmsg(status, &msg);
		zend_throw_exception_ex(ce_haruexception, status TSRMLS_CC, msg);
		efree(msg);
		return 1;
	}
	return 0;
}

/* }}} */

static int php_haru_check_error(HPDF_Error error TSRMLS_DC) /* {{{ */
{
	HPDF_STATUS status = HPDF_CheckError(error);

	return php_haru_status_to_exception(status TSRMLS_CC);
}

/* }}} */

static int php_haru_check_doc_error(php_harudoc *doc TSRMLS_DC) /* {{{ */
{
	HPDF_STATUS status = HPDF_GetError(doc->h);

	return php_haru_status_to_exception(status TSRMLS_CC);
}

/* }}} */

static HPDF_Rect php_haru_array_to_rect(zval *array) /* {{{ */
{
	int i = 0;
	zval **element, tmp, tmp_element;
	HPDF_Rect r;

	for (zend_hash_internal_pointer_reset(Z_ARRVAL_P(array));
			zend_hash_get_current_data(Z_ARRVAL_P(array), (void **) &element) == SUCCESS;
			zend_hash_move_forward(Z_ARRVAL_P(array))) {
		if (Z_TYPE_PP(element) != IS_DOUBLE) {
			tmp = **element;
			zval_copy_ctor(&tmp);
			INIT_PZVAL(&tmp);
			convert_to_double(&tmp);
			tmp_element = tmp;
		} else {
			tmp_element = **element;
		}

		switch(i) {
			case 0:
				r.left = Z_DVAL(tmp_element);
				break;
			case 1:
				r.bottom = Z_DVAL(tmp_element);
				break;
			case 2:
				r.right = Z_DVAL(tmp_element);
				break;
			case 3:
				r.top = Z_DVAL(tmp_element);
				break;
		}

		if (Z_TYPE_PP(element) != IS_DOUBLE) {
			zval_dtor(&tmp);
		}
		i++;
	}

	return r;
}
/* }}} */

/* }}} */

/* HaruDoc methods {{{ */

/* {{{ proto void HaruDoc::__construct()
 Construct new HaruDoc instance */
static PHP_METHOD(HaruDoc, __construct)
{
	zval *object = getThis();
	php_harudoc *doc;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "")) {
		return;
	}

	doc = (php_harudoc *)zend_object_store_get_object(object TSRMLS_CC);

	if (doc->h) {
		/* called __construct() twice, bail out */
		return;
	}

	doc->h = HPDF_New(NULL, NULL);

	PHP_HARU_NULL_CHECK(doc->h, "Cannot create HaruDoc handle");
}
/* }}} */

/* {{{ proto bool HaruDoc::resetError()
 Reset error state in the document handle */
static PHP_METHOD(HaruDoc, resetError)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	HPDF_ResetError(doc->h);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto object HaruDoc::addPage()
 Add new page to the document */
static PHP_METHOD(HaruDoc, addPage)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_harupage *page;
	HPDF_Page p;

	p = HPDF_AddPage(doc->h);

	if (php_haru_check_doc_error(doc TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(p, "Cannot create HaruPage handle");

	object_init_ex(return_value, ce_harupage);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);
	page = (php_harupage *)zend_object_store_get_object(return_value TSRMLS_CC);

	page->doc = *getThis();
	page->h = p;

	zend_objects_store_add_ref(getThis() TSRMLS_CC);
}
/* }}} */

/* {{{ proto object HaruDoc::insertPage(object page)
 Insert new page just before the specified page */
static PHP_METHOD(HaruDoc, insertPage)
{
	zval *z_page;
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_harupage *target, *page;
	HPDF_Page p;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &z_page, ce_harupage) == FAILURE) {
		return;
	}

	target = (php_harupage *)zend_object_store_get_object(z_page TSRMLS_CC);

	p = HPDF_InsertPage(doc->h, target->h);

	if (php_haru_check_doc_error(doc TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(p, "Cannot create HaruPage handle");

	object_init_ex(return_value, ce_harupage);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);

	page = (php_harupage *)zend_object_store_get_object(return_value TSRMLS_CC);

	page->doc = *getThis();
	page->h = p;

	zend_objects_store_add_ref(getThis() TSRMLS_CC);
}
/* }}} */

/* {{{ proto object HaruDoc::getCurrentPage()
 Return current page of the document */
static PHP_METHOD(HaruDoc, getCurrentPage)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_harupage *page;
	HPDF_Page p;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	p = HPDF_GetCurrentPage(doc->h);

	if (php_haru_check_doc_error(doc TSRMLS_CC)) {
		return;
	}

	if (!p) {
		/* no current page is not an error */
		RETURN_FALSE;
	}

	object_init_ex(return_value, ce_harupage);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);

	page = (php_harupage *)zend_object_store_get_object(return_value TSRMLS_CC);

	page->doc = *getThis();
	page->h = p;

	zend_objects_store_add_ref(getThis() TSRMLS_CC);
}
/* }}} */

/* {{{ proto object HaruDoc::getEncoder(string encoding)
 Return HaruEncoder instance with the specified encoding */
static PHP_METHOD(HaruDoc, getEncoder)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_haruencoder *encoder;
	HPDF_Encoder e;
	char *enc;
	int enc_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &enc, &enc_len) == FAILURE) {
		return;
	}

	e = HPDF_GetEncoder(doc->h, (const char *)enc);

	if (php_haru_check_doc_error(doc TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(e, "Cannot create HaruEncoder handle");

	object_init_ex(return_value, ce_haruencoder);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);

	encoder = (php_haruencoder *)zend_object_store_get_object(return_value TSRMLS_CC);

	encoder->doc = *getThis();
	encoder->h = e;

	zend_objects_store_add_ref(getThis() TSRMLS_CC);
}
/* }}} */

/* {{{ proto object HaruDoc::getCurrentEncoder()
 Return HaruEncoder currently used in the document */
static PHP_METHOD(HaruDoc, getCurrentEncoder)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_haruencoder *encoder;
	HPDF_Encoder e;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	e = HPDF_GetCurrentEncoder(doc->h);

	if (php_haru_check_doc_error(doc TSRMLS_CC)) {
		return;
	}

	if (!e) {
		/* no encoder set */
		RETURN_FALSE;
	}

	object_init_ex(return_value, ce_haruencoder);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);

	encoder = (php_haruencoder *)zend_object_store_get_object(return_value TSRMLS_CC);

	encoder->doc = *getThis();
	encoder->h = e;

	zend_objects_store_add_ref(getThis() TSRMLS_CC);
}
/* }}} */

/* {{{ proto bool HaruDoc::setCurrentEncoder(string encoding)
 Set the current encoder for the document */
static PHP_METHOD(HaruDoc, setCurrentEncoder)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	char *enc;
	int enc_len;
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &enc, &enc_len) == FAILURE) {
		return;
	}

	status = HPDF_SetCurrentEncoder(doc->h, (const char *)enc);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::save(string file)
 Save the document into the specified file */
static PHP_METHOD(HaruDoc, save)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	char *filename;
	int filename_len;
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &filename, &filename_len) == FAILURE) {
		return;
	}

	HARU_CHECK_FILE(filename);

	status = HPDF_SaveToFile(doc->h, filename);
	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::output()
 Write the document data to the output buffer */
static PHP_METHOD(HaruDoc, output)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	HPDF_UINT32 size, requested_bytes;
	char *buffer;
	unsigned int buffer_size;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_SaveToStream(doc->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}

	size = HPDF_GetStreamSize(doc->h);

	if (!size) {
		zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Zero stream size, the PDF document contains no data");
		return;
	}

	buffer_size = (size > PHP_HARU_BUF_SIZE) ? PHP_HARU_BUF_SIZE : size;
	buffer = emalloc(buffer_size + 1);

	while (size > 0) {
		requested_bytes = buffer_size;

		status = HPDF_ReadFromStream(doc->h, (HPDF_BYTE *)buffer, &requested_bytes);
		if (status != HPDF_STREAM_EOF && php_haru_status_to_exception(status TSRMLS_CC)) {
			efree(buffer);
			return;
		}

		if (requested_bytes > 0) {
			PHPWRITE(buffer, requested_bytes);
			size -= requested_bytes;
		}

		if (status == HPDF_STREAM_EOF) {
			/* reached the end of the stream */
			break;
		}
	}
	efree(buffer);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::saveToStream()
 Save the document data to a temporary stream */
static PHP_METHOD(HaruDoc, saveToStream)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_SaveToStream(doc->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::resetStream()
 Rewind the temporary stream */
static PHP_METHOD(HaruDoc, resetStream)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_ResetStream(doc->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::getStreamSize()
 Get the size of the temporary stream */
static PHP_METHOD(HaruDoc, getStreamSize)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_UINT32 size;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	size = HPDF_GetStreamSize(doc->h);

	RETURN_LONG((long)size);
}
/* }}} */

/* {{{ proto bool HaruDoc::readFromStream(int bytes)
 Reads data from the temporary stream */
static PHP_METHOD(HaruDoc, readFromStream)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	long size;
	HPDF_UINT32 requested_bytes;
	char *buffer;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &size) == FAILURE) {
		return;
	}

	if (size <= 0 || (size + 1) <= 0) {
		zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "number of bytes must be greater than zero");
		return;
	}

	buffer = safe_emalloc(size, 1, 1);
	requested_bytes = size;

	status = HPDF_ReadFromStream(doc->h, (HPDF_BYTE *)buffer, &requested_bytes);

	if (status != HPDF_STREAM_EOF && php_haru_status_to_exception(status TSRMLS_CC)) {
		efree(buffer);
		return;
	}

	if (requested_bytes > 0) {
		buffer[requested_bytes] = '\0';
		RETURN_STRINGL(buffer, requested_bytes, 0);
	}
	efree(buffer);
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool HaruDoc::setPageLayout(int layout)
 Set how pages should be displayed */
static PHP_METHOD(HaruDoc, setPageLayout)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	long layout;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &layout) == FAILURE) {
		return;
	}

	switch(layout) {
		case HPDF_PAGE_LAYOUT_SINGLE:
		case HPDF_PAGE_LAYOUT_ONE_COLUMN:
		case HPDF_PAGE_LAYOUT_TWO_COLUMN_LEFT:
		case HPDF_PAGE_LAYOUT_TWO_COLUMN_RIGHT:
			/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid page layout value");
			return;
	}

	status = HPDF_SetPageLayout(doc->h, (HPDF_PageLayout)layout);
	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int HaruDoc::getPageLayout()
 Return current page layout */
static PHP_METHOD(HaruDoc, getPageLayout)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_PageLayout layout;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	layout = HPDF_GetPageLayout(doc->h);
	if (layout == HPDF_PAGE_LAYOUT_EOF) {
		RETURN_FALSE;
	}
	RETURN_LONG((long)layout);
}
/* }}} */

/* {{{ proto bool HaruDoc::setPageMode(int mode)
 Set how the document should be displayed */
static PHP_METHOD(HaruDoc, setPageMode)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	long mode;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &mode) == FAILURE) {
		return;
	}

	switch(mode) {
		case HPDF_PAGE_MODE_USE_NONE:
		case HPDF_PAGE_MODE_USE_OUTLINE:
		case HPDF_PAGE_MODE_USE_THUMBS:
		case HPDF_PAGE_MODE_FULL_SCREEN:
			/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid page mode value");
			return;
	}

	status = HPDF_SetPageMode(doc->h, (HPDF_PageMode)mode);
	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int HaruDoc::getPageMode()
 Return current page mode */
static PHP_METHOD(HaruDoc, getPageMode)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_PageMode mode;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	mode = HPDF_GetPageMode(doc->h);
	RETURN_LONG((long)mode);
}
/* }}} */

/* {{{ proto bool HaruDoc::setInfoAttr(int type, string info)
 Set the info attributes of the document */
static PHP_METHOD(HaruDoc, setInfoAttr)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	long type;
	char *info;
	int info_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ls", &type, &info, &info_len) == FAILURE) {
		return;
	}

	switch(type) {
		case HPDF_INFO_AUTHOR:
		case HPDF_INFO_CREATOR:
		case HPDF_INFO_TITLE:
		case HPDF_INFO_SUBJECT:
		case HPDF_INFO_KEYWORDS:
			/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid info attribute type value");
			return;
	}

	status = HPDF_SetInfoAttr(doc->h, (HPDF_InfoType)type, (const char *)info);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HaruDoc::getInfoAttr(int type)
 Get current value of the specified document attribute */
static PHP_METHOD(HaruDoc, getInfoAttr)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	long type;
	const char *info;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &type) == FAILURE) {
		return;
	}

	switch(type) {
		case HPDF_INFO_CREATION_DATE:
		case HPDF_INFO_MOD_DATE:
		case HPDF_INFO_AUTHOR:
		case HPDF_INFO_CREATOR:
		case HPDF_INFO_TITLE:
		case HPDF_INFO_SUBJECT:
		case HPDF_INFO_KEYWORDS:
			/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid info attribute type value");
			return;
	}

	info = HPDF_GetInfoAttr(doc->h, (HPDF_InfoType)type);

	if (php_haru_check_doc_error(doc TSRMLS_CC)) {
		return;
	}

	if (!info) { /* no error, it's just not set */
		RETURN_EMPTY_STRING();
	}
	RETURN_STRING((char *)info, 1);
}
/* }}} */

/* {{{ proto bool HaruDoc::setInfoDateAttr(int type, int year, int month, int day, int hour, int min, int sec, string ind, int off_hour, int off_min)
 Set the datetime info attributes of the document */
static PHP_METHOD(HaruDoc, setInfoDateAttr)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	long type, year, month, day, hour, min, sec, off_hour, off_min;
	char *ind;
	int ind_len;
	HPDF_Date value;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lllllllsll", &type, &year, &month, &day, &hour, &min, &sec, &ind, &ind_len, &off_hour, &off_min) == FAILURE) {
		return;
	}

	switch(type) {
		case HPDF_INFO_CREATION_DATE:
		case HPDF_INFO_MOD_DATE:
			/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid datetime info attribute type value");
			return;
	}

	value.year		= (HPDF_INT) year;
	value.month		= (HPDF_INT) month;
	value.day		= (HPDF_INT) day;
	value.hour		= (HPDF_INT) hour;
	value.minutes	= (HPDF_INT) min;
	value.seconds	= (HPDF_INT) sec;
	value.ind		= (ind[0] == 0) ? 32 /* ' ' */ : ind[0]; /* make libharu happy with empty ind */
	value.off_hour	= (HPDF_INT) off_hour;
	value.off_minutes = (HPDF_INT) off_min;

	status = HPDF_SetInfoDateAttr(doc->h, (HPDF_InfoType)type, value);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto object HaruDoc::getFont(string fontname[, string encoding ])
 Create and return HaruFont instance */
static PHP_METHOD(HaruDoc, getFont)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	char *fontname, *encoding = NULL;
	int fontname_len, encoding_len = 0;
	HPDF_Font f;
	php_harufont *font;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &fontname, &fontname_len, &encoding, &encoding_len) == FAILURE) {
		return;
	}

	if (encoding_len == 0) {
		encoding = NULL;
	}

	f = HPDF_GetFont(doc->h, (const char *)fontname, (const char*)encoding);

	if (php_haru_check_doc_error(doc TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(f, "Cannot create HaruFont handle");

	object_init_ex(return_value, ce_harufont);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);

	font = (php_harufont*)zend_object_store_get_object(return_value TSRMLS_CC);

	font->doc = *getThis();
	font->h = f;

	zend_objects_store_add_ref(getThis() TSRMLS_CC);
}
/* }}} */

/* {{{ proto string HaruDoc::loadTTF(string fontfile[, bool embed])
 Load TTF font file */
static PHP_METHOD(HaruDoc, loadTTF)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	char *fontfile;
	int fontfile_len;
	zend_bool embed = 0;
	const char *name;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &fontfile, &fontfile_len, &embed) == FAILURE) {
		return;
	}

	HARU_CHECK_FILE(fontfile);

	name = HPDF_LoadTTFontFromFile(doc->h, (const char *)fontfile, (HPDF_BOOL)embed);

	if (php_haru_check_doc_error(doc TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(name, "Failed to load TTF font");

	RETURN_STRING((char *)name, 1);
}
/* }}} */

/* {{{ proto string HaruDoc::loadTTC(string fontfile, int index[, bool embed ])
 Load font with the speicifed index from TTC file */
static PHP_METHOD(HaruDoc, loadTTC)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	char *fontfile;
	int fontfile_len;
	zend_bool embed = 0;
	const char *name;
	long index;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl|b", &fontfile, &fontfile_len, &index, &embed) == FAILURE) {
		return;
	}

	HARU_CHECK_FILE(fontfile);

	name = HPDF_LoadTTFontFromFile2(doc->h, (const char *)fontfile, (HPDF_UINT)index, (HPDF_BOOL)embed);

	if (php_haru_check_doc_error(doc TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(name, "Failed to load TTF font from the font collection");

	RETURN_STRING((char *)name, 1);
}
/* }}} */

/* {{{ proto string HaruDoc::loadType1(string afmfile[, string pfmfile])
 Load Type1 font */
static PHP_METHOD(HaruDoc, loadType1)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	char *afmfile, *pfmfile = NULL;
	int afmfile_len, pfmfile_len = 0;
	const char *name;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &afmfile, &afmfile_len, &pfmfile, &pfmfile_len) == FAILURE) {
		return;
	}

	HARU_CHECK_FILE(afmfile);

	if (pfmfile_len == 0) {
		pfmfile = NULL;
	} else {
		HARU_CHECK_FILE(pfmfile);
	}

	name = HPDF_LoadType1FontFromFile(doc->h, (const char *)afmfile, (const char *)pfmfile);

	if (php_haru_check_doc_error(doc TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(name, "Failed to load Type1 font");

	RETURN_STRING((char *)name, 1);
}
/* }}} */

/* {{{ proto object HaruDoc::loadPNG(string filename[, bool deferred])
 Load PNG image and return HaruImage instance */
static PHP_METHOD(HaruDoc, loadPNG)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_haruimage *image;
	HPDF_Image i;
	char *filename;
	int filename_len;
	zend_bool deferred = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &filename, &filename_len, &deferred) == FAILURE) {
		return;
	}

	HARU_CHECK_FILE(filename);

	if (deferred) {
		i = HPDF_LoadPngImageFromFile2(doc->h, (const char*)filename);
	} else {
		/* default */
		i = HPDF_LoadPngImageFromFile(doc->h, (const char*)filename);
	}

	if (php_haru_check_doc_error(doc TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(i, "Failed to load PNG image");

	object_init_ex(return_value, ce_haruimage);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);

	image = (php_haruimage *)zend_object_store_get_object(return_value TSRMLS_CC);

	image->doc = *getThis();
	image->h = i;
	image->filename = estrndup(filename, filename_len);

	zend_objects_store_add_ref(getThis() TSRMLS_CC);
}
/* }}} */

/* {{{ proto object HaruDoc::loadJPEG(string filename)
 Load JPEG image and return HaruImage instance */
static PHP_METHOD(HaruDoc, loadJPEG)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_haruimage *image;
	HPDF_Image i;
	char *filename;
	int filename_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &filename, &filename_len) == FAILURE) {
		return;
	}

	HARU_CHECK_FILE(filename);

	i = HPDF_LoadJpegImageFromFile(doc->h, (const char*)filename);

	if (php_haru_check_doc_error(doc TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(i, "Failed to load JPEG image");

	object_init_ex(return_value, ce_haruimage);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);

	image = (php_haruimage *)zend_object_store_get_object(return_value TSRMLS_CC);

	image->doc = *getThis();
	image->h = i;
	image->filename = estrndup(filename, filename_len);

	zend_objects_store_add_ref(getThis() TSRMLS_CC);
}
/* }}} */

/* {{{ proto object HaruDoc::loadRaw(string filename, int width, int height, int color_space)
 Load RAW image and return HaruImage instance */
static PHP_METHOD(HaruDoc, loadRaw)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_haruimage *image;
	HPDF_Image i;
	char *filename;
	int filename_len;
	long width, height, color_space;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slll", &filename, &filename_len, &width, &height, &color_space) == FAILURE) {
		return;
	}

	HARU_CHECK_FILE(filename);

	switch(color_space) {
		case HPDF_CS_DEVICE_GRAY:
		case HPDF_CS_DEVICE_RGB:
		case HPDF_CS_DEVICE_CMYK:
			/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid color_space parameter value");
			return;
	}

	i = HPDF_LoadRawImageFromFile(doc->h, (const char *)filename, width, height, (HPDF_ColorSpace)color_space);

	if (php_haru_check_doc_error(doc TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(i, "Failed to load RAW image");

	object_init_ex(return_value, ce_haruimage);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);

	image = (php_haruimage *)zend_object_store_get_object(return_value TSRMLS_CC);

	image->doc = *getThis();
	image->h = i;
	image->filename = estrndup(filename, filename_len);

	zend_objects_store_add_ref(getThis() TSRMLS_CC);
}
/* }}} */

/* {{{ proto bool HaruDoc::setPassword(string owner_password, string user_password)
 Set owner and user passwords for the document */
static PHP_METHOD(HaruDoc, setPassword)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	char *owner_pswd, *user_pswd;
	int owner_pswd_len, user_pswd_len;
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &owner_pswd, &owner_pswd_len, &user_pswd, &user_pswd_len) == FAILURE) {
		return;
	}

	status = HPDF_SetPassword(doc->h, (const char *)owner_pswd, (const char *)user_pswd);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::setPermission(int permission)
 Set permissions for the document */
static PHP_METHOD(HaruDoc, setPermission)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	long permission;
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &permission) == FAILURE) {
		return;
	}

	status = HPDF_SetPermission(doc->h, (HPDF_UINT)permission);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::setEncryptionMode(int mode[, int key_len])
 Set encryption mode for the document */
static PHP_METHOD(HaruDoc, setEncryptionMode)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	long mode, key_len = 5;
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|l", &mode, &key_len) == FAILURE) {
		return;
	}

	switch (mode) {
		case HPDF_ENCRYPT_R2:
		case HPDF_ENCRYPT_R3:
			/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid encrypt mode value");
			return;
	}

	status = HPDF_SetEncryptionMode(doc->h, (HPDF_EncryptMode)mode, (HPDF_UINT)key_len);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::setCompressionMode(int mode)
 Set compression mode for the document */
static PHP_METHOD(HaruDoc, setCompressionMode)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	long mode;
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &mode) == FAILURE) {
		return;
	}

	status = HPDF_SetCompressionMode(doc->h, (HPDF_UINT)mode);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::setPagesConfiguration(int page_per_pages)
 Set the number of pages per set of pages object */
static PHP_METHOD(HaruDoc, setPagesConfiguration)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	long page_per_pages;
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &page_per_pages) == FAILURE) {
		return;
	}

	status = HPDF_SetPagesConfiguration(doc->h, (HPDF_UINT)page_per_pages);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto object HaruDoc::setOpenAction(object destination)
 Define which page is shown when the document is opened */
static PHP_METHOD(HaruDoc, setOpenAction)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_harudestination *dest;
	zval *destination;
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &destination, ce_harudestination) == FAILURE) {
		return;
	}

	dest = (php_harudestination *)zend_object_store_get_object(destination TSRMLS_CC);

	status = HPDF_SetOpenAction(doc->h, dest->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto object HaruDoc::createOutline(string title[, object parent_outline[, object encoder ] ])
 Create a HaruOutline instance */
static PHP_METHOD(HaruDoc, createOutline)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_haruoutline *o, *o_parent;
	php_haruencoder *e;
	HPDF_Outline outline, out_parent = NULL;
	HPDF_Encoder enc = NULL;
	zval *encoder = NULL, *parent = NULL;
	char *title;
	int title_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|O!O!", &title, &title_len, &parent, ce_haruoutline, &encoder, ce_haruencoder) == FAILURE) {
		return;
	}

	if (parent) {
		o_parent = (php_haruoutline *)zend_object_store_get_object(parent TSRMLS_CC);
		out_parent = o_parent->h;
	}

	if (encoder) {
		e = (php_haruencoder *)zend_object_store_get_object(encoder TSRMLS_CC);
		enc = e->h;
	}

	outline = HPDF_CreateOutline(doc->h, out_parent, (const char *)title, enc);

	if (php_haru_check_doc_error(doc TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(outline, "Cannot create HaruOutline handle");

	object_init_ex(return_value, ce_haruoutline);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);

	o = (php_haruoutline *)zend_object_store_get_object(return_value TSRMLS_CC);

	o->doc = *getThis();
	o->h = outline;

	zend_objects_store_add_ref(getThis() TSRMLS_CC);
}
/* }}} */

/* {{{ proto bool HaruDoc::addPageLabel(int first_page, int style, int first_num[, string prefix ])
 Set the numbering style for the specified range of the pages */
static PHP_METHOD(HaruDoc, addPageLabel)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	long first_page, style, first_num;
	char *prefix = NULL;
	int prefix_len = 0;
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lll|s", &first_page, &style, &first_num, &prefix, &prefix_len) == FAILURE) {
		return;
	}

	switch (style) {
		case HPDF_PAGE_NUM_STYLE_DECIMAL:
		case HPDF_PAGE_NUM_STYLE_UPPER_ROMAN:
		case HPDF_PAGE_NUM_STYLE_LOWER_ROMAN:
		case HPDF_PAGE_NUM_STYLE_UPPER_LETTERS:
		case HPDF_PAGE_NUM_STYLE_LOWER_LETTERS:
				/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid numbering mode");
			return;
	}

	if (!prefix_len) {
		prefix = NULL;
	}

	status = HPDF_AddPageLabel(doc->h, (HPDF_UINT)first_page, (HPDF_PageNumStyle)style, (HPDF_UINT)first_num, (const char *)prefix);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::useJPFonts()
 Enable builtin Japanese fonts */
static PHP_METHOD(HaruDoc, useJPFonts)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_UseJPFonts(doc->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::useJPEncodings()
 Enable Japanese encodings */
static PHP_METHOD(HaruDoc, useJPEncodings)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_UseJPEncodings(doc->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::useKRFonts()
 Enable builtin Korean fonts */
static PHP_METHOD(HaruDoc, useKRFonts)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_UseKRFonts(doc->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::useKREncodings()
 Enable Korean encodings */
static PHP_METHOD(HaruDoc, useKREncodings)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_UseKREncodings(doc->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::useCNSFonts()
 Enable builtin Chinese simplified fonts */
static PHP_METHOD(HaruDoc, useCNSFonts)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_UseCNSFonts(doc->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::useCNSEncodings()
 Enable Chinese simplified encodings */
static PHP_METHOD(HaruDoc, useCNSEncodings)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_UseCNSEncodings(doc->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::useCNTFonts()
 Enable builtin Chinese traditional fonts */
static PHP_METHOD(HaruDoc, useCNTFonts)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_UseCNTFonts(doc->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::useCNTEncodings()
 Enable Chinese traditional encodings */
static PHP_METHOD(HaruDoc, useCNTEncodings)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_UseCNTEncodings(doc->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDoc::useUTFEncodings()
 Enable Unicode encodings */
static PHP_METHOD(HaruDoc, useUTFEncodings)
{
	php_harudoc *doc = (php_harudoc *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_UseUTFEncodings(doc->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}

/* }}} */

/* HaruPage methods {{{ */

/* {{{ proto bool HaruPage::__construct()
 Dummy constructor */
static PHP_METHOD(HaruPage, __construct)
{
	return;
}
/* }}} */

/* {{{ proto bool HaruPage::drawImage(object image, double x, double y, double width, double height)
 Show image at the page */
static PHP_METHOD(HaruPage, drawImage)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_haruimage *image;
	zval *z_image;
	double x, y, width, height;
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Odddd", &z_image, ce_haruimage, &x, &y, &width, &height) == FAILURE) {
		return;
	}

	image = (php_haruimage *)zend_object_store_get_object(z_image TSRMLS_CC);

	status = HPDF_Page_DrawImage(page->h, image->h, (HPDF_REAL)x, (HPDF_REAL)y, (HPDF_REAL)width, (HPDF_REAL)height);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setLineWidth(double width)
 Set line width for the page */
static PHP_METHOD(HaruPage, setLineWidth)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double width;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &width) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetLineWidth(page->h, (HPDF_REAL)width);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setLineCap(int cap)
 Set the shape to be used at the ends of line */
static PHP_METHOD(HaruPage, setLineCap)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	long cap;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &cap) == FAILURE) {
		return;
	}

	switch (cap) {
		case HPDF_BUTT_END:
		case HPDF_ROUND_END:
		case HPDF_PROJECTING_SCUARE_END:
			/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid line cap value");
			return;
	}

	status = HPDF_Page_SetLineCap(page->h, (HPDF_LineCap)cap);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setLineJoin(int join)
 Set line join style for the page */
static PHP_METHOD(HaruPage, setLineJoin)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	long join;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &join) == FAILURE) {
		return;
	}

	switch (join) {
		case HPDF_MITER_JOIN:
		case HPDF_ROUND_JOIN:
		case HPDF_BEVEL_JOIN:
			/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid line cap value");
			return;
	}

	status = HPDF_Page_SetLineJoin(page->h, (HPDF_LineJoin)join);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setMiterLimit(double limit)
 Set the current value of the miter limit of the page */
static PHP_METHOD(HaruPage, setMiterLimit)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double limit;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &limit) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetMiterLimit(page->h, (HPDF_REAL)limit);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setDash(array pattern, int phase)
 Set the dash pattern for the page */
static PHP_METHOD(HaruPage, setDash)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	HPDF_UINT16 *pat = NULL;
	zval *pattern;
	int pat_num = 0;
	long phase;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a!l", &pattern, &phase) == FAILURE) {
		return;
	}

	if (pattern) {
		pat_num = zend_hash_num_elements(Z_ARRVAL_P(pattern));
		if (pat_num > 8) {
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "first parameter is expected to be array with at most 8 elements, %d given", pat_num);
			return;
		}
	}

	if (phase > pat_num) {
		zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "phase parameter cannot be greater than the number of elements in the pattern");
		return;
	}

	if (pat_num > 0) {
		zval **element, tmp, tmp_element;
		int i = 0;

		pat = emalloc(pat_num * sizeof(HPDF_UINT16)); /* safe */

		for (zend_hash_internal_pointer_reset(Z_ARRVAL_P(pattern));
				zend_hash_get_current_data(Z_ARRVAL_P(pattern), (void **) &element) == SUCCESS;
				zend_hash_move_forward(Z_ARRVAL_P(pattern))) {
			if (Z_TYPE_PP(element) != IS_LONG) {
				tmp = **element;
				zval_copy_ctor(&tmp);
				INIT_PZVAL(&tmp);
				convert_to_long(&tmp);
				tmp_element = tmp;
			} else {
				tmp_element = **element;
			}

			pat[i++] = Z_LVAL(tmp_element);

			if (Z_TYPE_PP(element) != IS_LONG) {
				zval_dtor(&tmp);
			}
		}
	}

	status = HPDF_Page_SetDash(page->h, (const HPDF_UINT16 *)pat, (HPDF_UINT)pat_num, (HPDF_UINT)phase);

	if (pat) {
		efree(pat);
	}

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setFlatness(double flatness)
 Set flatness for the page */
static PHP_METHOD(HaruPage, setFlatness)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double flatness;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &flatness) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetFlat(page->h, (HPDF_REAL)flatness);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setFontAndSize(object font, double size)
 Set font and fontsize for the page */
static PHP_METHOD(HaruPage, setFontAndSize)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_harufont *font;
	HPDF_STATUS status;
	double size;
	zval *z_font;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Od", &z_font, ce_harufont, &size) == FAILURE) {
		return;
	}

	font = (php_harufont *)zend_object_store_get_object(z_font TSRMLS_CC);

	status = HPDF_Page_SetFontAndSize(page->h, font->h, (HPDF_REAL)size);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setCharSpace(double char_space)
 Set character spacing for the page */
static PHP_METHOD(HaruPage, setCharSpace)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double char_space;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &char_space) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetCharSpace(page->h, (HPDF_REAL)char_space);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setWordSpace(double word_space)
 Set word spacing for the page */
static PHP_METHOD(HaruPage, setWordSpace)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double word_space;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &word_space) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetWordSpace(page->h, (HPDF_REAL)word_space);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setHorizontalScaling(double scaling)
 Set horizontal scaling for the page */
static PHP_METHOD(HaruPage, setHorizontalScaling)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double scaling;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &scaling) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetHorizontalScalling(page->h, (HPDF_REAL)scaling);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setTextLeading(double text_leading)
 Set text leading (line spacing) for the page */
static PHP_METHOD(HaruPage, setTextLeading)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double text_leading;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &text_leading) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetTextLeading(page->h, (HPDF_REAL)text_leading);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setTextRenderingMode(int mode)
 Set text rendering mode for the page */
static PHP_METHOD(HaruPage, setTextRenderingMode)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	long mode;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &mode) == FAILURE) {
		return;
	}

	switch (mode) {
		case HPDF_FILL:
		case HPDF_STROKE:
		case HPDF_FILL_THEN_STROKE:
		case HPDF_INVISIBLE:
		case HPDF_FILL_CLIPPING:
		case HPDF_STROKE_CLIPPING:
		case HPDF_FILL_STROKE_CLIPPING:
		case HPDF_CLIPPING:
			/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid line cap value");
			return;
	}

	status = HPDF_Page_SetTextRenderingMode(page->h, (HPDF_TextRenderingMode)mode);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setTextRise(double rise)
 */
static PHP_METHOD(HaruPage, setTextRise)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double rise;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &rise) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetTextRise(page->h, (HPDF_REAL)rise);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setGrayFill(double value)
 Set filling color for the page */
static PHP_METHOD(HaruPage, setGrayFill)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double val;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &val) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetGrayFill(page->h, (HPDF_REAL)val);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setGrayStroke(double value)
 Sets stroking color for the page */
static PHP_METHOD(HaruPage, setGrayStroke)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double val;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &val) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetGrayStroke(page->h, (HPDF_REAL)val);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setRGBFill(double r, double g, double b)
 Set filling color for the page */
static PHP_METHOD(HaruPage, setRGBFill)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double r, g, b;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ddd", &r, &g, &b) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetRGBFill(page->h, (HPDF_REAL)r, (HPDF_REAL)g, (HPDF_REAL)b);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setRGBStroke(double r, double g, double b)
 Set stroking color for the page */
static PHP_METHOD(HaruPage, setRGBStroke)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double r, g, b;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ddd", &r, &g, &b) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetRGBStroke(page->h, (HPDF_REAL)r, (HPDF_REAL)g, (HPDF_REAL)b);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setCMYKFill(double c, double m, double y, double k)
 Set filling color for the page */
static PHP_METHOD(HaruPage, setCMYKFill)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double c, m, y, k;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dddd", &c, &m, &y, &k) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetCMYKFill(page->h, (HPDF_REAL)c, (HPDF_REAL)m, (HPDF_REAL)y, (HPDF_REAL)k);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setCMYKStroke(double c, double m, double y, double k)
 Set stroking color for the page */
static PHP_METHOD(HaruPage, setCMYKStroke)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double c, m, y, k;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dddd", &c, &m, &y, &k) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetCMYKStroke(page->h, (HPDF_REAL)c, (HPDF_REAL)m, (HPDF_REAL)y, (HPDF_REAL)k);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::Concat(double a, double b, double c, double d, double x, double y)
 Concatenate current transformation matrix of the page and the specified matrix */
static PHP_METHOD(HaruPage, Concat)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double a, b, c, d, x, y;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dddddd", &a, &b, &c, &d, &x, &y) == FAILURE) {
		return;
	}

	status = HPDF_Page_Concat(page->h, (HPDF_REAL)a, (HPDF_REAL)b, (HPDF_REAL)c, (HPDF_REAL)d, (HPDF_REAL)x, (HPDF_REAL)y);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array HaruPage::getTransMatrix()
 Get the current transformation matrix of the page */
static PHP_METHOD(HaruPage, getTransMatrix)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_TransMatrix matrix;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	matrix = HPDF_Page_GetTransMatrix(page->h);

	array_init(return_value);
	add_assoc_double_ex(return_value, "a", sizeof("a"), matrix.a);
	add_assoc_double_ex(return_value, "b", sizeof("b"), matrix.b);
	add_assoc_double_ex(return_value, "c", sizeof("c"), matrix.c);
	add_assoc_double_ex(return_value, "d", sizeof("d"), matrix.d);
	add_assoc_double_ex(return_value, "x", sizeof("x"), matrix.x);
	add_assoc_double_ex(return_value, "y", sizeof("y"), matrix.y);
}
/* }}} */

/* {{{ proto bool HaruPage::setTextMatrix(double a, double b, double c, double d, double x, double y)
 Set the current text transformation matrix of the page */
static PHP_METHOD(HaruPage, setTextMatrix)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double a, b, c, d, x, y;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dddddd", &a, &b, &c, &d, &x, &y) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetTextMatrix(page->h, (HPDF_REAL)a, (HPDF_REAL)b, (HPDF_REAL)c, (HPDF_REAL)d, (HPDF_REAL)x, (HPDF_REAL)y);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		/* knock-knock, follow the white rabbit */
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array HaruPage::getTextMatrix()
 Get the current text transformation matrix of the page */
static PHP_METHOD(HaruPage, getTextMatrix)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_TransMatrix matrix;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	matrix = HPDF_Page_GetTextMatrix(page->h);

	array_init(return_value);
	add_assoc_double_ex(return_value, "a", sizeof("a"), matrix.a);
	add_assoc_double_ex(return_value, "b", sizeof("b"), matrix.b);
	add_assoc_double_ex(return_value, "c", sizeof("c"), matrix.c);
	add_assoc_double_ex(return_value, "d", sizeof("d"), matrix.d);
	add_assoc_double_ex(return_value, "x", sizeof("x"), matrix.x);
	add_assoc_double_ex(return_value, "y", sizeof("y"), matrix.y);
}
/* }}} */

/* {{{ proto bool HaruPage::moveTo(double x, double y)
 Set start point for new drawing path */
static PHP_METHOD(HaruPage, moveTo)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double x, y;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dd", &x, &y) == FAILURE) {
		return;
	}

	status = HPDF_Page_MoveTo(page->h, (HPDF_REAL)x, (HPDF_REAL)y);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::stroke([bool close_path])
 Paint current path */
static PHP_METHOD(HaruPage, stroke)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	zend_bool close_path = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &close_path) == FAILURE) {
		return;
	}

	if (!close_path) {
		status = HPDF_Page_Stroke(page->h);
	} else {
		status = HPDF_Page_ClosePathStroke(page->h);
	}

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::fill()
 Fill current path using nonzero winding number rule */
static PHP_METHOD(HaruPage, fill)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_Page_Fill(page->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::eofill()
 Fill current path using even-odd rule */
static PHP_METHOD(HaruPage, eofill)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_Page_Eofill(page->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::fillStroke([bool close_path])
 Fill current path using nonzero winding number rule, then paint the path */
static PHP_METHOD(HaruPage, fillStroke)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	zend_bool close_path = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &close_path) == FAILURE) {
		return;
	}

	if (!close_path) {
		status = HPDF_Page_FillStroke(page->h);
	} else {
		status = HPDF_Page_ClosePathFillStroke(page->h);
	}

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::eoFillStroke([bool close_path])
 Fill current path using even-odd rule, then paint the path */
static PHP_METHOD(HaruPage, eoFillStroke)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	zend_bool close_path = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &close_path) == FAILURE) {
		return;
	}

	if (!close_path) {
		status = HPDF_Page_EofillStroke(page->h);
	} else {
		status = HPDF_Page_ClosePathEofillStroke(page->h);
	}

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::closePath()
 Append a straight line from the current point to the start point of the path */
static PHP_METHOD(HaruPage, closePath)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_Page_ClosePath(page->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::endPath()
 End current path object without filling and painting operation */
static PHP_METHOD(HaruPage, endPath)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_Page_EndPath(page->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::lineTo(double x, double y)
 Append a line from the current point to the specified point to the current path */
static PHP_METHOD(HaruPage, lineTo)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double x, y;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dd", &x, &y) == FAILURE) {
		return;
	}

	status = HPDF_Page_LineTo(page->h, (HPDF_REAL)x, (HPDF_REAL)y);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::curveTo(double x1, double y1, double x2, double y2, double x3, double y3)
 Append a Bezier curve to the current path */
static PHP_METHOD(HaruPage, curveTo)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double x1, x2, x3, y1, y2, y3;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dddddd", &x1, &y1, &x2, &y2, &x3, &y3) == FAILURE) {
		return;
	}

	status = HPDF_Page_CurveTo(page->h, (HPDF_REAL)x1, (HPDF_REAL)y1, (HPDF_REAL)x2, (HPDF_REAL)y2, (HPDF_REAL)x3, (HPDF_REAL)y3);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::curveTo2(double x2, double y2, double x3, double y3)
 Append a Bezier curve to the current path */
static PHP_METHOD(HaruPage, curveTo2)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double x2, x3, y2, y3;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dddd", &x2, &y2, &x3, &y3) == FAILURE) {
		return;
	}

	status = HPDF_Page_CurveTo2(page->h, (HPDF_REAL)x2, (HPDF_REAL)y2, (HPDF_REAL)x3, (HPDF_REAL)y3);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::curveTo3(double x1, double y1, double x3, double y3)
 Append a Bezier curve to the current path */
static PHP_METHOD(HaruPage, curveTo3)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double x1, x3, y1, y3;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dddd", &x1, &y1, &x3, &y3) == FAILURE) {
		return;
	}

	status = HPDF_Page_CurveTo3(page->h, (HPDF_REAL)x1, (HPDF_REAL)y1, (HPDF_REAL)x3, (HPDF_REAL)y3);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::rectangle(double x, double y, double width, double height)
 Append a rectangle to the current path */
static PHP_METHOD(HaruPage, rectangle)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double x, y, width, height;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dddd", &x, &y, &width, &height) == FAILURE) {
		return;
	}

	status = HPDF_Page_Rectangle(page->h, (HPDF_REAL)x, (HPDF_REAL)y, (HPDF_REAL)width, (HPDF_REAL)height);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::arc(double x, double y, double ray, double ang1, double ang2)
 Append an arc to the current path */
static PHP_METHOD(HaruPage, arc)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double x, y, ray, ang1, ang2;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ddddd", &x, &y, &ray, &ang1, &ang2) == FAILURE) {
		return;
	}

	status = HPDF_Page_Arc(page->h, (HPDF_REAL)x, (HPDF_REAL)y, (HPDF_REAL)ray, (HPDF_REAL)ang1, (HPDF_REAL)ang2);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::circle(double x, double y, double ray)
 Append a circle to the current path */
static PHP_METHOD(HaruPage, circle)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double x, y, ray;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ddd", &x, &y, &ray) == FAILURE) {
		return;
	}

	status = HPDF_Page_Circle(page->h, (HPDF_REAL)x, (HPDF_REAL)y, (HPDF_REAL)ray);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::ellipse(double x, double y, double xray, double yray)
 Append an ellipse to the current path */
static PHP_METHOD(HaruPage, ellipse)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double x, y, xray, yray;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dddd", &x, &y, &xray, &yray) == FAILURE) {
		return;
	}

	status = HPDF_Page_Ellipse(page->h, (HPDF_REAL) x, (HPDF_REAL)y, (HPDF_REAL)xray, (HPDF_REAL)yray);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::showText(string text)
 Print text at the current position of the page */
static PHP_METHOD(HaruPage, showText)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	char *text;
	int text_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &text, &text_len) == FAILURE) {
		return;
	}

	status = HPDF_Page_ShowText(page->h, (const char*)text);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::showTextNextLine(string text)
 Move current position to the start of the next line and print the text */
static PHP_METHOD(HaruPage, showTextNextLine)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	char *text;
	int text_len;
	double word_space = 0, char_space = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|dd", &text, &text_len, &word_space, &char_space) == FAILURE) {
		return;
	}

	if (ZEND_NUM_ARGS() == 1) {
		status = HPDF_Page_ShowTextNextLine(page->h, (const char*)text);
	} else {
		status = HPDF_Page_ShowTextNextLineEx(page->h, (HPDF_REAL)word_space, (HPDF_REAL)char_space, (const char*)text);
	}

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::textOut(double x, double y, string text)
 Print the text on the specified position */
static PHP_METHOD(HaruPage, textOut)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double x, y;
	char *text;
	int text_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dds", &x, &y, &text, &text_len) == FAILURE) {
		return;
	}

	status = HPDF_Page_TextOut(page->h, (HPDF_REAL)x, (HPDF_REAL)y, (const char*)text);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::beginText()
 Begin a text object and set the current text position to (0,0) */
static PHP_METHOD(HaruPage, beginText)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_Page_BeginText(page->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::endText()
 End current text object */
static PHP_METHOD(HaruPage, endText)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_Page_EndText(page->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::textRect(double left, double top, double right, double bottom, string text[, int align ])
 Print the text inside the specified region */
static PHP_METHOD(HaruPage, textRect)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double left, top, right, bottom;
	char *str;
	int str_len;
	long align = HPDF_TALIGN_LEFT;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dddds|l", &left, &top, &right, &bottom, &str, &str_len, &align) == FAILURE) {
		return;
	}

	switch(align) {
		case HPDF_TALIGN_LEFT:
		case HPDF_TALIGN_RIGHT:
		case HPDF_TALIGN_CENTER:
		case HPDF_TALIGN_JUSTIFY:
			/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid align value");
			return;
	}

	status = HPDF_Page_TextRect(page->h, (HPDF_REAL)left, (HPDF_REAL)top, (HPDF_REAL)right, (HPDF_REAL)bottom, (const char *)str, (HPDF_TextAlignment) align, NULL);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::moveTextPos(double x, double y[, bool set_leading ])
 Move text position to the specified offset */
static PHP_METHOD(HaruPage, moveTextPos)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double x, y;
	zend_bool set_leading = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dd|b", &x, &y, &set_leading) == FAILURE) {
		return;
	}

	if (!set_leading) {
		/* default */
		status = HPDF_Page_MoveTextPos(page->h, (HPDF_REAL)x, (HPDF_REAL)y);
	} else {
		status = HPDF_Page_MoveTextPos2(page->h, (HPDF_REAL)x, (HPDF_REAL)y);
	}

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::moveToNextLine()
 Move text position to the start of the next line */
static PHP_METHOD(HaruPage, moveToNextLine)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_Page_MoveToNextLine(page->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setWidth(double width)
 Set width of the page */
static PHP_METHOD(HaruPage, setWidth)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double width;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &width) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetWidth(page->h, (HPDF_REAL)width);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setHeight(double height)
 Set height of the page */
static PHP_METHOD(HaruPage, setHeight)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double height;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &height) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetHeight(page->h, (HPDF_REAL)height);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setSize(int size, int direction)
 Set size and direction of the page */
static PHP_METHOD(HaruPage, setSize)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	long size, direction;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ll", &size, &direction) == FAILURE) {
		return;
	}

	switch(size) {
		case HPDF_PAGE_SIZE_LETTER:
		case HPDF_PAGE_SIZE_LEGAL:
		case HPDF_PAGE_SIZE_A3:
		case HPDF_PAGE_SIZE_A4:
		case HPDF_PAGE_SIZE_A5:
		case HPDF_PAGE_SIZE_B4:
		case HPDF_PAGE_SIZE_B5:
		case HPDF_PAGE_SIZE_EXECUTIVE:
		case HPDF_PAGE_SIZE_US4x6:
		case HPDF_PAGE_SIZE_US4x8:
		case HPDF_PAGE_SIZE_US5x7:
		case HPDF_PAGE_SIZE_COMM10:
		/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid page size value");
		return;
	}

	switch(direction) {
		case HPDF_PAGE_PORTRAIT:
		case HPDF_PAGE_LANDSCAPE:
		/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid page size value");
		return;
	}

	status = HPDF_Page_SetSize(page->h, (HPDF_PageSizes)size, (HPDF_PageDirection)direction);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::setRotate(int angle)
 Set rotation angle of the page */
static PHP_METHOD(HaruPage, setRotate)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	long angle;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &angle) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetRotate(page->h, (HPDF_UINT16)angle);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto double HaruPage::getWidth()
 Get width of the page */
static PHP_METHOD(HaruPage, getWidth)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_REAL width;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	width = HPDF_Page_GetWidth(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}
	RETURN_DOUBLE((double)width);
}
/* }}} */

/* {{{ proto double HaruPage::getHeight()
 Get height of the page */
static PHP_METHOD(HaruPage, getHeight)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_REAL height;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	height = HPDF_Page_GetHeight(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}
	RETURN_DOUBLE((double)height);
}
/* }}} */

/* {{{ proto object HaruPage::createDestination()
 Create and return new HaruDestination instance */
static PHP_METHOD(HaruPage, createDestination)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_harudestination *destination;
	HPDF_Destination dest;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	dest = HPDF_Page_CreateDestination(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(dest, "Cannot create HaruDestination handle");

	object_init_ex(return_value, ce_harudestination);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);

	destination = (php_harudestination *)zend_object_store_get_object(return_value TSRMLS_CC);

	destination->page = *getThis();
	destination->h = dest;

	zend_objects_store_add_ref(getThis() TSRMLS_CC);
}
/* }}} */

/* {{{ proto object HaruPage::createTextAnnotation(array rectangle, string text[, object encoder ])
 Create and return new HaruAnnotation instance */
static PHP_METHOD(HaruPage, createTextAnnotation)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_haruannotation *annotation;
	php_haruencoder *enc;
	HPDF_Annotation ann;
	HPDF_Rect r;
	HPDF_Encoder e = NULL;
	zval *rect, *encoder = NULL;
	char *text;
	int text_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "as|O", &rect, &text, &text_len, &encoder, ce_haruencoder) == FAILURE) {
		return;
	}

	if (zend_hash_num_elements(Z_ARRVAL_P(rect)) != 4) {
		zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Rectangle parameter is expected to be an array with exactly 4 elements");
		return;
	}

	r = php_haru_array_to_rect(rect);

	if (encoder) {
		enc = (php_haruencoder *)zend_object_store_get_object(encoder TSRMLS_CC);
		e = enc->h;
	}

	ann = HPDF_Page_CreateTextAnnot(page->h, r, (const char *)text, e);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(ann, "Cannot create HaruAnnotation handle");

	object_init_ex(return_value, ce_haruannotation);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);

	annotation = (php_haruannotation *)zend_object_store_get_object(return_value TSRMLS_CC);

	annotation->page = *getThis();
	annotation->h = ann;

	zend_objects_store_add_ref(getThis() TSRMLS_CC);
}
/* }}} */

/* {{{ proto object HaruPage::createLinkAnnotation(array rectangle, object destination)
 Create and return new HaruAnnotation instance */
static PHP_METHOD(HaruPage, createLinkAnnotation)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_haruannotation *annotation;
	php_harudestination *dest;
	HPDF_Annotation ann;
	HPDF_Rect r;
	zval *rect, *destination;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "aO", &rect, &destination, ce_harudestination) == FAILURE) {
		return;
	}

	if (zend_hash_num_elements(Z_ARRVAL_P(rect)) != 4) {
		zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Rectangle parameter is expected to be an array with exactly 4 elements");
		return;
	}

	r = php_haru_array_to_rect(rect);

	dest = (php_harudestination *)zend_object_store_get_object(destination TSRMLS_CC);

	ann = HPDF_Page_CreateLinkAnnot(page->h, r, dest->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(ann, "Cannot create HaruAnnotation handle");

	object_init_ex(return_value, ce_haruannotation);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);

	annotation = (php_haruannotation *)zend_object_store_get_object(return_value TSRMLS_CC);

	annotation->page = *getThis();
	annotation->h = ann;

	zend_objects_store_add_ref(getThis() TSRMLS_CC);
}
/* }}} */

/* {{{ proto object HaruPage::createURLAnnotation(array rectangle, string url)
 Create and return new HaruAnnotation instance */
static PHP_METHOD(HaruPage, createURLAnnotation)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_haruannotation *annotation;
	HPDF_Annotation ann;
	HPDF_Rect r;
	zval *rect;
	int url_len;
	char *url;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "as", &rect, &url, &url_len) == FAILURE) {
		return;
	}

	if (zend_hash_num_elements(Z_ARRVAL_P(rect)) != 4) {
		zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Rectangle parameter is expected to be an array with exactly 4 elements");
		return;
	}

	r = php_haru_array_to_rect(rect);

	ann = HPDF_Page_CreateURILinkAnnot(page->h, r, (const char *)url);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(ann, "Cannot create HaruAnnotation handle");

	object_init_ex(return_value, ce_haruannotation);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);

	annotation = (php_haruannotation *)zend_object_store_get_object(return_value TSRMLS_CC);

	annotation->page = *getThis();
	annotation->h = ann;

	zend_objects_store_add_ref(getThis() TSRMLS_CC);
}
/* }}} */

/* {{{ proto double HaruPage::getTextWidth(string text)
 Get the width of the text using current fontsize, character spacing and word spacing */
static PHP_METHOD(HaruPage, getTextWidth)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_REAL width;
	char *str;
	int str_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
		return;
	}

	width = HPDF_Page_TextWidth(page->h, (const char *)str);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}
	RETURN_DOUBLE((double)width);
}
/* }}} */

/* {{{ proto int HaruPage::MeasureText(string text, double width[, bool wordwrap])
 Calculate the number of characters which can be included within the specified width */
static PHP_METHOD(HaruPage, MeasureText)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_UINT result;
	char *str;
	int str_len;
	double width;
	zend_bool wordwrap = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sd|b", &str, &str_len, &width, &wordwrap) == FAILURE) {
		return;
	}

	result = HPDF_Page_MeasureText(page->h, (const char *)str, (HPDF_REAL)width, (HPDF_BOOL)wordwrap, NULL);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}
	RETURN_LONG(result);
}
/* }}} */

/* {{{ proto int HaruPage::getGMode()
 Get the current graphics mode */
static PHP_METHOD(HaruPage, getGMode)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_UINT16 result;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	result = HPDF_Page_GetGMode(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}
	RETURN_LONG((long)result);
}
/* }}} */

/* {{{ proto array HaruPage::getCurrentPos()
 Get the current position for path painting */
static PHP_METHOD(HaruPage, getCurrentPos)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_Point point;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	point = HPDF_Page_GetCurrentPos(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	array_init(return_value);
	add_assoc_double_ex(return_value, "x", sizeof("x"), point.x);
	add_assoc_double_ex(return_value, "y", sizeof("y"), point.y);
}
/* }}} */

/* {{{ proto array HaruPage::getCurrentTextPos()
 Get the current position for text printing */
static PHP_METHOD(HaruPage, getCurrentTextPos)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_Point point;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	point = HPDF_Page_GetCurrentTextPos(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	array_init(return_value);
	add_assoc_double_ex(return_value, "x", sizeof("x"), point.x);
	add_assoc_double_ex(return_value, "y", sizeof("y"), point.y);
}
/* }}} */

/* {{{ proto object HaruPage::getCurrentFont()
 Get the currently used font */
static PHP_METHOD(HaruPage, getCurrentFont)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_harufont *font;
	HPDF_Font f;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	f = HPDF_Page_GetCurrentFont(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	if (!f) { /* no error */
		RETURN_FALSE;
	}

	object_init_ex(return_value, ce_harufont);
	HARU_SET_REFCOUNT_AND_IS_REF(return_value);

	font = (php_harufont *)zend_object_store_get_object(return_value TSRMLS_CC);

	font->doc = page->doc;
	font->h = f;

	zend_objects_store_add_ref(&page->doc TSRMLS_CC);
}
/* }}} */

/* {{{ proto double HaruPage::getCurrentFontSize()
 Get the current font size */
static PHP_METHOD(HaruPage, getCurrentFontSize)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_REAL size;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	size = HPDF_Page_GetCurrentFontSize(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_DOUBLE((double)size);
}
/* }}} */

/* {{{ proto double HaruPage::getLineWidth()
 Get the current line width */
static PHP_METHOD(HaruPage, getLineWidth)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_REAL width;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	width = HPDF_Page_GetLineWidth(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_DOUBLE((double)width);
}
/* }}} */

/* {{{ proto int HaruPage::getLineCap()
 Get the current line cap style */
static PHP_METHOD(HaruPage, getLineCap)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_LineCap cap;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	cap = HPDF_Page_GetLineCap(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_LONG((long)cap);
}
/* }}} */

/* {{{ proto int HaruPage::getLineJoin()
 Get the current line join style */
static PHP_METHOD(HaruPage, getLineJoin)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_LineJoin join;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	join = HPDF_Page_GetLineJoin(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_LONG((long)join);
}
/* }}} */

/* {{{ proto double HaruPage::getMiterLimit()
 Get the value of miter limit */
static PHP_METHOD(HaruPage, getMiterLimit)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_REAL limit;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	limit = HPDF_Page_GetMiterLimit(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_DOUBLE((double)limit);
}
/* }}} */

/* {{{ proto array HaruPage::getDash()
 Get the current dash pattern */
static PHP_METHOD(HaruPage, getDash)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_DashMode mode;
	unsigned int i;
	zval *element;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	mode = HPDF_Page_GetDash(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	if (!mode.num_ptn) {
		RETURN_FALSE;
	}

	array_init(return_value);
	MAKE_STD_ZVAL(element);
	array_init(element);

	for (i = 0; i < mode.num_ptn; i++) {
		add_next_index_long(element, mode.ptn[i]);
	}
	add_assoc_zval_ex(return_value, "pattern", sizeof("pattern"), element);

	MAKE_STD_ZVAL(element);
	ZVAL_LONG(element, mode.phase);

	add_assoc_zval_ex(return_value, "phase", sizeof("phase"), element);
}
/* }}} */

/* {{{ proto double HaruPage::getFlatness()
 Get the flatness of the page */
static PHP_METHOD(HaruPage, getFlatness)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_REAL flatness;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	flatness = HPDF_Page_GetFlat(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_DOUBLE((double)flatness);
}
/* }}} */

/* {{{ proto double HaruPage::getCharSpace()
 Get the current value of character spacing */
static PHP_METHOD(HaruPage, getCharSpace)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_REAL space;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	space = HPDF_Page_GetCharSpace(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_DOUBLE((double)space);
}
/* }}} */

/* {{{ proto double HaruPage::getWordSpace()
 Get the current value of word spacing */
static PHP_METHOD(HaruPage, getWordSpace)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_REAL space;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	space = HPDF_Page_GetWordSpace(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_DOUBLE((double)space);
}
/* }}} */

/* {{{ proto double HaruPage::getHorizontalScaling()
 Get the current value of horizontal scaling */
static PHP_METHOD(HaruPage, getHorizontalScaling)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_REAL scaling;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	scaling = HPDF_Page_GetHorizontalScalling(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_DOUBLE((double)scaling);
}
/* }}} */

/* {{{ proto double HaruPage::getTextLeading()
 Get the current value of line spacing */
static PHP_METHOD(HaruPage, getTextLeading)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_REAL leading;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	leading = HPDF_Page_GetTextLeading(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_DOUBLE((double)leading);
}
/* }}} */

/* {{{ proto int HaruPage::getTextRenderingMode()
 Get the current text rendering mode */
static PHP_METHOD(HaruPage, getTextRenderingMode)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_TextRenderingMode mode;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	mode = HPDF_Page_GetTextRenderingMode(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_LONG((long)mode);
}
/* }}} */

/* {{{ proto double HaruPage::getTextRise()
 Get the current value of text rising */
static PHP_METHOD(HaruPage, getTextRise)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_REAL rise;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	rise = HPDF_Page_GetTextRise(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_DOUBLE((double)rise);
}
/* }}} */

/* {{{ proto array HaruPage::getRGBFill()
 Get the current filling color */
static PHP_METHOD(HaruPage, getRGBFill)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_RGBColor fill;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	fill = HPDF_Page_GetRGBFill(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	array_init(return_value);
	add_assoc_double_ex(return_value, "r", sizeof("r"), fill.r);
	add_assoc_double_ex(return_value, "g", sizeof("g"), fill.g);
	add_assoc_double_ex(return_value, "b", sizeof("b"), fill.b);
}
/* }}} */

/* {{{ proto array HaruPage::getRGBStroke()
 Get the current stroking color */
static PHP_METHOD(HaruPage, getRGBStroke)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_RGBColor stroke;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	stroke = HPDF_Page_GetRGBStroke(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	array_init(return_value);
	add_assoc_double_ex(return_value, "r", sizeof("r"), stroke.r);
	add_assoc_double_ex(return_value, "g", sizeof("g"), stroke.g);
	add_assoc_double_ex(return_value, "b", sizeof("b"), stroke.b);
}
/* }}} */

/* {{{ proto array HaruPage::getCMYKFill()
 Get the current filling color */
static PHP_METHOD(HaruPage, getCMYKFill)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_CMYKColor fill;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	fill = HPDF_Page_GetCMYKFill(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	array_init(return_value);
	add_assoc_double_ex(return_value, "c", sizeof("c"), fill.c);
	add_assoc_double_ex(return_value, "m", sizeof("m"), fill.m);
	add_assoc_double_ex(return_value, "y", sizeof("y"), fill.y);
	add_assoc_double_ex(return_value, "k", sizeof("k"), fill.k);
}
/* }}} */

/* {{{ proto array HaruPage::getCMYKStroke()
 Get the current stroking color */
static PHP_METHOD(HaruPage, getCMYKStroke)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_CMYKColor stroke;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	stroke = HPDF_Page_GetCMYKStroke(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	array_init(return_value);
	add_assoc_double_ex(return_value, "c", sizeof("c"), stroke.c);
	add_assoc_double_ex(return_value, "m", sizeof("m"), stroke.m);
	add_assoc_double_ex(return_value, "y", sizeof("y"), stroke.y);
	add_assoc_double_ex(return_value, "k", sizeof("k"), stroke.k);
}
/* }}} */

/* {{{ proto double HaruPage::getGrayFill()
 Get the current filling color */
static PHP_METHOD(HaruPage, getGrayFill)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_REAL fill;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	fill = HPDF_Page_GetGrayFill(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_DOUBLE((double)fill);
}
/* }}} */

/* {{{ proto double HaruPage::getGrayStroke()
 Get the current stroking color */
static PHP_METHOD(HaruPage, getGrayStroke)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_REAL stroke;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	stroke = HPDF_Page_GetGrayStroke(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_DOUBLE((double)stroke);
}
/* }}} */

/* {{{ proto int HaruPage::getFillingColorSpace()
 Get the current filling color space */
static PHP_METHOD(HaruPage, getFillingColorSpace)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_ColorSpace space;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	space = HPDF_Page_GetFillingColorSpace(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_LONG((long)space);
}
/* }}} */

/* {{{ proto int HaruPage::getStrokingColorSpace()
 Get the current stroking color space */
static PHP_METHOD(HaruPage, getStrokingColorSpace)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_ColorSpace space;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	space = HPDF_Page_GetStrokingColorSpace(page->h);

	if (php_haru_check_error(page->h->error TSRMLS_CC)) {
		return;
	}

	RETURN_LONG((long)space);
}
/* }}} */

/* {{{ proto bool HaruPage::setSlideShow(int type, double disp_time, double trans_time)
 Set transition style for the page */
static PHP_METHOD(HaruPage, setSlideShow)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	long type;
	double disp_time, trans_time;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ldd", &type, &disp_time, &trans_time) == FAILURE) {
		return;
	}

	switch(type) {
		case HPDF_TS_WIPE_RIGHT:
		case HPDF_TS_WIPE_UP:
		case HPDF_TS_WIPE_LEFT:
		case HPDF_TS_WIPE_DOWN:
		case HPDF_TS_BARN_DOORS_HORIZONTAL_OUT:
		case HPDF_TS_BARN_DOORS_HORIZONTAL_IN:
		case HPDF_TS_BARN_DOORS_VERTICAL_OUT:
		case HPDF_TS_BARN_DOORS_VERTICAL_IN:
		case HPDF_TS_BOX_OUT:
		case HPDF_TS_BOX_IN:
		case HPDF_TS_BLINDS_HORIZONTAL:
		case HPDF_TS_BLINDS_VERTICAL:
		case HPDF_TS_DISSOLVE:
		case HPDF_TS_GLITTER_RIGHT:
		case HPDF_TS_GLITTER_DOWN:
		case HPDF_TS_GLITTER_TOP_LEFT_TO_BOTTOM_RIGHT:
		case HPDF_TS_REPLACE:
			/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid transition style value");
		return;
	}

	status = HPDF_Page_SetSlideShow(page->h, (HPDF_TransitionStyle)type, (HPDF_REAL)disp_time, (HPDF_REAL)trans_time);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::clip()
 Modifies the current clipping path by intersecting it with the current path using the nonzero winding number rule.
 The clipping path is only modified after the succeeding painting operator.
 The following painting operations will only affect the regions of the page contained by the clipping path.
 Initially, the clipping path includes the entire page.
 There is no way to enlarge the current clipping path, or to replace the clipping path with a new one. */
static PHP_METHOD(HaruPage, clip)
{
	php_harupage *page = (php_harupage *) zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_Page_Clip(page->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::saveGS()
 Saves the page's current graphics state to the stack.
 The parameters that are saved are:
    Character Spacing
    Clipping Path
    Dash Mode
    Filling Color
    Flatness
    Font
    Font Size
    Horizontal Scalling
    Line Width
    Line Cap Style
    Line Join Style
    Miter Limit
    Rendering Mode
    Stroking Color
    Text Leading
    Text Rise
    Transformation Matrix
    Word Spacing */
static PHP_METHOD(HaruPage, saveGS)
{
	php_harupage *page = (php_harupage *) zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_Page_GSave(page->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruPage::restoreGS()
 Restores the graphics state saved by saveGS() */
static PHP_METHOD(HaruPage, restoreGS)
{
	HPDF_STATUS status;
	php_harupage *page = (php_harupage *) zend_object_store_get_object(getThis() TSRMLS_CC);

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_Page_GRestore(page->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

#if defined(HPDF_VERSION_ID) && HPDF_VERSION_ID >= 20200
/* {{{ proto bool HaruPage::setZoom(double zoom)
 Set size and direction of the page */
static PHP_METHOD(HaruPage, setZoom)
{
	php_harupage *page = (php_harupage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double zoom;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "f", &zoom) == FAILURE) {
		return;
	}

	status = HPDF_Page_SetZoom(page->h, (HPDF_REAL) zoom);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */
#endif

/* }}} */

/* HaruImage methods {{{ */

/* {{{ proto bool HaruImage::__construct()
 Dummy constructor */
static PHP_METHOD(HaruImage, __construct)
{
	return;
}
/* }}} */

/* {{{ proto array HaruImage::getSize()
 Get size of the image */
static PHP_METHOD(HaruImage, getSize)
{
	php_haruimage *image = (php_haruimage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_Point ret;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	ret = HPDF_Image_GetSize(image->h);

	if (php_haru_check_error(image->h->error TSRMLS_CC)) {
		return;
	}

	array_init(return_value);
	add_assoc_double_ex(return_value, "width", sizeof("width"), ret.x);
	add_assoc_double_ex(return_value, "height", sizeof("height"), ret.y);
}
/* }}} */

/* {{{ proto int HaruImage::getWidth()
 Get width of the image */
static PHP_METHOD(HaruImage, getWidth)
{
	php_haruimage *image = (php_haruimage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_UINT width;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	width = HPDF_Image_GetWidth(image->h);

	if (php_haru_check_error(image->h->error TSRMLS_CC)) {
		return;
	}
	RETURN_LONG(width);
}
/* }}} */

/* {{{ proto int HaruImage::getHeight()
 Get height of the image */
static PHP_METHOD(HaruImage, getHeight)
{
	php_haruimage *image = (php_haruimage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_UINT height;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	height = HPDF_Image_GetHeight(image->h);

	if (php_haru_check_error(image->h->error TSRMLS_CC)) {
		return;
	}
	RETURN_LONG(height);
}
/* }}} */

/* {{{ proto int HaruImage::getBitsPerComponent()
 Get the number of bits used to describe each color component of the image */
static PHP_METHOD(HaruImage, getBitsPerComponent)
{
	php_haruimage *image = (php_haruimage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_UINT bits;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	bits = HPDF_Image_GetBitsPerComponent(image->h);

	if (php_haru_check_error(image->h->error TSRMLS_CC)) {
		return;
	}
	RETURN_LONG(bits);
}
/* }}} */

/* {{{ proto string HaruImage::getColorSpace()
 Get the name of the color space */
static PHP_METHOD(HaruImage, getColorSpace)
{
	php_haruimage *image = (php_haruimage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	const char *space;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	space = HPDF_Image_GetColorSpace(image->h);

	if (php_haru_check_error(image->h->error TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(space, "Failed to get the color space of the image");

	RETURN_STRING((char *)space, 1);
}
/* }}} */

/* {{{ proto bool HaruImage::setColorMask(int rmin, int rmax, int gmin, int gmax, int bmin, int bmax)
 Set the color mask of the image */
static PHP_METHOD(HaruImage, setColorMask)
{
	php_haruimage *image = (php_haruimage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	long rmin, rmax, gmin, gmax, bmin, bmax;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "llllll", &rmin, &rmax, &gmin, &gmax, &bmin, &bmax) == FAILURE) {
		return;
	}

	status = HPDF_Image_SetColorMask(image->h, (HPDF_UINT)rmin, (HPDF_UINT)rmax, (HPDF_UINT)gmin, (HPDF_UINT)gmax, (HPDF_UINT)bmin, (HPDF_UINT)bmax);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruImage::setMaskImage(object mask_image)
 Set the image mask */
static PHP_METHOD(HaruImage, setMaskImage)
{
	php_haruimage *image = (php_haruimage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_haruimage *mask_image;
	HPDF_STATUS status;
	zval *z_mask_image;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &z_mask_image, ce_haruimage) == FAILURE) {
		return;
	}

	mask_image = (php_haruimage *)zend_object_store_get_object(z_mask_image TSRMLS_CC);

	status = HPDF_Image_SetMaskImage(image->h, mask_image->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

#if defined(HPDF_VERSION_ID) && HPDF_VERSION_ID >= 20200

/* {{{ proto bool HaruImage::addSMask(object smask_image)
 Set image transparency mask */
static PHP_METHOD(HaruImage, addSMask)
{
	php_haruimage *image = (php_haruimage *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_haruimage *smask_image;
	HPDF_STATUS status;
	zval *z_smask_image;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &z_smask_image, ce_haruimage) == FAILURE) {
		return;
	}

	smask_image = (php_haruimage *)zend_object_store_get_object(z_smask_image TSRMLS_CC);

	status = HPDF_Image_AddSMask(image->h, smask_image->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */
#endif

/* }}} */

/* HaruFont methods {{{ */

/* {{{ proto bool HaruFont::__construct()
 Dummy constructor */
static PHP_METHOD(HaruFont, __construct)
{
	return;
}
/* }}} */

/* {{{ proto string HaruFont::getFontName()
 Get the name of the font */
static PHP_METHOD(HaruFont, getFontName)
{
	php_harufont *font = (php_harufont *)zend_object_store_get_object(getThis() TSRMLS_CC);
	const char *name;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	name = HPDF_Font_GetFontName(font->h);

	if (php_haru_check_error(font->h->error TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(name, "Failed to get the name of the font");

	RETURN_STRING((char *)name, 1);
}
/* }}} */

/* {{{ proto string HaruFont::getEncodingName()
 Get the name of the encoding */
static PHP_METHOD(HaruFont, getEncodingName)
{
	php_harufont *font = (php_harufont *)zend_object_store_get_object(getThis() TSRMLS_CC);
	const char *name;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	name = HPDF_Font_GetEncodingName(font->h);

	if (php_haru_check_error(font->h->error TSRMLS_CC)) {
		return;
	}
	PHP_HARU_NULL_CHECK(name, "Failed to get the encoding name of the font");

	RETURN_STRING((char *)name, 1);
}
/* }}} */

/* {{{ proto int HaruFont::getUnicodeWidth(int character)
 Get the width of the character in the font */
static PHP_METHOD(HaruFont, getUnicodeWidth)
{
	php_harufont *font = (php_harufont *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_INT width;
	long character;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &character) == FAILURE) {
		return;
	}

	width = HPDF_Font_GetUnicodeWidth(font->h, (HPDF_UINT16)character);

	if (php_haru_check_error(font->h->error TSRMLS_CC)) {
		return;
	}
	RETURN_LONG((long)width);
}
/* }}} */

/* {{{ proto int HaruFont::getAscent()
 Get the vertical ascent of the font */
static PHP_METHOD(HaruFont, getAscent)
{
	php_harufont *font = (php_harufont *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_INT ascent;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	ascent = HPDF_Font_GetAscent(font->h);

	if (php_haru_check_error(font->h->error TSRMLS_CC)) {
		return;
	}
	RETURN_LONG((long)ascent);
}
/* }}} */

/* {{{ proto int HaruFont::getDescent()
 Get the vertical descent of the font */
static PHP_METHOD(HaruFont, getDescent)
{
	php_harufont *font = (php_harufont *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_INT descent;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	descent = HPDF_Font_GetDescent(font->h);

	if (php_haru_check_error(font->h->error TSRMLS_CC)) {
		return;
	}
	RETURN_LONG(descent);
}
/* }}} */

/* {{{ proto int HaruFont::getXHeight()
 Get the distance from the baseline of lowercase letter */
static PHP_METHOD(HaruFont, getXHeight)
{
	php_harufont *font = (php_harufont *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_UINT xheight;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	xheight = HPDF_Font_GetXHeight(font->h);

	if (php_haru_check_error(font->h->error TSRMLS_CC)) {
		return;
	}
	RETURN_LONG((long)xheight);
}
/* }}} */

/* {{{ proto int HaruFont::getCapHeight()
 Get the distance from the baseline of uppercase letter */
static PHP_METHOD(HaruFont, getCapHeight)
{
	php_harufont *font = (php_harufont *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_UINT cap_height;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	cap_height = HPDF_Font_GetCapHeight(font->h);

	if (php_haru_check_error(font->h->error TSRMLS_CC)) {
		return;
	}
	RETURN_LONG((long)cap_height);
}
/* }}} */

/* {{{ proto array HaruFont::getTextWidth(string text)
 Get the total width of the text, number of characters, number of words and number of spaces */
static PHP_METHOD(HaruFont, getTextWidth)
{
	php_harufont *font = (php_harufont *)zend_object_store_get_object(getThis() TSRMLS_CC);
	char *str;
	int str_len;
	HPDF_TextWidth width;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
		return;
	}

	width = HPDF_Font_TextWidth(font->h, (const HPDF_BYTE *)str, (HPDF_UINT)str_len);

	if (php_haru_check_error(font->h->error TSRMLS_CC)) {
		return;
	}

	array_init(return_value);
	add_assoc_long_ex(return_value, "numchars", sizeof("numchars"), width.numchars);
	add_assoc_long_ex(return_value, "numwords", sizeof("numwords"), width.numwords);
	add_assoc_long_ex(return_value, "width", sizeof("width"), width.width);
	add_assoc_long_ex(return_value, "numspace", sizeof("numspace"), width.numspace);
}
/* }}} */

/* {{{ proto int HaruFont::MeasureText(string text, double width, double font_size, double char_space, double word_space[, bool word_wrap])
 Calculate the number of characters which can be included within the specified width */
static PHP_METHOD(HaruFont, MeasureText)
{
	php_harufont *font = (php_harufont *)zend_object_store_get_object(getThis() TSRMLS_CC);
	char *str;
	int str_len, result;
	double width, font_size, char_space, word_space;
	zend_bool wordwrap = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sdddd|b", &str, &str_len, &width, &font_size, &char_space, &word_space, &wordwrap) == FAILURE) {
		return;
	}

	result = HPDF_Font_MeasureText(font->h, (const HPDF_BYTE *)str, (HPDF_UINT)str_len, (HPDF_REAL)width, (HPDF_REAL)font_size, (HPDF_REAL)char_space, (HPDF_REAL)word_space, (HPDF_BOOL)wordwrap, NULL);

	if (php_haru_check_error(font->h->error TSRMLS_CC)) {
		return;
	}
	RETURN_LONG(result);
}
/* }}} */

/* }}} */

/* HaruEncoder methods {{{ */

/* {{{ proto bool HaruEncoder::__construct()
 Dummy constructor */
static PHP_METHOD(HaruEncoder, __construct)
{
	return;
}
/* }}} */

/* {{{ proto int HaruEncoder::getType()
 Get the type of the encoder */
static PHP_METHOD(HaruEncoder, getType)
{
	php_haruencoder *encoder = (php_haruencoder *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_EncoderType type;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	type = HPDF_Encoder_GetType(encoder->h);

	RETURN_LONG((long)type);
}
/* }}} */

/* {{{ proto int HaruEncoder::getByteType(string text, int index)
 Get the type of the byte in the text on the current position */
static PHP_METHOD(HaruEncoder, getByteType)
{
	php_haruencoder *encoder = (php_haruencoder *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_ByteType type;
	char *str;
	int str_len;
	long index;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &str, &str_len, &index) == FAILURE) {
		return;
	}

	type = HPDF_Encoder_GetByteType(encoder->h, (const char*) str, (HPDF_UINT)index);

	RETURN_LONG((long)type);
}
/* }}} */

/* {{{ proto int HaruEncoder::getUnicode(int character)
 Convert the specified character to unicode */
static PHP_METHOD(HaruEncoder, getUnicode)
{
	php_haruencoder *encoder = (php_haruencoder *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_UNICODE unicode;
	long character;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &character) == FAILURE) {
		return;
	}

	unicode = HPDF_Encoder_GetUnicode(encoder->h, (HPDF_UINT16)character);

	RETURN_LONG((long)unicode);
}
/* }}} */

/* {{{ proto int HaruEncoder::getWritingMode()
 Get the writing mode of the encoder */
static PHP_METHOD(HaruEncoder, getWritingMode)
{
	php_haruencoder *encoder = (php_haruencoder *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_WritingMode mode;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	mode = HPDF_Encoder_GetWritingMode(encoder->h);

	RETURN_LONG((long)mode);
}
/* }}} */

/* }}}  */

/* HaruAnnotation methods {{{ */

/* {{{ proto bool HaruAnnotation::__construct()
 Dummy constructor */
static PHP_METHOD(HaruAnnotation, __construct)
{
	return;
}
/* }}} */

/* {{{ proto bool HaruAnnotation::setHighlightMode(int mode)
 Set the highlighting mode of the annotation */
static PHP_METHOD(HaruAnnotation, setHighlightMode)
{
	php_haruannotation *a = (php_haruannotation *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	long mode;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &mode) == FAILURE) {
		return;
	}

	switch(mode) {
		case HPDF_ANNOT_NO_HIGHTLIGHT:
		case HPDF_ANNOT_INVERT_BOX:
		case HPDF_ANNOT_INVERT_BORDER:
		case HPDF_ANNOT_DOWN_APPEARANCE:
			/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid highlight mode value");
		return;
	}

	status = HPDF_LinkAnnot_SetHighlightMode(a->h, (HPDF_AnnotHighlightMode)mode);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruAnnotation::setBorderStyle(double width, int dash_on, int dash_off)
 Set the border style of the annotation */
static PHP_METHOD(HaruAnnotation, setBorderStyle)
{
	php_haruannotation *a = (php_haruannotation *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double width;
	long dash_on, dash_off;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dll", &width, &dash_on, &dash_off) == FAILURE) {
		return;
	}

	status = HPDF_LinkAnnot_SetBorderStyle(a->h, (HPDF_REAL)width, (HPDF_UINT16)dash_on, (HPDF_UINT16)dash_off);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruAnnotation::setIcon(int icon)
 Set the icon style of the annotation */
static PHP_METHOD(HaruAnnotation, setIcon)
{
	php_haruannotation *a = (php_haruannotation *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	long icon;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &icon) == FAILURE) {
		return;
	}

	switch(icon) {
		case HPDF_ANNOT_ICON_COMMENT:
		case HPDF_ANNOT_ICON_KEY:
		case HPDF_ANNOT_ICON_NOTE:
		case HPDF_ANNOT_ICON_HELP:
		case HPDF_ANNOT_ICON_NEW_PARAGRAPH:
		case HPDF_ANNOT_ICON_PARAGRAPH:
		case HPDF_ANNOT_ICON_INSERT:
			/* only these are valid */
			break;
		default:
			zend_throw_exception_ex(ce_haruexception, 0 TSRMLS_CC, "Invalid icon value");
		return;
	}

	status = HPDF_TextAnnot_SetIcon(a->h, (HPDF_AnnotIcon)icon);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruAnnotation::setOpened(bool opened)
 Set the initial state of the annotation */
static PHP_METHOD(HaruAnnotation, setOpened)
{
	php_haruannotation *a = (php_haruannotation *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	zend_bool opened;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &opened) == FAILURE) {
		return;
	}

	status = HPDF_TextAnnot_SetOpened(a->h, (HPDF_BOOL)opened);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* }}} */

/* HaruDestination methods {{{ */

/* {{{ proto bool HaruDestination::__construct()
 Dummy constructor */
static PHP_METHOD(HaruDestination, __construct)
{
	return;
}
/* }}} */

/* {{{ proto bool HaruDestination::setXYZ(double left, double top, double zoom)
 Set the appearance of the page */
static PHP_METHOD(HaruDestination, setXYZ)
{
	php_harudestination *dest = (php_harudestination *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double left, top, zoom;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ddd", &left, &top, &zoom) == FAILURE) {
		return;
	}

	status = HPDF_Destination_SetXYZ(dest->h, (HPDF_REAL)left, (HPDF_REAL)top, (HPDF_REAL)zoom);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDestination::setFit()
 Set the appearance of the page to fit the window */
static PHP_METHOD(HaruDestination, setFit)
{
	php_harudestination *dest = (php_harudestination *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_Destination_SetFit(dest->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDestination::setFitH(double top)
 Set the appearance of the page to fit the window width */
static PHP_METHOD(HaruDestination, setFitH)
{
	php_harudestination *dest = (php_harudestination *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double top;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &top) == FAILURE) {
		return;
	}

	status = HPDF_Destination_SetFitH(dest->h, (HPDF_REAL)top);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDestination::setFitV(double left)
 Set the appearance of the page to fit the window height */
static PHP_METHOD(HaruDestination, setFitV)
{
	php_harudestination *dest = (php_harudestination *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double left;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &left) == FAILURE) {
		return;
	}

	status = HPDF_Destination_SetFitV(dest->h, (HPDF_REAL)left);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDestination::setFitR(double left, double bottom, double right, double top)
 Set the appearance of the page to fit the specified rectangle */
static PHP_METHOD(HaruDestination, setFitR)
{
	php_harudestination *dest = (php_harudestination *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double left, bottom, right, top;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dddd", &left, &bottom, &right, &top) == FAILURE) {
		return;
	}

	status = HPDF_Destination_SetFitR(dest->h, (HPDF_REAL) left, (HPDF_REAL) bottom, (HPDF_REAL) right, (HPDF_REAL) top);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDestination::setFitB()
 Set the appearance of the page to fit the bounding box of the page within the window */
static PHP_METHOD(HaruDestination, setFitB)
{
	php_harudestination *dest = (php_harudestination *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	status = HPDF_Destination_SetFitB(dest->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDestination::setFitBH(double top)
 Set the appearance of the page to fit the width of the bounding box */
static PHP_METHOD(HaruDestination, setFitBH)
{
	php_harudestination *dest = (php_harudestination *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double top;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &top) == FAILURE) {
		return;
	}

	status = HPDF_Destination_SetFitBH(dest->h, (HPDF_REAL)top);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruDestination::setFitBV(double left)
 Set the appearance of the page to fit the height of the boudning box */
static PHP_METHOD(HaruDestination, setFitBV)
{
	php_harudestination *dest = (php_harudestination *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	double left;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &left) == FAILURE) {
		return;
	}

	status = HPDF_Destination_SetFitBV(dest->h, (HPDF_REAL)left);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* }}} */

/* HaruOutline methods {{{ */

/* {{{ proto bool HaruOutline::__construct()
 Dummy constructor */
static PHP_METHOD(HaruOutline, __construct)
{
	return;
}
/* }}} */

/* {{{ proto bool HaruOutline::setOpened(bool opened)
 Set the initial state of the outline */
static PHP_METHOD(HaruOutline, setOpened)
{
	php_haruoutline *outline = (php_haruoutline *)zend_object_store_get_object(getThis() TSRMLS_CC);
	HPDF_STATUS status;
	zend_bool opened;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &opened) == FAILURE) {
		return;
	}

	status = HPDF_Outline_SetOpened(outline->h, (HPDF_BOOL)opened);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HaruOutline::setDestination(object destination)
 Set the destination for the outline */
static PHP_METHOD(HaruOutline, setDestination)
{
	php_haruoutline *outline = (php_haruoutline *)zend_object_store_get_object(getThis() TSRMLS_CC);
	php_harudestination *d;
	HPDF_STATUS status;
	zval *destination;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &destination, ce_harudestination) == FAILURE) {
		return;
	}

	d = (php_harudestination *)zend_object_store_get_object(destination TSRMLS_CC);

	status = HPDF_Outline_SetDestination(outline->h, d->h);

	if (php_haru_status_to_exception(status TSRMLS_CC)) {
		return;
	}
	RETURN_TRUE;
}
/* }}} */

/* }}} */

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO(arginfo_harudoc___void, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_insertpage, 0, 0, 1)
	ZEND_ARG_INFO(0, page)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_setcurrentencoder, 0, 0, 1)
	ZEND_ARG_INFO(0, encoding)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_save, 0, 0, 1)
	ZEND_ARG_INFO(0, file)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_readfromstream, 0, 0, 1)
	ZEND_ARG_INFO(0, bytes)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_setpagelayout, 0, 0, 1)
	ZEND_ARG_INFO(0, layout)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_setpagemode, 0, 0, 1)
	ZEND_ARG_INFO(0, mode)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_setinfoattr, 0, 0, 2)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, info)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_getinfoattr, 0, 0, 1)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_setinfodateattr, 0, 0, 10)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, year)
	ZEND_ARG_INFO(0, month)
	ZEND_ARG_INFO(0, day)
	ZEND_ARG_INFO(0, hour)
	ZEND_ARG_INFO(0, min)
	ZEND_ARG_INFO(0, sec)
	ZEND_ARG_INFO(0, ind)
	ZEND_ARG_INFO(0, off_hour)
	ZEND_ARG_INFO(0, off_min)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_getfont, 0, 0, 1)
	ZEND_ARG_INFO(0, fontname)
	ZEND_ARG_INFO(0, encoding)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_loadttf, 0, 0, 1)
	ZEND_ARG_INFO(0, fontfile)
	ZEND_ARG_INFO(0, embed)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_loadttc, 0, 0, 2)
	ZEND_ARG_INFO(0, fontfile)
	ZEND_ARG_INFO(0, index)
	ZEND_ARG_INFO(0, embed)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_loadtype1, 0, 0, 1)
	ZEND_ARG_INFO(0, afmfile)
	ZEND_ARG_INFO(0, pfmfile)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_loadpng, 0, 0, 1)
	ZEND_ARG_INFO(0, filename)
	ZEND_ARG_INFO(0, deferred)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_loadjpeg, 0, 0, 1)
	ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_loadraw, 0, 0, 4)
	ZEND_ARG_INFO(0, filename)
	ZEND_ARG_INFO(0, width)
	ZEND_ARG_INFO(0, height)
	ZEND_ARG_INFO(0, color_space)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_setpassword, 0, 0, 2)
	ZEND_ARG_INFO(0, owner_password)
	ZEND_ARG_INFO(0, user_password)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_setpermission, 0, 0, 1)
	ZEND_ARG_INFO(0, permission)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_setencryptionmode, 0, 0, 1)
	ZEND_ARG_INFO(0, mode)
	ZEND_ARG_INFO(0, key_len)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_setcompressionmode, 0, 0, 1)
	ZEND_ARG_INFO(0, mode)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_setpagesconfiguration, 0, 0, 1)
	ZEND_ARG_INFO(0, page_per_pages)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_setopenaction, 0, 0, 1)
	ZEND_ARG_INFO(0, destination)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_createoutline, 0, 0, 1)
	ZEND_ARG_INFO(0, title)
	ZEND_ARG_INFO(0, parent_outline)
	ZEND_ARG_INFO(0, encoder)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudoc_addpagelabel, 0, 0, 3)
	ZEND_ARG_INFO(0, first_page)
	ZEND_ARG_INFO(0, style)
	ZEND_ARG_INFO(0, first_num)
	ZEND_ARG_INFO(0, prefix)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_drawimage, 0, 0, 5)
	ZEND_ARG_INFO(0, image)
	ZEND_ARG_INFO(0, x)
	ZEND_ARG_INFO(0, y)
	ZEND_ARG_INFO(0, width)
	ZEND_ARG_INFO(0, height)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setlinewidth, 0, 0, 1)
	ZEND_ARG_INFO(0, width)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setlinecap, 0, 0, 1)
	ZEND_ARG_INFO(0, cap)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setlinejoin, 0, 0, 1)
	ZEND_ARG_INFO(0, join)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setmiterlimit, 0, 0, 1)
	ZEND_ARG_INFO(0, limit)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setdash, 0, 0, 2)
	ZEND_ARG_INFO(0, pattern)
	ZEND_ARG_INFO(0, phase)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setflatness, 0, 0, 1)
	ZEND_ARG_INFO(0, flatness)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setfontandsize, 0, 0, 2)
	ZEND_ARG_INFO(0, font)
	ZEND_ARG_INFO(0, size)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setcharspace, 0, 0, 1)
	ZEND_ARG_INFO(0, char_space)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setwordspace, 0, 0, 1)
	ZEND_ARG_INFO(0, word_space)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_sethorizontalscaling, 0, 0, 1)
	ZEND_ARG_INFO(0, scaling)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_settextleading, 0, 0, 1)
	ZEND_ARG_INFO(0, text_leading)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_settextrenderingmode, 0, 0, 1)
	ZEND_ARG_INFO(0, mode)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_settextrise, 0, 0, 1)
	ZEND_ARG_INFO(0, rise)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setgraystroke, 0, 0, 1)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setrgbstroke, 0, 0, 3)
	ZEND_ARG_INFO(0, r)
	ZEND_ARG_INFO(0, g)
	ZEND_ARG_INFO(0, b)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setcmykstroke, 0, 0, 4)
	ZEND_ARG_INFO(0, c)
	ZEND_ARG_INFO(0, m)
	ZEND_ARG_INFO(0, y)
	ZEND_ARG_INFO(0, k)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_concat, 0, 0, 6)
	ZEND_ARG_INFO(0, a)
	ZEND_ARG_INFO(0, b)
	ZEND_ARG_INFO(0, c)
	ZEND_ARG_INFO(0, d)
	ZEND_ARG_INFO(0, x)
	ZEND_ARG_INFO(0, y)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_settextmatrix, 0, 0, 6)
	ZEND_ARG_INFO(0, a)
	ZEND_ARG_INFO(0, b)
	ZEND_ARG_INFO(0, c)
	ZEND_ARG_INFO(0, d)
	ZEND_ARG_INFO(0, x)
	ZEND_ARG_INFO(0, y)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_moveto, 0, 0, 2)
	ZEND_ARG_INFO(0, x)
	ZEND_ARG_INFO(0, y)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_stroke, 0, 0, 0)
	ZEND_ARG_INFO(0, close_path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_fillstroke, 0, 0, 0)
	ZEND_ARG_INFO(0, close_path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_eofillstroke, 0, 0, 0)
	ZEND_ARG_INFO(0, close_path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_lineto, 0, 0, 2)
	ZEND_ARG_INFO(0, x)
	ZEND_ARG_INFO(0, y)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_curveto, 0, 0, 6)
	ZEND_ARG_INFO(0, x1)
	ZEND_ARG_INFO(0, y1)
	ZEND_ARG_INFO(0, x2)
	ZEND_ARG_INFO(0, y2)
	ZEND_ARG_INFO(0, x3)
	ZEND_ARG_INFO(0, y3)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_curveto2, 0, 0, 4)
	ZEND_ARG_INFO(0, x2)
	ZEND_ARG_INFO(0, y2)
	ZEND_ARG_INFO(0, x3)
	ZEND_ARG_INFO(0, y3)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_curveto3, 0, 0, 4)
	ZEND_ARG_INFO(0, x1)
	ZEND_ARG_INFO(0, y1)
	ZEND_ARG_INFO(0, x3)
	ZEND_ARG_INFO(0, y3)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_rectangle, 0, 0, 4)
	ZEND_ARG_INFO(0, x)
	ZEND_ARG_INFO(0, y)
	ZEND_ARG_INFO(0, width)
	ZEND_ARG_INFO(0, height)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_arc, 0, 0, 5)
	ZEND_ARG_INFO(0, x)
	ZEND_ARG_INFO(0, y)
	ZEND_ARG_INFO(0, ray)
	ZEND_ARG_INFO(0, ang1)
	ZEND_ARG_INFO(0, ang2)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_circle, 0, 0, 3)
	ZEND_ARG_INFO(0, x)
	ZEND_ARG_INFO(0, y)
	ZEND_ARG_INFO(0, ray)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_ellipse, 0, 0, 4)
	ZEND_ARG_INFO(0, x)
	ZEND_ARG_INFO(0, y)
	ZEND_ARG_INFO(0, xray)
	ZEND_ARG_INFO(0, yray)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_showtext, 0, 0, 1)
	ZEND_ARG_INFO(0, text)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_showtextnextline, 0, 0, 1)
	ZEND_ARG_INFO(0, text)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_textout, 0, 0, 3)
	ZEND_ARG_INFO(0, x)
	ZEND_ARG_INFO(0, y)
	ZEND_ARG_INFO(0, text)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_textrect, 0, 0, 5)
	ZEND_ARG_INFO(0, left)
	ZEND_ARG_INFO(0, top)
	ZEND_ARG_INFO(0, right)
	ZEND_ARG_INFO(0, bottom)
	ZEND_ARG_INFO(0, text)
	ZEND_ARG_INFO(0, align)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_movetextpos, 0, 0, 2)
	ZEND_ARG_INFO(0, x)
	ZEND_ARG_INFO(0, y)
	ZEND_ARG_INFO(0, set_leading)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setwidth, 0, 0, 1)
	ZEND_ARG_INFO(0, width)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setheight, 0, 0, 1)
	ZEND_ARG_INFO(0, height)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setsize, 0, 0, 2)
	ZEND_ARG_INFO(0, size)
	ZEND_ARG_INFO(0, direction)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setrotate, 0, 0, 1)
	ZEND_ARG_INFO(0, angle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_createtextannotation, 0, 0, 2)
	ZEND_ARG_INFO(0, rectangle)
	ZEND_ARG_INFO(0, text)
	ZEND_ARG_INFO(0, encoder)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_createlinkannotation, 0, 0, 2)
	ZEND_ARG_INFO(0, rectangle)
	ZEND_ARG_INFO(0, destination)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_createurlannotation, 0, 0, 2)
	ZEND_ARG_INFO(0, rectangle)
	ZEND_ARG_INFO(0, url)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_gettextwidth, 0, 0, 1)
	ZEND_ARG_INFO(0, text)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_measuretext, 0, 0, 2)
	ZEND_ARG_INFO(0, text)
	ZEND_ARG_INFO(0, width)
	ZEND_ARG_INFO(0, wordwrap)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setslideshow, 0, 0, 3)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, disp_time)
	ZEND_ARG_INFO(0, trans_time)
ZEND_END_ARG_INFO()

#if defined(HPDF_VERSION_ID) && HPDF_VERSION_ID >= 20200
ZEND_BEGIN_ARG_INFO_EX(arginfo_harupage_setzoom, 0, 0, 1)
	ZEND_ARG_INFO(0, zoom)
ZEND_END_ARG_INFO()
#endif

ZEND_BEGIN_ARG_INFO_EX(arginfo_haruimage_setcolormask, 0, 0, 6)
	ZEND_ARG_INFO(0, rmin)
	ZEND_ARG_INFO(0, rmax)
	ZEND_ARG_INFO(0, gmin)
	ZEND_ARG_INFO(0, gmax)
	ZEND_ARG_INFO(0, bmin)
	ZEND_ARG_INFO(0, bmax)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_haruimage_setmaskimage, 0, 0, 1)
	ZEND_ARG_INFO(0, mask_image)
ZEND_END_ARG_INFO()

#if defined(HPDF_VERSION_ID) && HPDF_VERSION_ID >= 20200
ZEND_BEGIN_ARG_INFO_EX(arginfo_haruimage_addsmask, 0, 0, 1)
	ZEND_ARG_INFO(0, smask_image)
ZEND_END_ARG_INFO()
#endif

ZEND_BEGIN_ARG_INFO_EX(arginfo_harufont_getunicodewidth, 0, 0, 1)
	ZEND_ARG_INFO(0, character)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harufont_gettextwidth, 0, 0, 1)
	ZEND_ARG_INFO(0, text)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harufont_measuretext, 0, 0, 5)
	ZEND_ARG_INFO(0, text)
	ZEND_ARG_INFO(0, width)
	ZEND_ARG_INFO(0, font_size)
	ZEND_ARG_INFO(0, char_space)
	ZEND_ARG_INFO(0, word_space)
	ZEND_ARG_INFO(0, word_wrap)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_haruencoder_getbytetype, 0, 0, 2)
	ZEND_ARG_INFO(0, text)
	ZEND_ARG_INFO(0, index)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_haruencoder_getunicode, 0, 0, 1)
	ZEND_ARG_INFO(0, character)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_haruannotation_sethighlightmode, 0, 0, 1)
	ZEND_ARG_INFO(0, mode)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_haruannotation_setborderstyle, 0, 0, 3)
	ZEND_ARG_INFO(0, width)
	ZEND_ARG_INFO(0, dash_on)
	ZEND_ARG_INFO(0, dash_off)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_haruannotation_seticon, 0, 0, 1)
	ZEND_ARG_INFO(0, icon)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_haruannotation_setopened, 0, 0, 1)
	ZEND_ARG_INFO(0, opened)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudestination_setxyz, 0, 0, 3)
	ZEND_ARG_INFO(0, left)
	ZEND_ARG_INFO(0, top)
	ZEND_ARG_INFO(0, zoom)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudestination_setfith, 0, 0, 1)
	ZEND_ARG_INFO(0, top)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudestination_setfitv, 0, 0, 1)
	ZEND_ARG_INFO(0, left)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudestination_setfitr, 0, 0, 4)
	ZEND_ARG_INFO(0, left)
	ZEND_ARG_INFO(0, bottom)
	ZEND_ARG_INFO(0, right)
	ZEND_ARG_INFO(0, top)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudestination_setfitbh, 0, 0, 1)
	ZEND_ARG_INFO(0, top)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_harudestination_setfitbv, 0, 0, 1)
	ZEND_ARG_INFO(0, left)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_haruoutline_setopened, 0, 0, 1)
	ZEND_ARG_INFO(0, opened)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_haruoutline_setdestination, 0, 0, 1)
	ZEND_ARG_INFO(0, destination)
ZEND_END_ARG_INFO()
/* }}} */


/* class method tables {{{ */

static zend_function_entry harudoc_methods[] = { /* {{{ */
	PHP_ME(HaruDoc, __construct, 			arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, resetError, 			arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, save, 					arginfo_harudoc_save, 					ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, output, 				arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, saveToStream, 			arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, resetStream, 			arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, getStreamSize, 			arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, readFromStream, 		arginfo_harudoc_readfromstream, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, addPage, 				arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, insertPage, 			arginfo_harudoc_insertpage, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, getCurrentPage, 		arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, getEncoder, 			arginfo_harudoc_setcurrentencoder, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, getCurrentEncoder, 		arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, setCurrentEncoder, 		arginfo_harudoc_setcurrentencoder, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, setPageLayout, 			arginfo_harudoc_setpagelayout, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, getPageLayout, 			arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, setPageMode, 			arginfo_harudoc_setpagemode, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, getPageMode, 			arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, setInfoAttr, 			arginfo_harudoc_setinfoattr, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, setInfoDateAttr, 		arginfo_harudoc_setinfodateattr, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, getInfoAttr, 			arginfo_harudoc_getinfoattr, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, getFont, 				arginfo_harudoc_getfont, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, loadTTF, 				arginfo_harudoc_loadttf, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, loadTTC, 				arginfo_harudoc_loadttc, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, loadType1, 				arginfo_harudoc_loadtype1, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, loadPNG, 				arginfo_harudoc_loadpng, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, loadJPEG, 				arginfo_harudoc_loadjpeg, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, loadRaw, 				arginfo_harudoc_loadraw, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, setPassword, 			arginfo_harudoc_setpassword, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, setPermission, 			arginfo_harudoc_setpermission, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, setEncryptionMode, 		arginfo_harudoc_setencryptionmode, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, setCompressionMode, 	arginfo_harudoc_setcompressionmode, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, setPagesConfiguration, 	arginfo_harudoc_setpagesconfiguration, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, setOpenAction, 			arginfo_harudoc_setopenaction, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, createOutline, 			arginfo_harudoc_createoutline, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, addPageLabel, 			arginfo_harudoc_addpagelabel, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, useJPFonts, 			arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, useJPEncodings, 		arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, useKRFonts, 			arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, useKREncodings, 		arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, useCNSFonts, 			arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, useCNSEncodings, 		arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, useCNTFonts, 			arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, useCNTEncodings, 		arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	PHP_ME(HaruDoc, useUTFEncodings, 		arginfo_harudoc___void, 				ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

/* }}} */

static zend_function_entry harupage_methods[] = { /* {{{ */
	PHP_ME(HaruPage, __construct, 				arginfo_harudoc___void, 		ZEND_ACC_PRIVATE)
	PHP_ME(HaruPage, drawImage, 				arginfo_harupage_drawimage, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setLineWidth, 				arginfo_harupage_setlinewidth, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setLineCap, 				arginfo_harupage_setlinecap, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setLineJoin, 				arginfo_harupage_setlinejoin, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setMiterLimit, 			arginfo_harupage_setmiterlimit, ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setFlatness, 				arginfo_harupage_setflatness, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setDash, 					arginfo_harupage_setdash, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, Concat, 					arginfo_harupage_concat, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getTransMatrix, 			arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setTextMatrix, 			arginfo_harupage_settextmatrix, ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getTextMatrix, 			arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, moveTo, 					arginfo_harupage_moveto, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, stroke, 					arginfo_harupage_stroke, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, fill, 						arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, eofill, 					arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, lineTo, 					arginfo_harupage_lineto, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, curveTo, 					arginfo_harupage_curveto, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, curveTo2, 					arginfo_harupage_curveto2, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, curveTo3, 					arginfo_harupage_curveto3, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, rectangle, 				arginfo_harupage_rectangle, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, arc, 						arginfo_harupage_arc, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, circle, 					arginfo_harupage_circle, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, showText, 					arginfo_harupage_showtext, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, showTextNextLine, 			arginfo_harupage_showtextnextline, ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, textOut, 					arginfo_harupage_textout, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, beginText, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, endText, 					arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setFontAndSize, 			arginfo_harupage_setfontandsize, ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setCharSpace, 				arginfo_harupage_setcharspace, ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setWordSpace, 				arginfo_harupage_setwordspace, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setHorizontalScaling, 		arginfo_harupage_sethorizontalscaling, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setTextLeading, 			arginfo_harupage_settextleading, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setTextRenderingMode, 		arginfo_harupage_settextrenderingmode, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setTextRise, 				arginfo_harupage_settextrise, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, moveTextPos, 				arginfo_harupage_movetextpos, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, fillStroke, 				arginfo_harupage_fillstroke, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, eoFillStroke, 				arginfo_harupage_eofillstroke, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, closePath, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, endPath, 					arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, ellipse, 					arginfo_harupage_ellipse, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, textRect, 					arginfo_harupage_textrect, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, moveToNextLine, 			arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setGrayFill, 				arginfo_harupage_setgraystroke, ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setGrayStroke, 			arginfo_harupage_setgraystroke, ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setRGBFill, 				arginfo_harupage_setrgbstroke, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setRGBStroke, 				arginfo_harupage_setrgbstroke, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setCMYKFill, 				arginfo_harupage_setcmykstroke,	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setCMYKStroke, 			arginfo_harupage_setcmykstroke, ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setWidth, 					arginfo_harupage_setwidth, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setHeight, 				arginfo_harupage_setheight, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setSize, 					arginfo_harupage_setsize, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setRotate, 				arginfo_harupage_setrotate, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getWidth, 					arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getHeight, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, createDestination, 		arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, createTextAnnotation, 		arginfo_harupage_createtextannotation, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, createLinkAnnotation, 		arginfo_harupage_createlinkannotation, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, createURLAnnotation, 		arginfo_harupage_createurlannotation, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getTextWidth, 				arginfo_harupage_gettextwidth, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, MeasureText, 				arginfo_harupage_measuretext, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getGMode, 					arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getCurrentPos, 			arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getCurrentTextPos, 		arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getCurrentFont, 			arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getCurrentFontSize, 		arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getLineWidth, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getLineCap, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getLineJoin, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getMiterLimit, 			arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getDash, 					arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getFlatness, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getCharSpace, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getWordSpace, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getHorizontalScaling, 		arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getTextLeading, 			arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getTextRenderingMode, 		arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getTextRise, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getRGBFill, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getRGBStroke, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getCMYKFill, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getCMYKStroke, 			arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getGrayFill, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getGrayStroke, 			arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getFillingColorSpace, 		arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, getStrokingColorSpace, 	arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, setSlideShow, 				arginfo_harupage_setslideshow, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, clip, arginfo_harudoc___void, ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, saveGS, arginfo_harudoc___void, ZEND_ACC_PUBLIC)
	PHP_ME(HaruPage, restoreGS, arginfo_harudoc___void, ZEND_ACC_PUBLIC)
#if defined(HPDF_VERSION_ID) && HPDF_VERSION_ID >= 20200
	PHP_ME(HaruPage, setZoom,					arginfo_harupage_setzoom,		ZEND_ACC_PUBLIC)
#endif
	{NULL, NULL, NULL}
};

/* }}} */

static zend_function_entry harufont_methods[] = { /* {{{ */
	PHP_ME(HaruFont, __construct, arginfo_harudoc___void, ZEND_ACC_PRIVATE)
	PHP_ME(HaruFont, getFontName, 		arginfo_harudoc___void, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruFont, getEncodingName, 	arginfo_harudoc___void, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruFont, getUnicodeWidth, 	arginfo_harufont_getunicodewidth, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruFont, getAscent, 		arginfo_harudoc___void, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruFont, getDescent, 		arginfo_harudoc___void, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruFont, getXHeight, 		arginfo_harudoc___void, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruFont, getCapHeight, 		arginfo_harudoc___void, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruFont, getTextWidth, 		arginfo_harufont_gettextwidth, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruFont, MeasureText, 		arginfo_harufont_measuretext, 		ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

/* }}} */

static zend_function_entry haruimage_methods[] = { /* {{{ */
	PHP_ME(HaruImage, __construct, 			arginfo_harudoc___void, 		ZEND_ACC_PRIVATE)
	PHP_ME(HaruImage, getSize, 				arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruImage, getWidth, 			arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruImage, getHeight, 			arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruImage, getBitsPerComponent, 	arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruImage, getColorSpace, 		arginfo_harudoc___void, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruImage, setColorMask, 		arginfo_haruimage_setcolormask, ZEND_ACC_PUBLIC)
	PHP_ME(HaruImage, setMaskImage, 		arginfo_haruimage_setmaskimage, ZEND_ACC_PUBLIC)
#if defined(HPDF_VERSION_ID) && HPDF_VERSION_ID >= 20200
	PHP_ME(HaruImage, addSMask,				arginfo_haruimage_addsmask,		ZEND_ACC_PUBLIC)
#endif
	{NULL, NULL, NULL}
};

/* }}} */

static zend_function_entry harudestination_methods[] = { /* {{{ */
	PHP_ME(HaruDestination, __construct, arginfo_harudoc___void, 			ZEND_ACC_PRIVATE)
	PHP_ME(HaruDestination, setXYZ, 	arginfo_harudestination_setxyz, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruDestination, setFit, 	arginfo_harudoc___void, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruDestination, setFitH, 	arginfo_harudestination_setfith, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruDestination, setFitV, 	arginfo_harudestination_setfitv, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruDestination, setFitR, 	arginfo_harudestination_setfitr, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruDestination, setFitB, 	arginfo_harudoc___void, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruDestination, setFitBH, 	arginfo_harudestination_setfitbh, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruDestination, setFitBV, 	arginfo_harudestination_setfitbv, 	ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

/* }}} */

static zend_function_entry haruannotation_methods[] = { /* {{{ */
	PHP_ME(HaruAnnotation, __construct, 		arginfo_harudoc___void, 					ZEND_ACC_PRIVATE)
	PHP_ME(HaruAnnotation, setHighlightMode, 	arginfo_haruannotation_sethighlightmode, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruAnnotation, setBorderStyle, 		arginfo_haruannotation_setborderstyle, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruAnnotation, setIcon, 			arginfo_haruannotation_seticon, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruAnnotation, setOpened, 			arginfo_haruannotation_setopened, 			ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

/* }}} */

static zend_function_entry haruencoder_methods[] = { /* {{{ */
	PHP_ME(HaruEncoder, __construct, 		arginfo_harudoc___void, 			ZEND_ACC_PRIVATE)
	PHP_ME(HaruEncoder, getType, 			arginfo_harudoc___void, 			ZEND_ACC_PUBLIC)
	PHP_ME(HaruEncoder, getByteType, 		arginfo_haruencoder_getbytetype, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruEncoder, getUnicode, 		arginfo_haruencoder_getunicode, 	ZEND_ACC_PUBLIC)
	PHP_ME(HaruEncoder, getWritingMode, 	arginfo_harudoc___void, 			ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

/* }}} */

static zend_function_entry haruoutline_methods[] = { /* {{{ */
	PHP_ME(HaruOutline, __construct, 	arginfo_harudoc___void, 			ZEND_ACC_PRIVATE)
	PHP_ME(HaruOutline, setOpened, 		arginfo_haruoutline_setopened, 		ZEND_ACC_PUBLIC)
	PHP_ME(HaruOutline, setDestination, arginfo_haruoutline_setdestination, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

static zend_function_entry haruexception_methods[] = { /* {{{ */
	{NULL, NULL, NULL}
};
/* }}} */

/* }}} */

static zend_function_entry haru_functions[] = { /* {{{ */
	{NULL, NULL, NULL}
};
/* }}} */

#ifdef COMPILE_DL_HARU

ZEND_GET_MODULE(haru)
#endif

#define HARU_CLASS_CONST(ce, name, value)												\
	zend_declare_class_constant_long(ce, name, sizeof(name)-1, (long)value TSRMLS_CC);

#define HARU_INIT_CLASS(uc_class_name, lc_class_name)														\
	memcpy(&php_##lc_class_name##_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));	\
	php_##lc_class_name##_handlers.clone_obj = NULL;														\
	INIT_CLASS_ENTRY(ce, uc_class_name, lc_class_name##_methods);											\
	ce.create_object = php_##lc_class_name##_new;															\
	ce_##lc_class_name = zend_register_internal_class(&ce TSRMLS_CC);

/* {{{ PHP_MINIT_FUNCTION
 */
static PHP_MINIT_FUNCTION(haru)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "HaruException", haruexception_methods);
	ce_haruexception = zend_register_internal_class_ex(&ce, zend_exception_get_default(TSRMLS_C), NULL TSRMLS_CC);

	HARU_INIT_CLASS("HaruDoc", harudoc);
	HARU_INIT_CLASS("HaruPage", harupage);
	HARU_INIT_CLASS("HaruFont", harufont);
	HARU_INIT_CLASS("HaruImage", haruimage);
	HARU_INIT_CLASS("HaruDestination", harudestination);
	HARU_INIT_CLASS("HaruAnnotation", haruannotation);
	HARU_INIT_CLASS("HaruEncoder", haruencoder);
	HARU_INIT_CLASS("HaruOutline", haruoutline);

	HARU_CLASS_CONST(ce_harudoc, "CS_DEVICE_GRAY", HPDF_CS_DEVICE_GRAY);
	HARU_CLASS_CONST(ce_harudoc, "CS_DEVICE_RGB", HPDF_CS_DEVICE_RGB);
	HARU_CLASS_CONST(ce_harudoc, "CS_DEVICE_CMYK", HPDF_CS_DEVICE_CMYK);
	HARU_CLASS_CONST(ce_harudoc, "CS_CAL_GRAY", HPDF_CS_CAL_GRAY);
	HARU_CLASS_CONST(ce_harudoc, "CS_CAL_RGB", HPDF_CS_CAL_RGB);
	HARU_CLASS_CONST(ce_harudoc, "CS_LAB", HPDF_CS_LAB);
	HARU_CLASS_CONST(ce_harudoc, "CS_ICC_BASED", HPDF_CS_ICC_BASED);
	HARU_CLASS_CONST(ce_harudoc, "CS_SEPARATION", HPDF_CS_SEPARATION);
	HARU_CLASS_CONST(ce_harudoc, "CS_DEVICE_N", HPDF_CS_DEVICE_N);
	HARU_CLASS_CONST(ce_harudoc, "CS_INDEXED", HPDF_CS_INDEXED);
	HARU_CLASS_CONST(ce_harudoc, "CS_PATTERN", HPDF_CS_PATTERN);

	HARU_CLASS_CONST(ce_harudoc, "ENABLE_READ", HPDF_ENABLE_READ);
	HARU_CLASS_CONST(ce_harudoc, "ENABLE_PRINT", HPDF_ENABLE_PRINT);
	HARU_CLASS_CONST(ce_harudoc, "ENABLE_EDIT_ALL", HPDF_ENABLE_EDIT_ALL);
	HARU_CLASS_CONST(ce_harudoc, "ENABLE_COPY", HPDF_ENABLE_COPY);
	HARU_CLASS_CONST(ce_harudoc, "ENABLE_EDIT", HPDF_ENABLE_EDIT);

	HARU_CLASS_CONST(ce_harudoc, "ENCRYPT_R2", HPDF_ENCRYPT_R2);
	HARU_CLASS_CONST(ce_harudoc, "ENCRYPT_R3", HPDF_ENCRYPT_R3);

	HARU_CLASS_CONST(ce_harudoc, "INFO_AUTHOR", HPDF_INFO_AUTHOR);
	HARU_CLASS_CONST(ce_harudoc, "INFO_CREATOR", HPDF_INFO_CREATOR);
	HARU_CLASS_CONST(ce_harudoc, "INFO_TITLE", HPDF_INFO_TITLE);
	HARU_CLASS_CONST(ce_harudoc, "INFO_SUBJECT", HPDF_INFO_SUBJECT);
	HARU_CLASS_CONST(ce_harudoc, "INFO_KEYWORDS", HPDF_INFO_KEYWORDS);
	HARU_CLASS_CONST(ce_harudoc, "INFO_CREATION_DATE", HPDF_INFO_CREATION_DATE);
	HARU_CLASS_CONST(ce_harudoc, "INFO_MOD_DATE", HPDF_INFO_MOD_DATE);

	HARU_CLASS_CONST(ce_harudoc, "COMP_NONE", HPDF_COMP_NONE);
	HARU_CLASS_CONST(ce_harudoc, "COMP_TEXT", HPDF_COMP_TEXT);
	HARU_CLASS_CONST(ce_harudoc, "COMP_IMAGE", HPDF_COMP_IMAGE);
	HARU_CLASS_CONST(ce_harudoc, "COMP_METADATA", HPDF_COMP_METADATA);
	HARU_CLASS_CONST(ce_harudoc, "COMP_ALL", HPDF_COMP_ALL);

	HARU_CLASS_CONST(ce_harudoc, "PAGE_LAYOUT_SINGLE", HPDF_PAGE_LAYOUT_SINGLE);
	HARU_CLASS_CONST(ce_harudoc, "PAGE_LAYOUT_ONE_COLUMN", HPDF_PAGE_LAYOUT_ONE_COLUMN);
	HARU_CLASS_CONST(ce_harudoc, "PAGE_LAYOUT_TWO_COLUMN_LEFT", HPDF_PAGE_LAYOUT_TWO_COLUMN_LEFT);
	HARU_CLASS_CONST(ce_harudoc, "PAGE_LAYOUT_TWO_COLUMN_RIGHT", HPDF_PAGE_LAYOUT_TWO_COLUMN_RIGHT);

	HARU_CLASS_CONST(ce_harudoc, "PAGE_MODE_USE_NONE", HPDF_PAGE_MODE_USE_NONE);
	HARU_CLASS_CONST(ce_harudoc, "PAGE_MODE_USE_OUTLINE", HPDF_PAGE_MODE_USE_OUTLINE);
	HARU_CLASS_CONST(ce_harudoc, "PAGE_MODE_USE_THUMBS", HPDF_PAGE_MODE_USE_THUMBS);
	HARU_CLASS_CONST(ce_harudoc, "PAGE_MODE_FULL_SCREEN", HPDF_PAGE_MODE_FULL_SCREEN);

	HARU_CLASS_CONST(ce_harupage, "GMODE_PAGE_DESCRIPTION", HPDF_GMODE_PAGE_DESCRIPTION);
	HARU_CLASS_CONST(ce_harupage, "GMODE_TEXT_OBJECT", HPDF_GMODE_TEXT_OBJECT);
	HARU_CLASS_CONST(ce_harupage, "GMODE_PATH_OBJECT", HPDF_GMODE_PATH_OBJECT);
	HARU_CLASS_CONST(ce_harupage, "GMODE_CLIPPING_PATH", HPDF_GMODE_CLIPPING_PATH);
	HARU_CLASS_CONST(ce_harupage, "GMODE_SHADING", HPDF_GMODE_SHADING);
	HARU_CLASS_CONST(ce_harupage, "GMODE_INLINE_IMAGE", HPDF_GMODE_INLINE_IMAGE);
	HARU_CLASS_CONST(ce_harupage, "GMODE_EXTERNAL_OBJECT", HPDF_GMODE_EXTERNAL_OBJECT);

	HARU_CLASS_CONST(ce_harupage, "BUTT_END", HPDF_BUTT_END);
	HARU_CLASS_CONST(ce_harupage, "ROUND_END", HPDF_ROUND_END);
	HARU_CLASS_CONST(ce_harupage, "PROJECTING_SCUARE_END", HPDF_PROJECTING_SCUARE_END);

	HARU_CLASS_CONST(ce_harupage, "MITER_JOIN", HPDF_MITER_JOIN);
	HARU_CLASS_CONST(ce_harupage, "ROUND_JOIN", HPDF_ROUND_JOIN);
	HARU_CLASS_CONST(ce_harupage, "BEVEL_JOIN", HPDF_BEVEL_JOIN);

	HARU_CLASS_CONST(ce_harupage, "FILL", HPDF_FILL);
	HARU_CLASS_CONST(ce_harupage, "STROKE", HPDF_STROKE);
	HARU_CLASS_CONST(ce_harupage, "FILL_THEN_STROKE", HPDF_FILL_THEN_STROKE);
	HARU_CLASS_CONST(ce_harupage, "INVISIBLE", HPDF_INVISIBLE);
	HARU_CLASS_CONST(ce_harupage, "FILL_CLIPPING", HPDF_FILL_CLIPPING);
	HARU_CLASS_CONST(ce_harupage, "STROKE_CLIPPING", HPDF_STROKE_CLIPPING);
	HARU_CLASS_CONST(ce_harupage, "FILL_STROKE_CLIPPING", HPDF_FILL_STROKE_CLIPPING);
	HARU_CLASS_CONST(ce_harupage, "CLIPPING", HPDF_CLIPPING);

	HARU_CLASS_CONST(ce_harupage, "TALIGN_LEFT", HPDF_TALIGN_LEFT);
	HARU_CLASS_CONST(ce_harupage, "TALIGN_RIGHT", HPDF_TALIGN_RIGHT);
	HARU_CLASS_CONST(ce_harupage, "TALIGN_CENTER", HPDF_TALIGN_CENTER);
	HARU_CLASS_CONST(ce_harupage, "TALIGN_JUSTIFY", HPDF_TALIGN_JUSTIFY);

	HARU_CLASS_CONST(ce_harupage, "SIZE_LETTER", HPDF_PAGE_SIZE_LETTER);
	HARU_CLASS_CONST(ce_harupage, "SIZE_LEGAL", HPDF_PAGE_SIZE_LEGAL);
	HARU_CLASS_CONST(ce_harupage, "SIZE_A3", HPDF_PAGE_SIZE_A3);
	HARU_CLASS_CONST(ce_harupage, "SIZE_A4", HPDF_PAGE_SIZE_A4);
	HARU_CLASS_CONST(ce_harupage, "SIZE_A5", HPDF_PAGE_SIZE_A5);
	HARU_CLASS_CONST(ce_harupage, "SIZE_B4", HPDF_PAGE_SIZE_B4);
	HARU_CLASS_CONST(ce_harupage, "SIZE_B5", HPDF_PAGE_SIZE_B5);
	HARU_CLASS_CONST(ce_harupage, "SIZE_EXECUTIVE", HPDF_PAGE_SIZE_EXECUTIVE);
	HARU_CLASS_CONST(ce_harupage, "SIZE_US4x6", HPDF_PAGE_SIZE_US4x6);
	HARU_CLASS_CONST(ce_harupage, "SIZE_US4x8", HPDF_PAGE_SIZE_US4x8);
	HARU_CLASS_CONST(ce_harupage, "SIZE_US5x7", HPDF_PAGE_SIZE_US5x7);
	HARU_CLASS_CONST(ce_harupage, "SIZE_COMM10", HPDF_PAGE_SIZE_COMM10);

	HARU_CLASS_CONST(ce_harupage, "PORTRAIT", HPDF_PAGE_PORTRAIT);
	HARU_CLASS_CONST(ce_harupage, "LANDSCAPE", HPDF_PAGE_LANDSCAPE);

	HARU_CLASS_CONST(ce_harupage, "TS_WIPE_LIGHT", HPDF_TS_WIPE_RIGHT);
	HARU_CLASS_CONST(ce_harupage, "TS_WIPE_UP", HPDF_TS_WIPE_UP);
	HARU_CLASS_CONST(ce_harupage, "TS_WIPE_LEFT", HPDF_TS_WIPE_LEFT);
	HARU_CLASS_CONST(ce_harupage, "TS_WIPE_DOWN", HPDF_TS_WIPE_DOWN);
	HARU_CLASS_CONST(ce_harupage, "TS_BARN_DOORS_HORIZONTAL_OUT", HPDF_TS_BARN_DOORS_HORIZONTAL_OUT);
	HARU_CLASS_CONST(ce_harupage, "TS_BARN_DOORS_HORIZONTAL_IN", HPDF_TS_BARN_DOORS_HORIZONTAL_IN);
	HARU_CLASS_CONST(ce_harupage, "TS_BARN_DOORS_VERTICAL_OUT", HPDF_TS_BARN_DOORS_VERTICAL_OUT);
	HARU_CLASS_CONST(ce_harupage, "TS_BARN_DOORS_VERTICAL_IN", HPDF_TS_BARN_DOORS_VERTICAL_IN);
	HARU_CLASS_CONST(ce_harupage, "TS_BOX_OUT", HPDF_TS_BOX_OUT);
	HARU_CLASS_CONST(ce_harupage, "TS_BOX_IN", HPDF_TS_BOX_IN);
	HARU_CLASS_CONST(ce_harupage, "TS_BLINDS_HORIZONTAL", HPDF_TS_BLINDS_HORIZONTAL);
	HARU_CLASS_CONST(ce_harupage, "TS_BLINDS_VERTICAL", HPDF_TS_BLINDS_VERTICAL);
	HARU_CLASS_CONST(ce_harupage, "TS_DISSOLVE", HPDF_TS_DISSOLVE);
	HARU_CLASS_CONST(ce_harupage, "TS_GLITTER_RIGHT", HPDF_TS_GLITTER_RIGHT);
	HARU_CLASS_CONST(ce_harupage, "TS_GLITTER_DOWN", HPDF_TS_GLITTER_DOWN);
	HARU_CLASS_CONST(ce_harupage, "TS_GLITTER_TOP_LEFT_TO_BOTTOM_RIGHT", HPDF_TS_GLITTER_TOP_LEFT_TO_BOTTOM_RIGHT);
	HARU_CLASS_CONST(ce_harupage, "TS_REPLACE", HPDF_TS_REPLACE);

	HARU_CLASS_CONST(ce_harupage, "NUM_STYLE_DECIMAL", HPDF_PAGE_NUM_STYLE_DECIMAL);
	HARU_CLASS_CONST(ce_harupage, "NUM_STYLE_UPPER_ROMAN", HPDF_PAGE_NUM_STYLE_UPPER_ROMAN);
	HARU_CLASS_CONST(ce_harupage, "NUM_STYLE_LOWER_ROMAN", HPDF_PAGE_NUM_STYLE_LOWER_ROMAN);
	HARU_CLASS_CONST(ce_harupage, "NUM_STYLE_UPPER_LETTERS", HPDF_PAGE_NUM_STYLE_UPPER_LETTERS);
	HARU_CLASS_CONST(ce_harupage, "NUM_STYLE_LOWER_LETTERS", HPDF_PAGE_NUM_STYLE_LOWER_LETTERS);

	HARU_CLASS_CONST(ce_haruencoder, "TYPE_SINGLE_BYTE", HPDF_ENCODER_TYPE_SINGLE_BYTE);
	HARU_CLASS_CONST(ce_haruencoder, "TYPE_DOUBLE_BYTE", HPDF_ENCODER_TYPE_DOUBLE_BYTE);
	HARU_CLASS_CONST(ce_haruencoder, "TYPE_UNINITIALIZED", HPDF_ENCODER_TYPE_UNINITIALIZED);
	HARU_CLASS_CONST(ce_haruencoder, "UNKNOWN", HPDF_ENCODER_UNKNOWN);

	HARU_CLASS_CONST(ce_haruencoder, "BYTE_TYPE_SINGLE", HPDF_BYTE_TYPE_SINGLE);
	HARU_CLASS_CONST(ce_haruencoder, "BYTE_TYPE_LEAD", HPDF_BYTE_TYPE_LEAD);
	HARU_CLASS_CONST(ce_haruencoder, "BYTE_TYPE_TRAIL", HPDF_BYTE_TYPE_TRIAL); /* note the typo in the original name.. */
	HARU_CLASS_CONST(ce_haruencoder, "BYTE_TYPE_UNKNOWN", HPDF_BYTE_TYPE_UNKNOWN);

	HARU_CLASS_CONST(ce_haruencoder, "WMODE_HORIZONTAL", HPDF_WMODE_HORIZONTAL);
	HARU_CLASS_CONST(ce_haruencoder, "WMODE_VERTICAL", HPDF_WMODE_VERTICAL);

	HARU_CLASS_CONST(ce_haruannotation, "NO_HIGHLIGHT", HPDF_ANNOT_NO_HIGHTLIGHT);
	HARU_CLASS_CONST(ce_haruannotation, "INVERT_BOX", HPDF_ANNOT_INVERT_BOX);
	HARU_CLASS_CONST(ce_haruannotation, "INVERT_BORDER", HPDF_ANNOT_INVERT_BORDER);
	HARU_CLASS_CONST(ce_haruannotation, "DOWN_APPEARANCE", HPDF_ANNOT_DOWN_APPEARANCE);

	HARU_CLASS_CONST(ce_haruannotation, "ICON_COMMENT", HPDF_ANNOT_ICON_COMMENT);
	HARU_CLASS_CONST(ce_haruannotation, "ICON_KEY", HPDF_ANNOT_ICON_KEY);
	HARU_CLASS_CONST(ce_haruannotation, "ICON_NOTE", HPDF_ANNOT_ICON_NOTE);
	HARU_CLASS_CONST(ce_haruannotation, "ICON_HELP", HPDF_ANNOT_ICON_HELP);
	HARU_CLASS_CONST(ce_haruannotation, "ICON_NEW_PARAGRAPH", HPDF_ANNOT_ICON_NEW_PARAGRAPH);
	HARU_CLASS_CONST(ce_haruannotation, "ICON_PARAGRAPH", HPDF_ANNOT_ICON_PARAGRAPH);
	HARU_CLASS_CONST(ce_haruannotation, "ICON_INSERT", HPDF_ANNOT_ICON_INSERT);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
static PHP_MINFO_FUNCTION(haru)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Haru PDF support", "enabled");
	php_info_print_table_row(2, "Version", PHP_HARU_VERSION);
	php_info_print_table_row(2, "libharu version", HPDF_VERSION_TEXT);
	php_info_print_table_end();

}
/* }}} */

/* {{{ haru_module_entry
 */
zend_module_entry haru_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"haru",
	haru_functions,
	PHP_MINIT(haru),
	NULL,
	NULL,
	NULL,
	PHP_MINFO(haru),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_HARU_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
