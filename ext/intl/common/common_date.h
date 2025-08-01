/*
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Gustavo Lopes <cataphract@php.net>                          |
   +----------------------------------------------------------------------+
*/

#ifndef COMMON_DATE_H
#define	COMMON_DATE_H

#include <unicode/umachine.h>

U_CDECL_BEGIN
#include <php.h>
#include "../intl_error.h"
U_CDECL_END

#ifdef __cplusplus

#include <unicode/timezone.h>

using icu::TimeZone;

U_CFUNC TimeZone *timezone_convert_datetimezone(int type, void *object, bool is_datetime, intl_error *outside_error);
U_CFUNC zend_result intl_datetime_decompose(zend_object *obj, double *millis, TimeZone **tz, intl_error *err);

#endif

U_CFUNC double intl_zval_to_millis(zval *z, intl_error *err);

#endif	/* COMMON_DATE_H */
