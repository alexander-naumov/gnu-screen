/* Copyrigth (c) 2024
 *      Alexander Naumov (alexander_naumov@opensuse.org)
 * Copyright (c) 2013
 *      Mike Gerwitz (mtg@gnu.org)
 * Copyright (c) 2010
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Sadrul Habib Chowdhury (sadrul@users.sourceforge.net)
 * Copyright (c) 2008, 2009
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 *      Micah Cowan (micah@cowan.name)
 *      Sadrul Habib Chowdhury (sadrul@users.sourceforge.net)
 * Copyright (c) 1993-2002, 2003, 2005, 2006, 2007
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1987 Oliver Laumann
 *
 * This program is free software; you can redistribute it and/or modify
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
 * https://www.gnu.org/licenses/, or contact Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 ****************************************************************
 */

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "config.h"
#include "win.h"
#include "screen.h"
#include "fileio.h"
#include "help.h"
#include "logfile.h"
#include "mark.h"
#include "process.h"
#include "sched.h"

/* TODO: rid global variable (has been renamed to point this out; see commit
 * history) */
WinMsgBuf *g_winmsg;

#define CHRPAD 127

/* maximum limit on MakeWinMsgEv recursion */
#define WINMSG_RECLIMIT 10

#ifndef USE_LOCALE
static const char days[]   = "SunMonTueWedThuFriSat";
static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
#endif

/* escape char for backtick output */
#define WINMSG_BT_ESC '\005'

/* redundant definition abstraction for escape character handlers; note that
 * a variable varadic macro name is a gcc extension and is not portable, so
 * we instead use two separate macros */
#define WINMSG_ESC_PARAMS \
	__attribute__((unused)) WinMsgEsc *esc, \
	__attribute__((unused)) char **src, \
	__attribute__((unused)) WinMsgBufContext *wmbc, \
	__attribute__((unused)) WinMsgCond *cond
#define winmsg_esc__name(name) __WinMsgEsc##name
#define winmsg_esc__def(name) static void winmsg_esc__name(name)
#define winmsg_esc(name) winmsg_esc__def(name)(WINMSG_ESC_PARAMS)
#define winmsg_esc_ex(name, ...) winmsg_esc__def(name)(WINMSG_ESC_PARAMS, __VA_ARGS__)
#define WINMSG_ESC_ARGS &esc, &s, wmbc, cond
#define WinMsgDoEsc(name) winmsg_esc__name(name)(WINMSG_ESC_ARGS)
#define WinMsgDoEscEx(name, ...) winmsg_esc__name(name)(WINMSG_ESC_ARGS, __VA_ARGS__)

static void _MakeWinMsgEvRec(WinMsgBufContext *, WinMsgCond *, char *, Window *, int *, int);



/* Allocate and initialize to the empty string a new window message buffer. The
 * return value must be freed using wmbc_free. */
WinMsgBuf *wmb_create(void)
{
        WinMsgBuf *w = malloc(sizeof(WinMsgBuf));
        if (w == NULL)
                return NULL;

        w->buf = malloc(WINMSGBUF_SIZE);
        if (w->buf == NULL) {
                free(w);
                return NULL;
        }

        w->size = WINMSGBUF_SIZE;
        wmb_reset(w);
        return w;
}

/* Attempts to expand the buffer to hold at least MIN bytes. The new size of the
 * buffer is returned, which may be unchanged from the original size if
 * additional memory could not be allocated. */
size_t wmb_expand(WinMsgBuf *wmb, size_t min)
{
        size_t size = wmb->size;

        if (size >= min)
                return size;

        /* keep doubling the buffer until we reach at least the requested size; this
         * ensures that we'll always be a power of two (so long as the original
         * buffer size was) and doubling will help cut down on excessive allocation
         * requests on large buffers */
        while (size < min) {
                size *= 2;
        }

        void *p = realloc(wmb->buf, size);
        if (p == NULL) {
                /* reallocation failed; maybe the caller can do without? */
                return wmb->size;
        }

        /* realloc already handled the free for us */
        wmb->buf = p;
        wmb->size = size;
        return size;
}

/* Add a rendition to the buffer */
void wmb_rendadd(WinMsgBuf *wmb, uint64_t r, int offset)
{
        /* TODO: lift arbitrary limit; dynamically allocate */
        if (wmb->numrend >= MAX_WINMSG_REND)
                return;

        wmb->rend[wmb->numrend] = r;
        wmb->rendpos[wmb->numrend] = offset;
        wmb->numrend++;
}

/* Retrieve buffer size. This returns the total size of the buffer, not how much
 * has been used. */
size_t wmb_size(const WinMsgBuf *wmb)
{
        return wmb->size;
}

/* Retrieve a pointer to the raw buffer contents. This should not be used to
 * modify the buffer. */
const char *wmb_contents(const WinMsgBuf *wmb)
{
        return wmb->buf;
}

/* Initializes window buffer to the empty string; useful for re-using an
 * existing buffer without allocating a new one. */
void wmb_reset(WinMsgBuf *w)
{
        *w->buf = '\0';
        w->numrend = 0;
}

/* Deinitialize and free memory allocated to the given window buffer */
void wmb_free(WinMsgBuf *w)
{
        free(w->buf);
        free(w);
}


/* Allocate and initialize a buffer context for the given buffer. The return
 * value must be freed using wmbc_free. */
