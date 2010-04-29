/* Yash: yet another shell */
/* strbuf.c: modifiable string buffer */
/* (C) 2007-2009 magicant */

/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#include "common.h"
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include "strbuf.h"
#include "util.h"

#define XSTRBUF_INITSIZE 15
#define XWCSBUF_INITSIZE 15


/* If the type of the return value of the functions below is string buffer,
 * the return value is the argument buffer. */


/********** Multibyte String Buffer **********/

/* Initializes the specified string buffer as an empty string. */
xstrbuf_T *sb_init(xstrbuf_T *buf)
{
    buf->contents = xmalloc((XSTRBUF_INITSIZE + 1) * sizeof (char));
    buf->contents[0] = '\0';
    buf->length = 0;
    buf->maxlength = XSTRBUF_INITSIZE;
    return buf;
}

/* Initializes the specified multibyte string buffer with the specified string.
 * String `s' must be `free'able.
 * After calling this function, the string is used as the buffer, so you must
 * not touch or `free' it any more. */
xstrbuf_T *sb_initwith(xstrbuf_T *restrict buf, char *restrict s)
{
    buf->contents = s;
    buf->length = buf->maxlength = strlen(s);
    return buf;
}

/* Changes the max length of the specified buffer.
 * If `newmax' is less than the current length of the buffer, the end of
 * the buffer contents is truncated. */
xstrbuf_T *sb_setmax(xstrbuf_T *buf, size_t newmax)
{
    buf->contents = xrealloc(buf->contents, (newmax + 1) * sizeof (char));
    buf->maxlength = newmax;
    buf->contents[newmax] = '\0';
    if (newmax < buf->length)
	buf->length = newmax;
    return buf;
}

/* If `buf->maxlength' is less than `max', reallocates the buffer so that
 * `buf->maxlength' is no less than `max'. */
xstrbuf_T *sb_ensuremax(xstrbuf_T *buf, size_t max)
{
    if (max <= buf->maxlength)
	return buf;

    size_t len15 = buf->maxlength + (buf->maxlength >> 1);
    if (max < len15)
	max = len15;
    if (max < buf->maxlength + 10)
	max = buf->maxlength + 10;
    return sb_setmax(buf, max);
}

/* Replaces the specified part of the buffer with another string.
 * `bn' characters starting at offset `i' in buffer `buf' is removed and
 * the first `sn' characters of `s' take place of them.
 * No boundary checks are done and null characters are not considered special.
 * `s' must not be part of `buf->contents'. */
xstrbuf_T *sb_replace_force(
	xstrbuf_T *restrict buf, size_t i, size_t bn,
	const char *restrict s, size_t sn)
{
    size_t newlength = buf->length - bn + sn;
    sb_ensuremax(buf, newlength);
    memmove(buf->contents + i + sn, buf->contents + i + bn,
	    buf->length - (i + bn) + 1);
    memcpy(buf->contents + i, s, sn);
    buf->length = newlength;
    return buf;
}

/* Replaces the specified part of the buffer with another string.
 * `bn' characters starting at offset `i' in buffer `buf' is removed and
 * the first `sn' characters of `s' take place of them.
 * If (strlen(s) < sn), the whole of `s' is replaced with.
 * If (buf->length < i + sn), all the characters after offset `i' in the buffer
 * is replaced. Especially, if (buf->length <= i), `s' is appended.
 * `s' must not be part of `buf->contents'. */
xstrbuf_T *sb_replace(
	xstrbuf_T *restrict buf, size_t i, size_t bn,
	const char *restrict s, size_t sn)
{
    sn = xstrnlen(s, sn);
    if (i > buf->length)
	i = buf->length;
    if (bn > buf->length - i)
	bn = buf->length - i;
    return sb_replace_force(buf, i, bn, s, sn);
}

/* Appends byte `c' to the end of string buffer `buf'.
 * The byte is appended even if it is a null byte. */
xstrbuf_T *sb_ccat(xstrbuf_T *buf, char c)
{
    sb_ensuremax(buf, buf->length + 1);
    buf->contents[buf->length++] = c;
    buf->contents[buf->length] = '\0';
    return buf;
}

/* Appends `n' bytes of `c' to the end of buffer `buf'.
 * The bytes are appended even if `c' is a null byte. */
