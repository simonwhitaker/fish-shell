// This file only contains fallback implementations of functions which have been found to be missing
// or broken by the configuration scripts.
//
// Many of these functions are more or less broken and incomplete. lrand28_r internally uses the
// regular (bad) rand_r function, the gettext function doesn't actually do anything, etc.
#include "config.h"

// IWYU likes to recommend adding term.h when we want ncurses.h.
// IWYU pragma: no_include term.h
#include <dirent.h>  // IWYU pragma: keep
#include <errno.h>   // IWYU pragma: keep
#include <fcntl.h>   // IWYU pragma: keep
#include <limits.h>  // IWYU pragma: keep
#include <stdarg.h>  // IWYU pragma: keep
#include <stdio.h>   // IWYU pragma: keep
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>   // IWYU pragma: keep
#include <sys/types.h>  // IWYU pragma: keep
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <algorithm>
#if HAVE_GETTEXT
#include <libintl.h>
#endif
#if HAVE_CURSES_H
#include <curses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>  // IWYU pragma: keep
#elif HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#endif
#if HAVE_TERM_H
#include <term.h>  // IWYU pragma: keep
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif
#include <signal.h>  // IWYU pragma: keep
#include <wchar.h>   // IWYU pragma: keep

#include "common.h"    // IWYU pragma: keep
#include "fallback.h"  // IWYU pragma: keep
#include "util.h"      // IWYU pragma: keep

#ifdef TPARM_SOLARIS_KLUDGE
#undef tparm

char *tparm_solaris_kludge(char *str, long p1, long p2, long p3, long p4,
                           long p5, long p6, long p7, long p8, long p9) {
    return tparm(str, p1, p2, p3, p4, p5, p6, p7, p8, p9);
}

// Re-defining just to make sure nothing breaks further down in this file.
#define tparm tparm_solaris_kludge

#endif

int fish_mkstemp_cloexec(char *name_template) {
#if HAVE_MKOSTEMP
    // null check because mkostemp may be a weak symbol
    if (&mkostemp != nullptr) {
        return mkostemp(name_template, O_CLOEXEC);
    }
#endif
    int result_fd = mkstemp(name_template);
    if (result_fd != -1) {
        fcntl(result_fd, F_SETFD, FD_CLOEXEC);
    }
    return result_fd;
}

/// Fallback implementations of wcsdup and wcscasecmp. On systems where these are not needed (e.g.
/// building on Linux) these should end up just being stripped, as they are static functions that
/// are not referenced in this file.
// cppcheck-suppress unusedFunction
__attribute__((unused)) static wchar_t *wcsdup_fallback(const wchar_t *in) {
    size_t len = wcslen(in);
    wchar_t *out = (wchar_t *)malloc(sizeof(wchar_t) * (len + 1));
    if (out == 0) {
        return 0;
    }

    memcpy(out, in, sizeof(wchar_t) * (len + 1));
    return out;
}

__attribute__((unused)) static int wcscasecmp_fallback(const wchar_t *a, const wchar_t *b) {
    if (*a == 0) {
        return *b == 0 ? 0 : -1;
    } else if (*b == 0) {
        return 1;
    }
    int diff = towlower(*a) - towlower(*b);
    if (diff != 0) {
        return diff;
    }
    return wcscasecmp_fallback(a + 1, b + 1);
}

__attribute__((unused)) static int wcsncasecmp_fallback(const wchar_t *a, const wchar_t *b,
                                                        size_t count) {
    if (count == 0) return 0;

    if (*a == 0) {
        return *b == 0 ? 0 : -1;
    } else if (*b == 0) {
        return 1;
    }
    int diff = towlower(*a) - towlower(*b);
    if (diff != 0) return diff;
    return wcsncasecmp_fallback(a + 1, b + 1, count - 1);
}