WinMsgBufContext *wmbc_create(WinMsgBuf *w)
{
        if (w == NULL)
                return NULL;

        WinMsgBufContext *c = malloc(sizeof(WinMsgBufContext));
        if (c == NULL)
                return NULL;

        c->buf = w;
        c->p = w->buf;
        return c;
}

/* Rewind pointer to the first byte of the buffer. */
void wmbc_rewind(WinMsgBufContext *wmbc)
{
        wmbc->p = wmbc->buf->buf;
}

/* Place pointer at terminating null character. */
void wmbc_fastfw0(WinMsgBufContext *wmbc)
{
        wmbc->p += strlen(wmbc->p);
}

/* Place pointer just past the last byte in the buffer, ignoring terminating null
 * characters. The next write will trigger an expansion. */
void wmbc_fastfw_end(WinMsgBufContext *wmbc)
{
        wmbc->p = wmbc->buf->buf + wmbc->buf->size;
}

/* Attempts buffer expansion and updates context pointers appropriately. The
 * result is true if expansion succeeded, otherwise false. */
static bool _wmbc_expand(WinMsgBufContext *wmbc, size_t size)
{
        size_t offset = wmbc_offset(wmbc);

        if (wmb_expand(wmbc->buf, size) < size) {
                return false;
        }

        /* the buffer address may have changed; re-calculate pointer address */
        wmbc->p = wmbc->buf->buf + offset;
        return true;
}

/* Sets a character at the current buffer position and increments the pointer.
 * The terminating null character is not retained. The buffer will be
 * dynamically resized as needed. */
void wmbc_putchar(WinMsgBufContext *wmbc, char c)
{
        /* attempt to accomodate this character, but bail out silenty if it cannot
         * fit */
        if (!wmbc_bytesleft(wmbc)) {
                if (!_wmbc_expand(wmbc, wmbc->buf->size + 1)) {
                        return;
                }
        }

        *wmbc->p++ = c;
}

/* Copies a string into the buffer, dynamically resizing the buffer as needed to
 * accomodate length N. If S is shorter than N characters in length, the
 * remaining bytes are filled will nulls. The context pointer is adjusted to the
 * terminating null byte. A pointer to the first copied character in the buffer
 * is returned; it shall not be used to modify the buffer. */
const char *wmbc_strncpy(WinMsgBufContext *wmbc, const char *s, size_t n)
{
        size_t l = wmbc_bytesleft(wmbc);

        /* silently fail in the event that we cannot accomodate */
        if (l < n) {
                size_t size = wmbc->buf->size + (n - l);
                if (!_wmbc_expand(wmbc, size)) {
                        /* TODO: we should copy what can fit. */
                        return NULL;
                }
        }

        char *p = wmbc->p;
        strncpy(wmbc->p, s, n);
        wmbc->p += n;
        return p;
}

/* Copies a string into the buffer, dynamically resizing the buffer as needed to
 * accomodate the length of the string sans its terminating null byte. The
 * context pointer is adjusted to the the terminiating null byte. A pointer to
 * the first copied character in the destination buffer is returned; it shall
 * not be used to modify the buffer. */
const char *wmbc_strcpy(WinMsgBufContext *wmbc, const char *s)
{
        return wmbc_strncpy(wmbc, s, strlen(s));
}

/* Write data to the buffer using a printf-style format string. If needed, the
 * buffer will be automatically expanded to accomodate the resulting string and
 * is therefore protected against overflows. */
int wmbc_printf(WinMsgBufContext *wmbc, const char *fmt, ...)
{
        va_list ap;
        size_t  n, max;

        /* to prevent buffer overflows, cap the number of bytes to the remaining
         * buffer size */
        va_start(ap, fmt);
        max = wmbc_bytesleft(wmbc);
        n = vsnprintf(wmbc->p, max, fmt, ap);
        va_end(ap);

        /* more space is needed if vsnprintf returns a larger number than our max,
         * in which case we should accomodate by dynamically resizing the buffer and
         * trying again */
        if (n > max) {
                if (!_wmbc_expand(wmbc, wmb_size(wmbc->buf) + n - max)) {
                        /* failed to allocate additional memory; this will simply have to do */
                        wmbc_fastfw_end(wmbc);
                        return max;
                }

                va_start(ap, fmt);
                size_t m = vsnprintf(wmbc->p, n + 1, fmt, ap);
                assert(m == n); /* this should never fail */
                va_end(ap);
        }

        wmbc_fastfw0(wmbc);
        return n;
}

/* Retrieve the 0-indexed offset of the context pointer into the buffer */
size_t wmbc_offset(WinMsgBufContext *wmbc)
{
        ptrdiff_t offset = wmbc->p - wmbc->buf->buf;

        /* when using wmbc_* functions (as one always should), the offset should
         * always be within the bounds of the buffer or one byte outside of it
         * (the latter case would require an expansion before writing) */
        assert(offset > -1);
        assert((size_t)offset <= wmbc->buf->size);

        return (size_t)offset;
}

/* Calculate the number of bytes remaining in the buffer relative to the current
 * position within the buffer */
size_t wmbc_bytesleft(WinMsgBufContext *wmbc)
{
        return wmbc->buf->size - wmbc_offset(wmbc);
}

/* Merges the contents of another null-terminated buffer and its renditions. The
 * return value is a pointer to the first character of WMB's buffer. */
