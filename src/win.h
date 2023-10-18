/* Copyright (c) 2014
 *      Alexander Naumov (alexander_naumov@opensuse.org)
 *
 * This file is part of GNU screen.
 *
 * GNU screen is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, see
 * <https://www.gnu.org/licenses>.
 *
 ****************************************************************
 */

#ifndef SCREEN_WIN_H
#define SCREEN_WIN_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "window.h"
#include "backtick.h"
#include "screen.h"

/* Default window message buffer size */
#define WINMSGBUF_SIZE  MAXSTR
#define RENDBUF_SIZE    128 /* max rendition byte count */
#define MAX_WINMSG_REND 256 /* rendition changes */

/* TODO: complete truncation and rendition API */

/* Represents a working buffer for window messages */
typedef struct {
	char     *buf;
	size_t    size;
	uint64_t  rend[MAX_WINMSG_REND];
	int       rendpos[MAX_WINMSG_REND];
	int       numrend;
} WinMsgBuf;

typedef struct {
	WinMsgBuf *buf;
	char      *p;    /* pointer within buffer */

	/* truncation mark */
	struct {
		/* starting position of truncation; TODO: make this size_t and remove
		 * -1 as inactive indicator */
		int pos;
		/* target offset percentage relative to pos and trunc operator */
		uint8_t perc;
		/* whether to show ellipses */
		bool ellip;
	} trunc;
} WinMsgBufContext;

/* represents a window message condition (e.g. %?)*/
typedef struct {
        int  offset;  /* offset in dest string */
        bool initialized;
        bool state;   /* conditional truth value */
        bool locked;  /* when set, prevents state from changing */
} WinMsgCond;

/* WinMsgCond is intended to be used as an opaque type */
void wmc_init(WinMsgCond *, int);
void wmc_set(WinMsgCond *);
void wmc_clear(WinMsgCond *);
bool wmc_is_active(const WinMsgCond *);
bool wmc_is_set(const WinMsgCond *);
int  wmc_else(WinMsgCond *, int, bool *);
int  wmc_end(const WinMsgCond *, int, bool *);
void wmc_deinit(WinMsgCond *);


WinMsgBuf *wmb_create(void);
void wmb_reset(WinMsgBuf *);
size_t wmb_expand(WinMsgBuf *, size_t);
void wmb_rendadd(WinMsgBuf *, uint64_t, int);
size_t wmb_size(const WinMsgBuf *);
const char *wmb_contents(const WinMsgBuf *);
void wmb_reset(WinMsgBuf *);
void wmb_free(WinMsgBuf *);

WinMsgBufContext *wmbc_create(WinMsgBuf *);
void wmbc_rewind(WinMsgBufContext *);
void wmbc_fastfw0(WinMsgBufContext *);
void wmbc_fastfw_end(WinMsgBufContext *);
void wmbc_putchar(WinMsgBufContext *, char);
const char *wmbc_strncpy(WinMsgBufContext *, const char *, size_t);
const char *wmbc_strcpy(WinMsgBufContext *, const char *);
int wmbc_printf(WinMsgBufContext *, const char *, ...)
                __attribute__((format(printf,2,3)));
size_t wmbc_offset(WinMsgBufContext *);
size_t wmbc_bytesleft(WinMsgBufContext *);
const char *wmbc_mergewmb(WinMsgBufContext *, WinMsgBuf *);
const char *wmbc_finish(WinMsgBufContext *);
void wmbc_free(WinMsgBufContext *);

/* escape characters (alphabetical order) */
typedef enum {
        WINESC_HOUR            = 'A',
        WINESC_hour            = 'a',
        WINESC_TIME            = 'C',
        WINESC_time            = 'c',
        WINESC_DAY             = 'D',
        WINESC_day             = 'd',
        WINESC_ESC_SEEN        = 'E',
        WINESC_FOCUS           = 'F',
        WINESC_WFLAGS          = 'f',
        WINESC_WIN_GROUP       = 'g',
        WINESC_HOST            = 'H',
        WINESC_HSTATUS         = 'h',
        WINESC_MONTH           = 'M',
        WINESC_month           = 'm',
        WINESC_WIN_LOGNAME     = 'N',
        WINESC_WIN_NUM         = 'n',
        WINESC_WIN_COUNT       = 'O',
        WINESC_PID             = 'p',
        WINESC_COPY_MODE       = 'P',  /* copy/_P_aste mode */
        WINESC_SESS_NAME       = 'S',
        WINESC_WIN_SIZE        = 's',
        WINESC_WIN_TTY         = 'T',
        WINESC_WIN_TITLE       = 't',
        WINESC_WIN_NAMES_NOCUR = 'W',
        WINESC_WIN_NAMES       = 'w',
        WINESC_CMD             = 'X',
        WINESC_CMD_ARGS        = 'x',
        WINESC_YEAR            = 'Y',
        WINESC_year            = 'y',
        WINESC_REND_START      = '{',
        WINESC_REND_END        = '}',
        WINESC_REND_POP        = '-',
        WINESC_COND            = '?',  /* start and end delimiter */
        WINESC_COND_ELSE       = ':',
        WINESC_BACKTICK        = '`',
        WINESC_PAD             = '=',
        WINESC_TRUNC           = '<',
        WINESC_TRUNC_POS       = '>',
} WinMsgEscapeChar;

/* escape sequence */
typedef struct {
        int num;
        struct {
                bool zero  : 1;
                bool lng   : 1;
                bool minus : 1;
                bool plus  : 1;
        } flags;
} WinMsgEsc;

char *MakeWinMsg(char *, Window *, int);
char *MakeWinMsgEv(WinMsgBuf *, char *, Window *, int, int, Event *, int);
int   AddWinMsgRend(WinMsgBuf *, const char *, uint64_t);
void  WindowChanged (Window *, WinMsgEscapeChar);

extern WinMsgBuf *g_winmsg;

#endif