#if __APPLE__
#if __DARWIN_C_LEVEL >= 200809L
// Note parens avoid the macro expansion.
wchar_t *wcsdup_use_weak(const wchar_t *a) {
    if (&wcsdup != NULL) return (wcsdup)(a);
    return wcsdup_fallback(a);
}

int wcscasecmp_use_weak(const wchar_t *a, const wchar_t *b) {
    if (&wcscasecmp != NULL) return (wcscasecmp)(a, b);
    return wcscasecmp_fallback(a, b);
}

int wcsncasecmp_use_weak(const wchar_t *s1, const wchar_t *s2, size_t n) {
    if (&wcsncasecmp != NULL) return (wcsncasecmp)(s1, s2, n);
    return wcsncasecmp_fallback(s1, s2, n);
}
#else   // __DARWIN_C_LEVEL >= 200809L
wchar_t *wcsdup(const wchar_t *in) { return wcsdup_fallback(in); }
int wcscasecmp(const wchar_t *a, const wchar_t *b) { return wcscasecmp_fallback(a, b); }
int wcsncasecmp(const wchar_t *a, const wchar_t *b, size_t n) {
    return wcsncasecmp_fallback(a, b, n);
}
#endif  // __DARWIN_C_LEVEL >= 200809L
#else   // __APPLE__

#ifndef HAVE_WCSDUP
#ifndef HAVE_STD__WCSDUP
wchar_t *wcsdup(const wchar_t *in) { return wcsdup_fallback(in); }
#endif
#endif

#ifndef HAVE_WCSCASECMP
#ifndef HAVE_STD__WCSCASECMP
int wcscasecmp(const wchar_t *a, const wchar_t *b) { return wcscasecmp_fallback(a, b); }
#endif
#endif

#ifndef HAVE_WCSNCASECMP
#ifndef HAVE_STD__WCSNCASECMP
int wcsncasecmp(const wchar_t *a, const wchar_t *b, size_t n) {
    return wcsncasecmp_fallback(a, b, n);
}
#endif
#endif

#endif  // __APPLE__

#ifndef HAVE_WCSNDUP
wchar_t *wcsndup(const wchar_t *in, size_t c) {
    wchar_t *res = (wchar_t *)malloc(sizeof(wchar_t) * (c + 1));
    if (res == 0) {
        return 0;
    }
    wcslcpy(res, in, c + 1);
    return res;
}
#endif

#ifndef HAVE_WCSLCPY
/*$OpenBSD: strlcpy.c,v 1.8 2003/06/17 21:56:24 millert Exp $*/

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
size_t wcslcpy(wchar_t *dst, const wchar_t *src, size_t siz) {
    wchar_t *d = dst;
    const wchar_t *s = src;
    size_t n = siz;

    // Copy as many bytes as will fit.
    if (n != 0 && --n != 0) {
        do {
            if ((*d++ = *s++) == 0) break;
        } while (--n != 0);
    }

    // Not enough room in dst, add NUL and traverse rest of src.
    if (n == 0) {
        if (siz != 0) *d = '\0';  // NUL-terminate dst
        while (*s++)
            ;  // ignore rest of src
    }
    return s - src - 1;  // count does not include NUL
}
#endif

#if 0
// These are not currently used.
#ifndef HAVE_LRAND48_R
int lrand48_r(struct drand48_data *buffer, long int *result) {
    *result = rand_r(&buffer->seed);
    return 0;
}

int srand48_r(long int seedval, struct drand48_data *buffer) {
    buffer->seed = (unsigned int)seedval;
    return 0;
}
#endif
#endif

#ifndef HAVE_FUTIMES
int futimes(int fd, const struct timeval *times) {
    errno = ENOSYS;
    return -1;
}
#endif

#if HAVE_GETTEXT
char *fish_gettext(const char *msgid) {
    return gettext(msgid);
    ;
}

char *fish_bindtextdomain(const char *domainname, const char *dirname) {
    return bindtextdomain(domainname, dirname);
}

