/*

SPDX-License-Identifier: GPL-2.0+

Copyright 2001 Jan Bobrowski <jb@wizard.ae.krakow.pl>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "kbd.h"

#define elemof(T) (sizeof T/sizeof*T)
#define endof(T) (T + elemof(T))

typedef unsigned char u8;

static int csitbl[] = {
	KBD_INS, KBD_UP, KBD_DOWN, KBD_RIGHT, KBD_LEFT, KBD_CENTER, KBD_END,
	KBD_PAGE_DOWN, KBD_HOME, KBD_PAGE_UP, KBD_WIN1, KBD_DEL, KBD_INS,
	KBD_F1, KBD_F2, KBD_F3, KBD_F4, KBD_F5, KBD_F6, KBD_F7, KBD_F8,
	KBD_F9, KBD_F10, KBD_F11, KBD_F12, KBD_HOME, KBD_SHIFT|KBD_TAB, 0,0,0,0,0,0,
	KBD_SHIFT|KBD_UP, KBD_SHIFT|KBD_DOWN, KBD_SHIFT|KBD_RIGHT, KBD_SHIFT|KBD_LEFT,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0, KBD_WIN3, KBD_WIN2
};

static int ss3tbl[] = {
	0, KBD_UP, KBD_DOWN, KBD_RIGHT, KBD_LEFT, 0,0,0,0, KBD_TAB ,0,0,0, KBD_ENTER,
	0,0, KBD_F1, KBD_F2, KBD_F3, KBD_F4, 0,0,0,0, '=',0,0, 0,0,0,0,0,0,
	KBD_CTRL|KBD_UP, KBD_CTRL|KBD_DOWN, KBD_CTRL|KBD_RIGHT, KBD_CTRL|KBD_LEFT,
	0,0,0,0,0, '*', '+', ',', '-', '.', '/', '0','1','2','3','4','5','6','7','8','9'
};

static int csi_tilde[] = {
	0, KBD_HOME, KBD_INS, KBD_DEL, KBD_END, KBD_PAGE_UP, KBD_PAGE_DOWN,
	KBD_F1, KBD_F2, KBD_F3, KBD_F4, KBD_F1, KBD_F2, KBD_F3, KBD_F4, KBD_F5,
	KBD_F5, KBD_F6, KBD_F7, KBD_F8, KBD_F9, KBD_F10, 0, KBD_F11, KBD_F12,
	KBD_F13, KBD_F14, KBD_F15, KBD_F16, KBD_F17, KBD_F18, KBD_F19, KBD_F20
};

#define CSI_Z_START 192
static int csi_z[] = {
	KBD_F11, KBD_F12, 0,0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,
	KBD_HOME, 0, KBD_PAGE_UP, 0, KBD_CENTER, 0, KBD_END, 0, KBD_PAGE_DOWN, 0,
	KBD_F1, KBD_F2, KBD_F3, KBD_F4, KBD_F5, KBD_F6, KBD_F7, KBD_F8, KBD_F9,
	KBD_F10, KBD_F11, KBD_F12, 0,0,0,0,0, 0,0,0,0,0, 0, KBD_INS, 0, KBD_DEL
};

static int st_start(int c);
static int st_esc(int c);
static int st_csiss3(int c);
static int st_arg(int c);
static int st_mouse(int c);

static int (*state)(int) = st_start;

struct seq {
	char type, mod, argf, delf;
	int arg[16], *argp;
} seq = {argp: seq.arg};

#define CSISEQ (seq.type == '[')
#define SS3SEQ (seq.type == 'O')

static int lowtbl[32] = {
	0,0,0,0,0,0,0,0, KBD_BS, KBD_TAB, KBD_SHIFT|KBD_ENTER, 0,0,
	KBD_ENTER, 0,0,0,0,0,0,0,0,0,0,0,0,0, KBD_ESC
};

static int ordkey(int c)
{
	int k = c;
	if(c < 32) {
		if(ALLOW_DEL_CH && c == 8) seq.delf = 1; /* DEL is DEL */
		k = lowtbl[c];
	} else if(c >= 127 && c < 0xA0) {
		k = KBD_NOKEY;
		if(c == 127)
			k = seq.delf ? KBD_DEL : KBD_BS;
	}
	return k;
}

static int st_start(int c)
{
	int k;
	if(c == 27 || c == 0x9B || c == 0x8F) {
		seq.type = seq.mod = 0;
		seq.argp = seq.arg;
		seq.arg[0] = 0;
		seq.arg[1] = 0;
		seq.argf = 0;

		state = st_esc;
		if(c & 0x80) {
			seq.type = c - 64;
			state = st_csiss3;
		}
		return KBD_MORE;
	}
	k = ordkey(c);
	if(!k) {
		int m = KBD_CTRL;
		c ^= 64;
		if(c >= 128) {
			c ^= 192;
			m ^= KBD_ALT | KBD_CTRL;
		}
		k = ordkey(c);
		if(k) k |= m;
	}
	return k;
}

static int st_esc(int c)
{
	if(c == KBD_MORE)
		return KBD_INCOMPLETE;
	if(c == '[' || c == 'O') {
		seq.type = c;
		state = st_csiss3;
		return KBD_MORE;
	}
	if(c == 27) {
		if(seq.mod)
			return KBD_INCOMPLETE;
		seq.mod = c;
		return KBD_MORE;
	}

	c = ordkey(c);
	if(c) c |= KBD_ALT;
	return c;
}