const char *wmbc_mergewmb(WinMsgBufContext *wmbc, WinMsgBuf *wmb)
{
        const char *p;
        size_t offset = wmbc_offset(wmbc);

        /* import buffer contents into our own at our current position */
        assert(wmb);
        p = wmbc_strcpy(wmbc, wmb->buf);

        /* merge renditions, adjusting them to reflect their new offset */
        for (int i = 0; i < wmb->numrend; i++) {
                wmb_rendadd(wmbc->buf, wmb->rend[i], offset + wmb->rendpos[i]);
        }

        return p;
}

/* Write a terminating null byte to the buffer and return a pointer to the
 * buffer contents. This should not be used to modify the buffer. If buffer is
 * full and expansion fails, then the last byte in the buffer will be replaced
 * with the null byte. */
const char *wmbc_finish(WinMsgBufContext *wmbc)
{
        if (!wmbc_bytesleft(wmbc)) {
                size_t size = wmbc->buf->size + 1;
                if (wmb_expand(wmbc->buf, size) < size) {
                        /* we must terminate the string or we may cause big problems for the
                         * caller; overwrite the last char :x */
                        wmbc->p--;
                }
        }

        *wmbc->p = '\0';
        return wmb_contents(wmbc->buf);
}

/* Deinitializes and frees previously allocated context. The contained buffer
 * must be freed separately; this function will not do so for you. */
void wmbc_free(WinMsgBufContext *c)
{
        free(c);
}
















/* Initialize new condition and set to false; can be used to re-initialize a
 * condition for re-use */
void wmc_init(WinMsgCond *cond, int offset)
{       
        cond->locked = false;
        cond->offset = offset;
        cond->initialized = true;
        wmc_clear(cond);
}

/* Mark condition as true */
void wmc_set(WinMsgCond *cond)
{       
        if (cond->locked)
                return;
        
        cond->state = true;
}

/* Clear condition (equivalent to non-match) */
void wmc_clear(WinMsgCond *cond)
{       
        if (cond->locked)
                return;
        
        cond->state = false;
}

/* Determine if condition is active (has been initialized and can be used) */
bool wmc_is_active(const WinMsgCond *cond)
{
        return cond->initialized;
}

/* Determine if a condition is true; the result is undefined if
 * !wmc_active(cond) */
bool wmc_is_set(const WinMsgCond *cond)
{
        return cond->state;
}

/* "else" encounted */
int wmc_else(WinMsgCond *cond, int offset, bool *changed)
{
        assert(wmc_is_active(cond));

        /* if we're already set, then there is no point in processing the "else";
         * we will therefore consider the previous condition to have succeeded, so
         * now keep track of the start of the else so that it may be clobbered
         * instead of the beginning of the true condition---that is, we're accepting
         * the destination string up until this point */
        if (wmc_is_set(cond)) {
                wmc_init(cond, offset);  /* track this as a new condition */
                cond->locked = true;  /* "else" shall never succeed at this point */

                /* we want to keep the string we have so far (the truth string) */
                if (changed)
                        *changed = false;
                return offset;
        }

        /* now that we have reached "else" and are not true, we can never be true;
         * discard the truth part of the string */
        int prevoffset = cond->offset;
        cond->offset = offset;

        /* the "else" part must always be true at this point, because the previous
         * condition failed */
        wmc_set(cond);
        cond->locked = true;

        if (changed)
                *changed = true;
        return prevoffset;
}



/* End condition and determine if string should be reset or kept---if our value
 * is truthful, then accept the string, otherwise reject and reset to the
 * position that we were initialized with */
int wmc_end(const WinMsgCond *cond, int offset, bool *changed)
{
        bool set = wmc_is_set(cond);
        if (changed)
                *changed = !set;

        return (set) ? offset : cond->offset;
}

/* Deactivate a condition, preventing its use; this allows a single allocation
 * to be re-used and ignored until activated */
void wmc_deinit(WinMsgCond *cond)
{
        cond->state = false;
        cond->offset = 0;
        cond->locked = true;
        cond->initialized = false;
}






/* TODO: remove the redundant arguments */
static char *pad_expand(WinMsgBuf *winmsg, char *buf, char *p, int numpad, int padlen)
{
	char *pn, *pn2;
	int i, r;

	padlen = padlen - (p - buf);	/* space for rent */
	if (padlen < 0)
		padlen = 0;
	pn2 = pn = p + padlen;
	r = winmsg->numrend;
	while (p >= buf) {
		if (r && *p != CHRPAD && p - buf == winmsg->rendpos[r - 1]) {
			winmsg->rendpos[--r] = pn - buf;
			continue;
		}
		*pn-- = *p;
		if (*p-- == CHRPAD) {
			pn[1] = ' ';
			i = numpad > 0 ? (padlen + numpad - 1) / numpad : 0;
			padlen -= i;
			while (i-- > 0)
				*pn-- = ' ';
			numpad--;
			if (r && p - buf == winmsg->rendpos[r - 1])
				winmsg->rendpos[--r] = pn - buf;
		}
	}
	return pn2;
}

int AddWinMsgRend(WinMsgBuf *winmsg, const char *str, uint64_t r)
{
	if (winmsg->numrend >= MAX_WINMSG_REND || str < winmsg->buf || str >= winmsg->buf + MAXSTR)
		return -1;

	wmb_rendadd(winmsg, r, str - winmsg->buf);
	return 0;
}