char *fish_textdomain(const char *domainname) { return textdomain(domainname); }
#else
char *fish_gettext(const char *msgid) { return (char *)msgid; }
char *fish_bindtextdomain(const char *domainname, const char *dirname) {
    UNUSED(domainname);
    UNUSED(dirname);
    return NULL;
}
char *fish_textdomain(const char *domainname) {
    UNUSED(domainname);
    return NULL;
}
#endif

#ifndef HAVE_KILLPG
int killpg(int pgr, int sig) {
    assert(pgr > 1);
    return kill(-pgr, sig);
}
#endif

int g_fish_emoji_width = 0;

// 1 is the typical emoji width in Unicode 8.
int g_guessed_fish_emoji_width = 1;

int fish_get_emoji_width(wchar_t c) {
    (void)c;
    // Respect an explicit value. If we don't have one, use the guessed value. Do not try to fall
    // back to wcwidth(), it's hopeless.
    if (g_fish_emoji_width > 0) return g_fish_emoji_width;
    return g_guessed_fish_emoji_width;
}

// Big hack to use our versions of wcswidth where we know them to be broken, which is
// EVERYWHERE (https://github.com/fish-shell/fish-shell/issues/2199)
#ifndef HAVE_BROKEN_WCWIDTH
#define HAVE_BROKEN_WCWIDTH 1
#endif

#if !HAVE_BROKEN_WCWIDTH
int fish_wcwidth(wchar_t wc) { return wcwidth(wc); }
int fish_wcswidth(const wchar_t *str, size_t n) { return wcswidth(str, n); }
#else

#include "wcwidth9/wcwidth9.h"

// This is the sort listed of inclusive ranges of characters whose width was 1 in Unicode 8, but was
// changed to width 2 in Unicode 9. Note that no characters became narrower from Unicode 8 to 9.
static bool is_width_2_in_Uni9_but_1_in_Uni8(wchar_t c) {
    const struct pair_t {
        int lo;
        int hi;
    } pairs[] = {{0x0231A, 0x0231B}, {0x023E9, 0x023EC}, {0x023F0, 0x023F0}, {0x023F3, 0x023F3},
                 {0x025FD, 0x025FE}, {0x02614, 0x02615}, {0x02648, 0x02653}, {0x0267F, 0x0267F},
                 {0x02693, 0x02693}, {0x026A1, 0x026A1}, {0x026AA, 0x026AB}, {0x026BD, 0x026BE},
                 {0x026C4, 0x026C5}, {0x026CE, 0x026CE}, {0x026D4, 0x026D4}, {0x026EA, 0x026EA},
                 {0x026F2, 0x026F3}, {0x026F5, 0x026F5}, {0x026FA, 0x026FA}, {0x026FD, 0x026FD},
                 {0x02705, 0x02705}, {0x0270A, 0x0270B}, {0x02728, 0x02728}, {0x0274C, 0x0274C},
                 {0x0274E, 0x0274E}, {0x02753, 0x02755}, {0x02757, 0x02757}, {0x02795, 0x02797},
                 {0x027B0, 0x027B0}, {0x027BF, 0x027BF}, {0x02B1B, 0x02B1C}, {0x02B50, 0x02B50},
                 {0x02B55, 0x02B55}, {0x16FE0, 0x16FE0}, {0x17000, 0x187EC}, {0x18800, 0x18AF2},
                 {0x1F004, 0x1F004}, {0x1F0CF, 0x1F0CF}, {0x1F18E, 0x1F18E}, {0x1F191, 0x1F19A},
                 {0x1F23B, 0x1F23B}, {0x1F300, 0x1F320}, {0x1F32D, 0x1F335}, {0x1F337, 0x1F37C},
                 {0x1F37E, 0x1F393}, {0x1F3A0, 0x1F3CA}, {0x1F3CF, 0x1F3D3}, {0x1F3E0, 0x1F3F0},
                 {0x1F3F4, 0x1F3F4}, {0x1F3F8, 0x1F43E}, {0x1F440, 0x1F440}, {0x1F442, 0x1F4FC},
                 {0x1F4FF, 0x1F53D}, {0x1F54B, 0x1F54E}, {0x1F550, 0x1F567}, {0x1F57A, 0x1F57A},
                 {0x1F595, 0x1F596}, {0x1F5A4, 0x1F5A4}, {0x1F5FB, 0x1F64F}, {0x1F680, 0x1F6C5},
                 {0x1F6CC, 0x1F6CC}, {0x1F6D0, 0x1F6D2}, {0x1F6EB, 0x1F6EC}, {0x1F6F4, 0x1F6F6},
                 {0x1F910, 0x1F91E}, {0x1F920, 0x1F927}, {0x1F930, 0x1F930}, {0x1F933, 0x1F93E},
                 {0x1F940, 0x1F94B}, {0x1F950, 0x1F95E}, {0x1F980, 0x1F991}, {0x1F9C0, 0x1F9C0}};
    auto where = std::lower_bound(std::begin(pairs), std::end(pairs), c,
                                  [](pair_t p, wchar_t c) { return p.hi < c; });
    assert((where == std::end(pairs) || where->hi >= c) && "unexpected binary search result");
    return where != std::end(pairs) && where->lo <= c;
}