xstrbuf_T *sb_ccat_repeat(xstrbuf_T *buf, char c, size_t n)
{
    sb_ensuremax(buf, buf->length + n);
    memset(buf->contents + buf->length, c, n);
    buf->length += n;
    buf->contents[buf->length] = '\0';
    return buf;
}

/* Converts wide character `c' into a multibyte string and appends it to buffer
 * `buf'. Shift state `ps' is used for the conversion.
 * Returns true iff successful.
 * If `c' is a null character, the shift state is reset to the initial state but
 * the null character is not appended to the buffer. */
bool sb_wccat(xstrbuf_T *restrict buf, wchar_t c, mbstate_t *restrict ps)
{
    size_t count;

    sb_ensuremax(buf, buf->length + MB_CUR_MAX);
    count = wcrtomb(buf->contents + buf->length, c, ps);
    if (count == (size_t) -1) {
	buf->contents[buf->length] = '\0';
	return false;
    }
    assert(0 < count && count <= buf->maxlength - buf->length);
    buf->length += count;
    if (c == L'\0')
	buf->length--, assert(buf->contents[buf->length] == '\0');
    else
	buf->contents[buf->length] = '\0';
    return true;
}

/* Converts wide string `s' to a multibyte string and appends it to buffer
 * `buf'. Shift state `ps' is used for the conversion. After successful
 * conversion, `ps' will be the initial shift state.
 * Returns NULL if the whole string is converted and appended successfully,
 * otherwise a pointer to the character in `s' that caused the error.
 * A partial result may be left in the buffer on error. */
wchar_t *sb_wcscat(xstrbuf_T *restrict buf,
	const wchar_t *restrict s, mbstate_t *restrict ps)
{
    for (;;) {
	size_t count = wcsrtombs(buf->contents + buf->length,
		(const wchar_t **) &s,
		buf->maxlength - buf->length + 1,
		ps);
	if (count == (size_t) -1) {
	    buf->contents[buf->length] = '\0';
	    break;
	}
	buf->length += count;
	if (s == NULL)
	    break;
	sb_ensuremax(buf, buf->maxlength + 1);
    }
    assert(buf->contents[buf->length] == '\0');
    return (wchar_t *) s;
}

/* Appends the result of `vsprintf' to the specified buffer.
 * `format' and the following arguments must not be part of `buf->contents'.
 * Returns the number of appended bytes if successful.
 * On error, the buffer is not changed and -1 is returned. */
int sb_vprintf(xstrbuf_T *restrict buf, const char *restrict format, va_list ap)
{
    va_list copyap;
    va_copy(copyap, ap);

    int rest = buf->maxlength - buf->length + 1;
    int result = vsnprintf(buf->contents + buf->length, rest, format, ap);

    if (result >= rest) {
	/* If the buffer is too small... */
	sb_ensuremax(buf, buf->length + result);
	rest = buf->maxlength - buf->length + 1;
	result = vsnprintf(buf->contents + buf->length, rest, format, copyap);
    }
    assert(result < rest);
    if (result >= 0)
	buf->length += result;
    else
	buf->contents[buf->length] = '\0';
    assert(buf->contents[buf->length] == '\0');
    va_end(copyap);
    return result;
}

/* Appends the result of `sprintf' to the specified buffer.
 * `format' and the following arguments must not be part of `buf->contents'.
 * Returns the number of appended bytes if successful.
 * On error, the buffer is not changed and -1 is returned. */
int sb_printf(xstrbuf_T *restrict buf, const char *restrict format, ...)
{
    va_list ap;
    int result;

    va_start(ap, format);
    result = sb_vprintf(buf, format, ap);
    va_end(ap);
    return result;
}


/********** Wide String Buffer **********/

/* Initializes the specified wide string buffer as an empty string. */
xwcsbuf_T *wb_init(xwcsbuf_T *buf)
{
    buf->contents = xmalloc((XWCSBUF_INITSIZE + 1) * sizeof (wchar_t));
    buf->contents[0] = L'\0';
    buf->length = 0;
    buf->maxlength = XWCSBUF_INITSIZE;
    return buf;
}

/* Initializes the specified wide string buffer with the specified string.
 * String `s' must be `free'able.
 * After calling this function, the string is used as the buffer, so you must
 * not touch or `free' it any more. */
xwcsbuf_T *wb_initwith(xwcsbuf_T *restrict buf, wchar_t *restrict s)
{
    buf->contents = s;
    buf->length = buf->maxlength = wcslen(s);
    return buf;
}