winmsg_esc_ex(Wflags, Window *win)
{
	*wmbc->p = '\0';

	if (win)
		AddWindowFlags(wmbc->p, wmbc_bytesleft(wmbc), win);

	if (*wmbc->p)
		wmc_set(cond);

	wmbc_fastfw0(wmbc);
}

winmsg_esc(Pid)
{
	wmbc_printf(wmbc, "%d", (esc->flags.plus && display) ? D_userpid : getpid());
}

winmsg_esc_ex(Backtick, int id, Window *win, int *tick, struct timeval *now, int rec)
{
	Backtick *bt;
	char *btresult;

	if (!(bt = bt_find_id(id)))
		return;

	/* TODO: not re-entrant; static buffer returned */
	btresult = runbacktick(bt, tick, now->tv_sec);
	_MakeWinMsgEvRec(wmbc, cond, btresult, win, tick, rec);
}

winmsg_esc_ex(CopyMode, Event *ev)
{
	if (display && ev && ev != &D_hstatusev) {	/* Hack */
		/* Is the layer in the current canvas in copy mode? */
		Canvas *cv = (Canvas *)ev->data;
		if (ev == &cv->c_captev && cv->c_layer->l_layfn == &MarkLf)
			wmc_set(cond);
	}
}

winmsg_esc(EscSeen)
{
	if (display && D_ESCseen) {
		wmc_set(cond);
	}
}

winmsg_esc_ex(Focus, Window *win, Event *ev)
{
	/* small hack (TODO: explain.) */
	if (display && ((ev && ev == &D_forecv->c_captev) || (!ev && win && win == D_fore)))
		esc->flags.minus ^= 1;

	if (esc->flags.minus)
		wmc_set(cond);
}

winmsg_esc(HostName)
{
	if (*wmbc_strcpy(wmbc, HostName))
		wmc_set(cond);
}

winmsg_esc_ex(Hstatus, Window *win, int *tick, int rec)
{
	if (!win || win->w_hstatus == NULL || *win->w_hstatus == '\0')
		return;

	_MakeWinMsgEvRec(wmbc, cond, win->w_hstatus, win, tick, rec);
}

winmsg_esc_ex(PadOrTrunc, int *numpad, int *lastpad, int padlen)
{
	/* TODO: encapsulate */
	WinMsgBuf *winmsg = wmbc->buf;
	uint64_t r;

	wmbc_putchar(wmbc, ' ');
	wmbc->p--; /* TODO: temporary to work with old code */

	if (esc->num || esc->flags.zero || esc->flags.plus || esc->flags.lng || (**src != WINESC_PAD)) {
		/* expand all pads */
		if (esc->flags.minus) {
			esc->num = (esc->flags.plus ? *lastpad : padlen) - esc->num;

			if (!esc->flags.plus && padlen == 0)
				esc->num = wmbc->p - winmsg->buf;

			esc->flags.plus = 0;
		} else if (!esc->flags.zero) {
			if (**src != WINESC_PAD && esc->num == 0 && !esc->flags.plus)
				esc->num = 100;

			if (esc->num > 100)
				esc->num = 100;

			if (padlen == 0)
				esc->num = wmbc->p - winmsg->buf;
			else
				esc->num = (padlen - (esc->flags.plus ? *lastpad : 0)) * esc->num / 100;
		}

		if (esc->num < 0)
			esc->num = 0;

		if (esc->flags.plus)
			esc->num += *lastpad;

		if (esc->num > MAXSTR - 1)
			esc->num = MAXSTR - 1;

		if (*numpad)
			wmbc->p = pad_expand(winmsg, winmsg->buf, wmbc->p, *numpad, esc->num);

		*numpad = 0;
		if (wmbc->p - winmsg->buf > esc->num && !esc->flags.lng) {
			int left, trunc;

			if (wmbc->trunc.pos == -1) {
				wmbc->trunc.pos = *lastpad;
				wmbc->trunc.perc = 0;
			}

			trunc = *lastpad + wmbc->trunc.perc * (esc->num - *lastpad) / 100;
			if (trunc > esc->num)
				trunc = esc->num;
			if (trunc < *lastpad)
				trunc = *lastpad;

			left = wmbc->trunc.pos - trunc;
			if (left > wmbc->p - winmsg->buf - esc->num)
				left = wmbc->p - winmsg->buf - esc->num;

			if (left > 0) {
				if (left + *lastpad > wmbc->p - winmsg->buf)
					left = wmbc->p - winmsg->buf - *lastpad;

				if (wmbc->p - winmsg->buf - *lastpad - left > 0)
					memmove(winmsg->buf + *lastpad, winmsg->buf + *lastpad + left,
						wmbc->p - winmsg->buf - *lastpad - left);

				wmbc->p -= left;
				r = winmsg->numrend;
				while (r && winmsg->rendpos[r - 1] > *lastpad) {
					r--;
					winmsg->rendpos[r] -= left;
					if (winmsg->rendpos[r] < *lastpad)
						winmsg->rendpos[r] = *lastpad;
				}

				if (wmbc->trunc.ellip) {
					if (wmbc->p - winmsg->buf > *lastpad)
						winmsg->buf[*lastpad] = '.';
					if (wmbc->p - winmsg->buf > *lastpad + 1)
						winmsg->buf[*lastpad + 1] = '.';
					if (wmbc->p - winmsg->buf > *lastpad + 2)
						winmsg->buf[*lastpad + 2] = '.';
				}
			}

			if (wmbc->p - winmsg->buf > esc->num) {
				wmbc->p = winmsg->buf + esc->num;
				if (wmbc->trunc.ellip) {
					if (esc->num - 1 >= *lastpad)
						wmbc->p[-1] = '.';
					if (esc->num - 2 >= *lastpad)
						wmbc->p[-2] = '.';
					if (esc->num - 3 >= *lastpad)
						wmbc->p[-3] = '.';
				}

				r = winmsg->numrend;
				while (r && winmsg->rendpos[r - 1] > esc->num)
					winmsg->rendpos[--r] = esc->num;
			}

			wmbc->trunc.pos = -1;
			wmbc->trunc.ellip = false;

			if (*lastpad > wmbc->p - winmsg->buf)
				*lastpad = wmbc->p - winmsg->buf;
		}

		if (**src == WINESC_PAD) {
			while (wmbc->p - winmsg->buf < esc->num)
				wmbc_putchar(wmbc, ' ');

			*lastpad = wmbc->p - winmsg->buf;
			wmbc->trunc.pos = -1;
			wmbc->trunc.ellip = false;
		}
	} else if (padlen) {
		*wmbc->p = CHRPAD;	/* internal pad representation */
		(*numpad)++;
	}

	wmbc->p++; /* TODO: temporary; see above */
}