// Return whether wc is a variation selector, which wcwidth() is likely to mishandle (#2652).
static bool is_variation_selector(wchar_t wc) {
    return (0xFE00 <= wc && wc <= 0xFE0F)       // variation selectors
           || (0xE0100 <= wc && wc <= 0xE01EF)  // variation selector supplement
           || (0x180B <= wc && wc <= 0x180D);   // Mongolian variation selectors
}

// Possible negative return values from wcwidth9()
enum { width_non_printable = -1, width_ambiguous = -2, width_private_use = -3 };

int fish_wcwidth(wchar_t wc) {
    // Check for certain characters whose width is terminal emulator dependent.
    if (is_width_2_in_Uni9_but_1_in_Uni8(wc)) return fish_get_emoji_width(wc);

    // Check for variation selectors which system wcwidth mishandles (see #2652).
    if (is_variation_selector(wc)) return -1;

    int w9_width = wcwidth9(wc);
    if (w9_width >= 0) return w9_width;

    // Fall back to system wcwidth().
    int sys_width = wcwidth(wc);
    if (sys_width >= 0) return sys_width;

    // Treat ambiguous and private use widths as 1.
    if (w9_width == width_ambiguous || w9_width == width_private_use) return 1;

    return -1;
}

int fish_wcswidth(const wchar_t *str, size_t n) {
    int result = 0;
    for (size_t i = 0; i < n && str[i] != L'\0'; i++) {
        int w = fish_wcwidth(str[i]);
        if (w < 0) {
            result = -1;
            break;
        }
        result += w;
    }
    return result;
}

#endif  // HAVE_BROKEN_WCWIDTH

#ifndef HAVE_FLOCK
/*	$NetBSD: flock.c,v 1.6 2008/04/28 20:24:12 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Todd Vierling.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Emulate flock() with fcntl().
 */

int flock(int fd, int op) {
    int rc = 0;

    struct flock fl = {0};

    switch (op & (LOCK_EX | LOCK_SH | LOCK_UN)) {
        case LOCK_EX:
            fl.l_type = F_WRLCK;
            break;

        case LOCK_SH:
            fl.l_type = F_RDLCK;
            break;

        case LOCK_UN:
            fl.l_type = F_UNLCK;
            break;

        default:
            errno = EINVAL;
            return -1;
    }

    fl.l_whence = SEEK_SET;
    rc = fcntl(fd, op & LOCK_NB ? F_SETLK : F_SETLKW, &fl);

    if (rc && (errno == EAGAIN)) errno = EWOULDBLOCK;

    return rc;
}

#endif  // HAVE_FLOCK