/* Changes the max length of the specified buffer.
 * If `newmax' is less than the current length of the buffer, the end of
 * the buffer contents is truncated. */
xwcsbuf_T *wb_setmax(xwcsbuf_T *buf, size_t newmax)
{
    buf->contents = xrealloc(buf->contents, (newmax + 1) * sizeof (wchar_t));
    buf->maxlength = newmax;
    buf->contents[newmax] = L'\0';
    if (newmax < buf->length)
	buf->length = newmax;
    return buf;
}

/* If `buf->maxlength' is less than `max', reallocates the buffer so that
 * `buf->maxlength' is no less than `max'. */
xwcsbuf_T *wb_ensuremax(xwcsbuf_T *buf, size_t max)
{
    if (max <= buf->maxlength)
	return buf;

    size_t len15 = buf->maxlength + (buf->maxlength >> 1);
    if (max < len15)
	max = len15;
    if (max < buf->maxlength + 8)
	max = buf->maxlength + 8;
    return wb_setmax(buf, max);
}

/* Replaces the specified part of the buffer with another string.
 * `bn' characters starting at offset `i' in buffer `buf' is removed and
 * the first `sn' characters of `s' take place of them.
 * No boundary checks are done and null characters are not considered special.
 * `s' must not be part of `buf->contents'. */
xwcsbuf_T *wb_replace_force(
	xwcsbuf_T *restrict buf, size_t i, size_t bn,
	const wchar_t *restrict s, size_t sn)
{
    size_t newlength = buf->length - bn + sn;
    wb_ensuremax(buf, newlength);
    wmemmove(buf->contents + i + sn, buf->contents + i + bn,
	    buf->length - (i + bn) + 1);
    wmemcpy(buf->contents + i, s, sn);
    buf->length = newlength;
    return buf;
}

/* Replaces the specified part of the buffer with another string.
 * `bn' characters starting at offset `i' in buffer `buf' is removed and
 * the first `sn' characters of `s' take place of them.
 * If (wcslen(s) < sn), the whole of `s' is replaced with.
 * If (buf->length < i + sn), all the characters after offset `i' in the buffer
 * is replaced. Especially, if (buf->length <= i), `s' is appended.
 * `s' must not be part of `buf->contents'. */
xwcsbuf_T *wb_replace(
	xwcsbuf_T *restrict buf, size_t i, size_t bn,
	const wchar_t *restrict s, size_t sn)
{
    sn = xwcsnlen(s, sn);
    if (i > buf->length)
	i = buf->length;
    if (bn > buf->length - i)
	bn = buf->length - i;
    return wb_replace_force(buf, i, bn, s, sn);
}

/* Appends wide character `c' to the end of buffer `buf'.
 * The character is appended even if it is a null wide character. */
xwcsbuf_T *wb_wccat(xwcsbuf_T *buf, wchar_t c)
{
    wb_ensuremax(buf, buf->length + 1);
    buf->contents[buf->length++] = c;
    buf->contents[buf->length] = L'\0';
    return buf;
}

/* Appends `n' characters of `c' to the end of buffer `buf'.
 * The characters are appended even if `c' is a null wide character. */
xwcsbuf_T *wb_wccat_repeat(xwcsbuf_T *buf, wchar_t c, size_t n)
{
    wb_ensuremax(buf, buf->length + n);
    wmemset(buf->contents + buf->length, c, n);
    buf->length += n;
    buf->contents[buf->length] = L'\0';
    return buf;
}

/* Converts multibyte string `s' into a wide string and appends it to buffer
 * `buf'. The multibyte string is assumed to start in the initial shift state.
 * Returns NULL if the whole string is converted and appended successfully,
 * otherwise a pointer to the character in `s' that caused the error.
 * A partial result may be left in the buffer on error. */
char *wb_mbscat(xwcsbuf_T *restrict buf, const char *restrict s)
{
    mbstate_t state;
    size_t count;

    memset(&state, 0, sizeof state);  // initialize as the initial shift state

    for (;;) {
	count = mbsrtowcs(buf->contents + buf->length, (const char **) &s,
		buf->maxlength - buf->length + 1, &state);
	if (count == (size_t) -1)
	    break;
	buf->length += count;
	if (s == NULL)
	    break;
	wb_ensuremax(buf, buf->maxlength + 1);
    }

    buf->contents[buf->length] = L'\0';
    return (char *) s;
}