/**
 * Processes rendition
 *
 * The first character of SRC is assumed to be (unverified) the opening brace
 * of the sequence.
 */
winmsg_esc(Rend)
{
	char rbuf[RENDBUF_SIZE];
	uint8_t i;
	uint64_t r;

	(*src)++;
	for (i = 0; i < (RENDBUF_SIZE-1); i++) {
		char c = (*src)[i];
		if (c && c != WINESC_REND_END)
			rbuf[i] = c;
		else
			break;
	}

	if (((*src)[i] == WINESC_REND_END) && (wmbc->buf->numrend < MAX_WINMSG_REND)) {
		r = 0;
		rbuf[i] = '\0';
		if (i != 1 || rbuf[0] != WINESC_REND_POP)
			r = ParseAttrColor(rbuf, 0);
		AddWinMsgRend(wmbc->buf, wmbc->p, r);
	}
	*src += i;
}

winmsg_esc(SessName)
{
	char *session_name = strchr(SocketName, '.') + 1;

	if (*wmbc_strcpy(wmbc, session_name))
		wmc_set(cond);
}

winmsg_esc_ex(TruncPos, uint8_t perc, bool ellip)
{
	/* TODO: encapsulate */
	wmbc->trunc.pos = wmbc_offset(wmbc);
	wmbc->trunc.perc = perc;
	wmbc->trunc.ellip = ellip;
}

winmsg_esc_ex(WinNames, const bool hide_cur, Window *win)
{
	Window *oldfore = NULL;
	size_t max = wmbc_bytesleft(wmbc);

	if (display) {
		oldfore = D_fore;
		D_fore = win;
	}

	/* TODO: no need to enforce a limit here */
	AddWindows(wmbc, max - 1,
		hide_cur
			| (esc->flags.lng ? 0 : 2)
			| (esc->flags.plus ? 4 : 0)
			| (esc->flags.minus ? 8 : 0),
		win ? win->w_number : -1);

	if (display)
		D_fore = oldfore;

	if (*wmbc->p)
		wmc_set(cond);

	wmbc_fastfw0(wmbc);
}

winmsg_esc_ex(WinArgv, Window *win)
{
	if (!win || !win->w_cmdargs[0])
		return;

	wmbc_printf(wmbc, "%s", win->w_cmdargs[0]);
	wmbc_fastfw0(wmbc);

	if (**src == WINESC_CMD_ARGS) {
		for (int i = 1; win->w_cmdargs[i]; i++) {
			wmbc_printf(wmbc, " %s", win->w_cmdargs[i]);
			wmbc_fastfw0(wmbc);
		}
	}
}

static void
__WinMsgEscEsc_YEAR(WinMsgBufContext *wmbc, struct tm *tm)
{
	wmbc_printf(wmbc, "%04d", tm->tm_year + 1900);
}

static void
__WinMsgEscEsc_year(WinMsgBufContext *wmbc, struct tm *tm)
{
            wmbc_printf(wmbc, "%02d", tm->tm_year % 100);
}

static void
__WinMsgEscEsc_MONTH(WinMsgBufContext *wmbc, struct tm *tm)
{
#ifdef USE_LOCALE
            strftime(wmbc, l, (longflg ? "%B" : "%b"), tm);
#else
            wmbc_printf(wmbc, "%3.3s", months + 3 * tm->tm_mon);
#endif
}

static void
__WinMsgEscEsc_month(WinMsgBufContext *wmbc, struct tm *tm)
{
            wmbc_printf(wmbc, "%02d", tm->tm_mon + 1);
}

static void
__WinMsgEscEsc_DAY(WinMsgBufContext *wmbc, struct tm *tm)
{
#ifdef USE_LOCALE
            strftime(wmbc, l, (longflg ? "%A" : "%a"), tm);
#else
            wmbc_printf(wmbc, "%3.3s", days + 3 * tm->tm_wday);
#endif
}

static void
__WinMsgEscEsc_day(WinMsgBufContext *wmbc, struct tm *tm)
{
            wmbc_printf(wmbc, "%02d", tm->tm_mday % 100);
}

static void
__WinMsgEscEsc_HOUR(WinMsgBufContext *wmbc, struct tm *tm)
{
            wmbc_printf(wmbc, tm->tm_hour >= 12 ? "PM" : "AM");
}