static int csiss3(int c)
{
	int m = seq.arg[0], v = 0;

	if(SS3SEQ) {
		c -= 0x40;
		if(c < elemof(ss3tbl))
			v = ss3tbl[c];
		goto ready;
	}
	if(seq.mod == '[') {
		c -= 'A';
		if(c < 12)
			v = KBD_F1 + c;
		goto ready;
	}
	if(seq.mod == '?') {
		if(c == 'c') {
			v = KBD_DECID;
			goto inclarg;
		}
		goto ret;
	}
	if(seq.argf && (c == '~' || c == '$' || c == '^' || c == '@')) {
		if(seq.arg[0] >= elemof(csi_tilde))
			goto ret;
		v = csi_tilde[seq.arg[0]];
		if(!v) goto ret;
		if(ALLOW_DEL_CH || v == KBD_DEL) seq.delf = 0; /* DEL is BS */
		if(c != '~') {
			if(c != '^') v |= KBD_SHIFT;
			if(c != '$') v |= KBD_CTRL;
		}
		m = seq.arg[1];
		goto ready;
	}
	if(c == 'z') {
		unsigned a = seq.arg[0];
		if(a < elemof(csi_tilde))
			v = csi_tilde[a];
		else {
			a -= CSI_Z_START;
			if(a >= elemof(csi_z))
				goto ret;
			v = csi_z[a];
		}
		m = seq.arg[1];
		goto ready;
	}
	if(c == 'R' && seq.argp == seq.arg + 1) {
		v = KBD_CPOS;
inclarg:
		v |= seq.arg[0]<<16 | seq.arg[1]<<24;
		goto ret;
	}
	if(c == 't') {
		v = KBD_TERM_T;
		v |= seq.arg[0] & 0xFF;
		v |= seq.arg[1] << 16;
		if(seq.argp > seq.arg + 1)
			v |= seq.arg[2] << 24;
		goto ret;
	}

	c -= 0x40;
	if(c < elemof(csitbl))
		v = csitbl[c];
ready:
	if(m) m = ((m-1) & 7) << 12;	/*XXX Mod==9 */
	if(seq.mod == 27) m |= KBD_ALT;
	v |= m;
ret:
	return v;
}

static int st_csiss3(int c)
{
	if(c == KBD_MORE || c < ' ')
		return KBD_INCOMPLETE;
	if((c == '[' || c == '?') && CSISEQ) {
		if(seq.mod) return KBD_INCOMPLETE;
		seq.mod = c;
		return KBD_MORE;
	}
	if(c >= '0' && c <= '9') {
		seq.arg[0] = c - '0';
		goto goarg;
	}
	if(c == ';') {
		*++seq.argp = 0;
goarg:
		seq.argf = 1;
		state = st_arg;
		return KBD_MORE;
	}
	if(c == 'M' && CSISEQ) {
		state = st_mouse;
		return KBD_MORE;
	}
	return csiss3(c);
}

static int st_arg(int c)
{
	if(c == KBD_MORE)
		return KBD_INCOMPLETE;
	if(c >= '0' && c <= '9') {
		*seq.argp = 10**seq.argp + c-'0';
		return KBD_MORE;
	}
	if(c == ';') {
		if(++seq.argp >= endof(seq.arg))
			return KBD_NOKEY;
		*seq.argp = 0;
		return KBD_MORE;
	}

	return csiss3(c);
}

static int st_mouse(int c)
{
	if(c == KBD_MORE)
		return KBD_INCOMPLETE;
	*seq.argp++ = c - 0x20;
	if(seq.argp < seq.arg + 3)
		return KBD_MORE;

	return MSE_RELEASE + (((seq.arg[0] & 3)
		| seq.arg[1]<<16 | seq.arg[2]<<24
		| seq.arg[0]<<10) & 0xF000);
}

static int waitdata;
static u8 buf[20], *p = buf, *e = buf;

static int incomplete()
{
	int l = e - buf;
	p = buf;

	if(l == 1)
		return ordkey(*p++);
	if(l < sizeof(buf)) buf[l] = 0;
	if(!memcmp(p, "\33[M", 3)) {
		p += 3;
		return KBD_F1;
	} else if(!memcmp(p, "\0x9BM", 2)) {
		p += 2;
		return KBD_F1;
	}
	if(*p++ == 27)
		return ordkey(*p++) | KBD_ALT;
	return ordkey(p[-1]);
}

int getkey()
{
	int v, c;

again:
	if(p == e) {
		int l = endof(buf) - p;
		if(l == 0) goto nomore;
		v = read(0, p, l);
		if(v <= 0) {
			if(waitdata) {
nomore:
				c = KBD_MORE;
				goto key;
			}
			return v<0 ? KBD_NOKEY : KBD_ERROR;
		}
		e = p + v;
	}
	c = *p++;
key:
	v = state(c);
	if(v == KBD_INCOMPLETE)
		v = incomplete();
	assert(c >= 0 || v >= 0);
	if(v < 0) {
		if(p < e)
			goto again;
		waitdata = 1;
		return KBD_MORE;
	}
	waitdata = 0;

	{
		char *q = buf;
		while(p < e) *q++ = *p++;
		e = q;
		p = buf;
	}

	state = st_start;
	if(!(v&0xFFF)) goto again;

	return v;
}