/* Appends the result of `vswprintf' to the specified buffer.
 * `format' and the following arguments must not be part of `buf->contents'.
 * Returns the number of appended characters if successful.
 * On error, the buffer is not changed and -1 is returned. */
int wb_vwprintf(
	xwcsbuf_T *restrict buf, const wchar_t *restrict format, va_list ap)
{
    va_list copyap;
    int rest, result;

    for (int i = 0; i < 10; i++) {
	va_copy(copyap, ap);
	rest = buf->maxlength - buf->length + 1;
	result = vswprintf(buf->contents + buf->length, rest, format, copyap);
	va_end(copyap);

	if (0 <= result && result < rest)
	    break;

	/* According to POSIX, if the buffer is too short, `vswprintf' returns
	 * a negative integer. On some systems, however, it returns a desired
	 * buffer length as `vsprintf' does, which is rather preferable. */
	wb_ensuremax(buf, buf->length + (result < 0 ? 2 * rest : result));
    }
    if (result >= 0)
	buf->length += result;
    else
	buf->contents[buf->length] = L'\0';
    assert(buf->contents[buf->length] == L'\0');
    return result;
}

/* Appends the result of `swprintf' to the specified buffer.
 * `format' and the following arguments must not be part of `buf->contents'.
 * Returns the number of appended characters if successful.
 * On error, the buffer is not changed and -1 is returned. */
int wb_wprintf(xwcsbuf_T *restrict buf, const wchar_t *restrict format, ...)
{
    va_list ap;
    int result;

    va_start(ap, format);
    result = wb_vwprintf(buf, format, ap);
    va_end(ap);
    return result;
}



/********** Multibyte-Wide Conversion Utilities **********/

/* Converts the specified wide string into a newly malloced multibyte string.
 * Only the first `n' characters of `s' is converted at most.
 * Returns NULL on error.
 * The resulting string starts and ends in the initial shift state.*/
char *malloc_wcsntombs(const wchar_t *s, size_t n)
{
    size_t nn = xwcsnlen(s, n);
    if (s[nn] == L'\0')
	return malloc_wcstombs(s);

    wchar_t ss[nn + 1];
    wcsncpy(ss, s, nn);
    ss[nn] = L'\0';
    return malloc_wcstombs(ss);
}

/* Converts the specified wide string into a newly malloced multibyte string.
 * Returns NULL on error.
 * The resulting string starts and ends in the initial shift state.*/
char *malloc_wcstombs(const wchar_t *s)
{
    xstrbuf_T buf;
    mbstate_t state;

    sb_init(&buf);
    memset(&state, 0, sizeof state);  // initialize as the initial shift state
    if (sb_wcscat(&buf, s, &state) == NULL) {
	return sb_tostr(&buf);
    } else {
	sb_destroy(&buf);
	return NULL;
    }
}

/* Converts the specified multibyte string into a newly malloced wide string.
 * Only the first `n' bytes of `s' is converted at most.
 * Returns NULL on error.
 * The multibyte string is assumed to start in the initial shift state. */
wchar_t *malloc_mbsntowcs(const char *s, size_t n)
{
    size_t nn = xstrnlen(s, n);
    if (s[nn] == '\0')
	return malloc_mbstowcs(s);

    char ss[nn + 1];
    strncpy(ss, s, nn);
    ss[nn] = '\0';
    return malloc_mbstowcs(ss);
}

/* Converts the specified multibyte string into a newly malloced wide string.
 * Returns NULL on error. */
wchar_t *malloc_mbstowcs(const char *s)
{
    xwcsbuf_T buf;

    wb_init(&buf);
    if (wb_mbscat(&buf, s) == NULL) {
	return wb_towcs(&buf);
    } else {
	wb_destroy(&buf);
	return NULL;
    }
}


/********** Formatting Utilities **********/

/* Returns the result of `sprintf' as a newly malloced string. */
char *malloc_printf(const char *format, ...)
{
    va_list ap;
    char *result;
    va_start(ap, format);
    result = malloc_vprintf(format, ap);
    va_end(ap);
    return result;
}

/* Returns the result of `swprintf' as a newly malloced string. */
wchar_t *malloc_wprintf(const wchar_t *format, ...)
{
    va_list ap;
    wchar_t *result;
    va_start(ap, format);
    result = malloc_vwprintf(format, ap);
    va_end(ap);
    return result;
}


/* vim: set ts=8 sts=4 sw=4 noet tw=80: */