static void
__WinMsgEscEsc_hour(WinMsgBufContext *wmbc, struct tm *tm)
{
            wmbc_printf(wmbc, tm->tm_hour >= 12 ? "pm" : "am");
}


static void
__WinMsgEscEsc_TIME(WinMsgBufContext *wmbc, struct tm *tm, WinMsgEsc esc)
{
            wmbc_printf(wmbc, esc.flags.zero ? "%02d:%02d" : "%2d:%02d", (tm->tm_hour + 11) % 12 + 1, tm->tm_min);
}

static void
__WinMsgEscEsc_time(WinMsgBufContext *wmbc, struct tm *tm, WinMsgEsc esc)
{
            wmbc_printf(wmbc, esc.flags.zero ? "%02d:%02d" : "%2d:%02d", tm->tm_hour, tm->tm_min);
}

winmsg_esc_ex(WinNum, Window *win)
{
	if (esc->num == 0)
		esc->num = 1;

	if (!win) {
		wmbc_printf(wmbc, "%*s", esc->num, esc->num > 1 ? "--" : "-");
	} else {
		wmbc_printf(wmbc, "%*d", esc->num, win->w_number);
	}

	wmc_set(cond);
}

winmsg_esc_ex(WinCount, Window *win)
{
	if (esc->num == 0)
		esc->num = 1;

	if (!win) {
		wmbc_printf(wmbc, "%*s", esc->num, esc->num > 1 ? "--" : "-");
	} else {
		int count = 0;

		for (Window *w = win; w; w = w->w_prev_mru) {
			if (esc->flags.minus) {
				if (w->w_group == win->w_group && w->w_type != W_TYPE_GROUP) {
					count++;
				}
			} else {
				count++;
			}
		}

		wmbc_printf(wmbc, "%*d", esc->num, count);
	}

	wmc_set(cond);
}

winmsg_esc_ex(WinLogName, Window *win)
{
	if (win && win->w_log && win->w_log->fp)
		wmbc_printf(wmbc, "%s", win->w_log->name);

}

winmsg_esc_ex(WinTty, Window *win)
{
	if (win && win->w_tty[0])
		wmbc_printf(wmbc, "%s", win->w_tty);

}

winmsg_esc_ex(WinSize, Window *win)
{
	if (!win)
		wmbc_printf(wmbc, "--x--");
	else
		wmbc_printf(wmbc, "%dx%d", win->w_width, win->w_height);
}

winmsg_esc_ex(WinTitle, Window *win)
{
	if (!win)
		return;

	if (*wmbc_strcpy(wmbc, win->w_title))
		wmc_set(cond);
}

winmsg_esc_ex(WinGroup, Window *win)
{
	if (!win || !win->w_group)
		return;

	if (*wmbc_strcpy(wmbc, win->w_group->w_title))
		wmc_set(cond);

}

winmsg_esc_ex(Cond, int *condrend)
{
	if (wmc_is_active(cond)) {
		bool chg;
		wmbc->p = wmbc->buf->buf + wmc_end(cond, wmbc_offset(wmbc), &chg);

		if (chg)
			wmbc->buf->numrend = *condrend;

		wmc_deinit(cond);
		return;
	}

	wmc_init(cond, wmbc_offset(wmbc));
	*condrend = wmbc->buf->numrend;
}

winmsg_esc_ex(CondElse, int *condrend)
{
	if (wmc_is_active(cond)) {
		bool chg;
		wmbc->p = wmbc->buf->buf + wmc_else(cond, wmbc_offset(wmbc), &chg);

		/* if the true branch was discarded, restore to previous rendition
		 * state; otherwise, we're keeping it, so update the rendition state */
		if (chg)
			wmbc->buf->numrend = *condrend;
		else
			*condrend = wmbc->buf->numrend;
	}
}


/* TODO: this is temporary until refactoring is complete and this code need not
 * be abstracted */
static void _MakeWinMsgEvRec(WinMsgBufContext *wmbc, WinMsgCond *cond, char *str,
                             Window *win, int *tick, int rec)
{
	int oldtick = *tick;
	WinMsgBuf *tmp = wmb_create();

	if (tmp == NULL)
		Panic(0, "%s", strnomem);

	/* create message in a new buffer and merge into our own */
	MakeWinMsgEv(tmp, str, win, WINMSG_BT_ESC, 0, NULL, rec + 1);
	if (*wmbc_mergewmb(wmbc, tmp))
		wmc_set(cond);

	/* TODO: handle some other way; not re-entrant */
	if (!*tick || oldtick < *tick)
		*tick = oldtick;

	wmb_free(tmp);
}

/* TODO: const char *str for safety and reassurance */
char *MakeWinMsgEv(WinMsgBuf *winmsg, char *str, Window *win,
                   int chesc, int padlen, Event *ev, int rec)
{
	static int tick;
	int qmnumrend = 0;
	int numpad = 0;
	int lastpad = 0;
	WinMsgBufContext *wmbc;
	WinMsgEsc esc;
	WinMsgCond *cond;

	struct tm *tm;
	struct timeval now;
	tm = 0;
	gettimeofday(&now, NULL);
	time_t nowsec = now.tv_sec;
	tm = localtime(&nowsec);

	/* TODO: temporary to work into existing code */
	if (winmsg == NULL) {
		if (g_winmsg == NULL) {
			if ((g_winmsg = wmb_create()) == NULL)
				Panic(0, "%s", strnomem);
		}
		winmsg = g_winmsg;
	}

	if (rec > WINMSG_RECLIMIT)
		return winmsg->buf;

	cond = calloc(1, sizeof(WinMsgCond));
	if (cond == NULL)
		Panic(0, "%s", strnomem);

	/* set to sane state (clear garbage) */
	wmc_deinit(cond);

	/* TODO: we can get rid of this once winmsg is properly handled by caller */
	if (winmsg->numrend > 0)
		winmsg->numrend = 0;

	wmb_reset(winmsg);
	wmbc = wmbc_create(winmsg);

	if (wmbc == NULL)
		Panic(0, "%s", strnomem);

	tick = 0;
	gettimeofday(&now, NULL);
	for (char *s = str; *s; s++) {
		if (*s != chesc) {
			if ((chesc == '%') && (*s == '^')) {
				s++;
				if (*s != '^' && *s >= 64)
					wmbc_putchar(wmbc, *s & 0x1f);
				continue;
			}
			wmbc_putchar(wmbc, *s);
			continue;
		}

		if (*++s == chesc)	/* double escape ? */
			continue;

		/* initialize escape */
		if ((esc.flags.plus = (*s == '+')) != 0)
			s++;
		if ((esc.flags.minus = (*s == '-')) != 0)
			s++;
		if ((esc.flags.zero = (*s == '0')) != 0)
			s++;
		esc.num = 0;
		while (*s >= '0' && *s <= '9')
			esc.num = esc.num * 10 + (*s++ - '0');
		if ((esc.flags.lng = (*s == 'L')) != 0)
			s++;

	        if (!tick || tick > 3600)
		        tick = 3600;

		switch (*s) {
		case WINESC_TIME:
			__WinMsgEscEsc_TIME(wmbc, tm, esc);
			if (!tick || tick > 60)
				tick = 60;
			break;
		case WINESC_time:
			__WinMsgEscEsc_time(wmbc, tm, esc);
			if (!tick || tick > 60)
				tick = 60;
			break;
		case WINESC_HOUR:
			__WinMsgEscEsc_HOUR(wmbc, tm);
			break;
		case WINESC_hour:
			__WinMsgEscEsc_hour(wmbc, tm);
			break;
		case WINESC_DAY:
			__WinMsgEscEsc_DAY(wmbc, tm);
			break;
		case WINESC_day:
			__WinMsgEscEsc_day(wmbc, tm);
			break;
		case WINESC_MONTH:
			__WinMsgEscEsc_MONTH(wmbc, tm);
			break;
		case WINESC_month:
			__WinMsgEscEsc_month(wmbc, tm);
			break;
		case WINESC_YEAR:
			__WinMsgEscEsc_YEAR(wmbc, tm);
			break;
		case WINESC_year:
			__WinMsgEscEsc_year(wmbc, tm);
			break;
		case WINESC_COND:
			WinMsgDoEscEx(Cond, &qmnumrend);
			break;
		case WINESC_COND_ELSE:
			WinMsgDoEscEx(CondElse, &qmnumrend);
			break;
		case WINESC_HSTATUS:
			WinMsgDoEscEx(Hstatus, win, &tick, rec);
			break;
		case WINESC_BACKTICK:
			WinMsgDoEscEx(Backtick, esc.num, win, &tick, &now, rec);
			break;
		case WINESC_CMD:
		case WINESC_CMD_ARGS:
			WinMsgDoEscEx(WinArgv, win);
			break;
		case WINESC_WIN_NAMES:
		case WINESC_WIN_NAMES_NOCUR:
			WinMsgDoEscEx(WinNames, (*s == WINESC_WIN_NAMES_NOCUR), win);
			break;
		case WINESC_WFLAGS:
			WinMsgDoEscEx(Wflags, win);
			break;
		case WINESC_WIN_TITLE:
			WinMsgDoEscEx(WinTitle, win);
			break;
		case WINESC_WIN_GROUP:
			WinMsgDoEscEx(WinGroup, win);
			break;
		case WINESC_REND_START:
			WinMsgDoEsc(Rend);
			break;
		case WINESC_HOST:
			WinMsgDoEsc(HostName);
			break;
		case WINESC_SESS_NAME:
			WinMsgDoEsc(SessName);
			break;
		case WINESC_PID:
			WinMsgDoEsc(Pid);
			break;
		case WINESC_FOCUS:
			WinMsgDoEscEx(Focus, win, ev);
			break;
		case WINESC_COPY_MODE:
			WinMsgDoEscEx(CopyMode, ev);
			break;
		case WINESC_ESC_SEEN:
			WinMsgDoEsc(EscSeen);
			break;
		case WINESC_TRUNC_POS:
			WinMsgDoEscEx(TruncPos,
				((esc.num > 100) ? 100 : esc.num),
				esc.flags.lng);
			break;
		case WINESC_PAD:
		case WINESC_TRUNC:
			WinMsgDoEscEx(PadOrTrunc, &numpad, &lastpad, padlen);
			break;
		case WINESC_WIN_SIZE:
			WinMsgDoEscEx(WinSize, win);
			break;
		case WINESC_WIN_NUM:
			WinMsgDoEscEx(WinNum, win);
			break;
		case WINESC_WIN_COUNT:
			WinMsgDoEscEx(WinCount, win);
			break;
		case WINESC_WIN_LOGNAME:
			WinMsgDoEscEx(WinLogName, win);
			break;
		case WINESC_WIN_TTY:
			WinMsgDoEscEx(WinTty, win);
			break;
		}
	}
	if (wmc_is_active(cond) && !wmc_is_set(cond))
		wmbc->p = wmbc->buf->buf + wmc_end(cond, wmbc_offset(wmbc), NULL) + 1;
	wmbc_putchar(wmbc, '\0' );
	wmbc->p--; /* TODO: temporary to work with old code */
	if (numpad) {
		if (padlen > MAXSTR - 1)
			padlen = MAXSTR - 1;
		pad_expand(winmsg, winmsg->buf, wmbc->p, numpad, padlen);
	}
	if (ev) {
		evdeq(ev);	/* just in case */
		ev->timeout = 0;
	}
	if (ev && tick) {
		now.tv_usec = 100000;
		if (tick == 1)
			now.tv_sec++;
		else
			now.tv_sec += tick - (now.tv_sec % tick);
		ev->timeout = (now.tv_sec * 1000 + now.tv_usec / 1000);
	}

	free(cond);
	wmbc_free(wmbc);
	return winmsg->buf;
}

char *MakeWinMsg(char *s, Window *win, int esc)
{
	return MakeWinMsgEv(NULL, s, win, esc, 0, NULL, 0);
}

static int WindowChangedCheck(char *s, WinMsgEscapeChar what, int *hp)
{
	int h = 0;
	int l;
	while (*s) {
		if (*s++ != (hp ? '%' : '\005'))
			continue;
		l = 0;
		s += (*s == '+');
		s += (*s == '-');
		while (*s >= '0' && *s <= '9')
			s++;
		if (*s == 'L') {
			s++;
			l = 0x100;
		}
		if (*s == WINESC_HSTATUS)
			h = 1;
		if (*s == (char)what || ((*s | l) == (int)what))
			break;
		if (*s)
			s++;
	}
	if (hp)
		*hp = h;
	return *s ? 1 : 0;
}

void WindowChanged(Window *win, WinMsgEscapeChar what)
{
	int inwstr, inhstr, inlstr;
	int inwstrh = 0, inhstrh = 0, inlstrh = 0;
	int got, ox, oy;
	Display *olddisplay = display;
	Canvas *cv;

	if (what == WINESC_WFLAGS) {
		WindowChanged(NULL, WINESC_WIN_NAMES | 0x100);
		WindowChanged(NULL, WINESC_WIN_NAMES_NOCUR | 0x100);
	}

	if (what) {
		inwstr = WindowChangedCheck(captionstring, what, &inwstrh);
		inhstr = WindowChangedCheck(hstatusstring, what, &inhstrh);
		inlstr = WindowChangedCheck(wliststr, what, &inlstrh);
	} else {
		inwstr = inhstr = 0;
		inlstr = 1;
	}

	if (win == NULL) {
		for (display = displays; display; display = display->d_next) {
			ox = D_x;
			oy = D_y;
			for (cv = D_cvlist; cv; cv = cv->c_next) {
				if (inlstr
				    || (inlstrh && win && win->w_hstatus && *win->w_hstatus
					&& WindowChangedCheck(win->w_hstatus, what, NULL)))
					WListUpdatecv(cv, NULL);
				win = Layer2Window(cv->c_layer);
				if (inwstr
				    || (inwstrh && win && win->w_hstatus && *win->w_hstatus
					&& WindowChangedCheck(win->w_hstatus, what, NULL))) {
					if (captiontop) {
						if (cv->c_ys - 1 >= 0)
							RefreshLine(cv->c_ys - 1, 0, D_width -1 , 0);
					} else {
						if (cv->c_ye + 1 < D_height)
							RefreshLine(cv->c_ye + 1, 0, D_width - 1, 0);
					}
				}
			}
			win = D_fore;
			if (inhstr
			    || (inhstrh && win && win->w_hstatus && *win->w_hstatus
				&& WindowChangedCheck(win->w_hstatus, what, NULL)))
				RefreshHStatus();
			if (ox != -1 && oy != -1)
				GotoPos(ox, oy);
		}
		display = olddisplay;
		return;
	}

	if (win->w_hstatus && *win->w_hstatus && (inwstrh || inhstrh || inlstrh)
	    && WindowChangedCheck(win->w_hstatus, what, NULL)) {
		inwstr |= inwstrh;
		inhstr |= inhstrh;
		inlstr |= inlstrh;
	}
	if (!inwstr && !inhstr && !inlstr)
		return;
	for (display = displays; display; display = display->d_next) {
		got = 0;
		ox = D_x;
		oy = D_y;
		for (cv = D_cvlist; cv; cv = cv->c_next) {
			if (inlstr)
				WListUpdatecv(cv, win);
			if (Layer2Window(cv->c_layer) != win)
				continue;
			got = 1;
			if (inwstr) {
				if (captiontop) {
					if (cv->c_ys -1 >= 0)
						RefreshLine(cv->c_ys - 1, 0, D_width - 1, 0);
				} else {
					if (cv->c_ye + 1 < D_height)
						RefreshLine(cv->c_ye + 1, 0, D_width - 1, 0);
				}
			}
		}
		if (got && inhstr && win == D_fore)
			RefreshHStatus();
		if (ox != -1 && oy != -1)
			GotoPos(ox, oy);
	}
	display = olddisplay;
}
