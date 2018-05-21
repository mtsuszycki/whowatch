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

#define KBD_ENTER	0x100
#define KBD_BS		0x101
#define KBD_TAB		0x102
#define KBD_ESC		0x103
#define KBD_LEFT	0x104
#define KBD_RIGHT	0x105
#define KBD_UP		0x106
#define KBD_DOWN	0x107
#define KBD_INS		0x108
#define KBD_DEL		0x109
#define KBD_HOME	0x10a
#define KBD_END		0x10b
#define KBD_PAGE_UP	0x10c
#define KBD_PGUP KBD_PAGE_UP
#define KBD_PAGE_DOWN	0x10d
#define KBD_PGDN KBD_PAGE_DOWN
#define KBD_CENTER	0x110
#define KBD_WIN1	0x111
#define KBD_WIN2	0x112
#define KBD_WIN3	0x113

#define KBD_F1		0x120
#define KBD_F2		0x121
#define KBD_F3		0x122
#define KBD_F4		0x123
#define KBD_F5		0x124
#define KBD_F6		0x125
#define KBD_F7		0x126
#define KBD_F8		0x127
#define KBD_F9		0x128
#define KBD_F10		0x129
#define KBD_F11		0x12a
#define KBD_F12		0x12b

#define KBD_F13		0x12c
#define KBD_F14		0x12d
#define KBD_F15		0x12e
#define KBD_F16		0x12f
#define KBD_F17		0x130
#define KBD_F18		0x131
#define KBD_F19		0x132
#define KBD_F20		0x133

#define KBD_MOUSE	0x800
#define MSE_RELEASE	0x800
#define MSE_BUTTON1	0x801
#define MSE_BUTTON2	0x802
#define MSE_BUTTON3	0x803

#define KBD_CPOS	0x900
#define KBD_DECID	0x901
#define KBD_TERM_T	0xA00
#define KBD_TSIZE	0xA08

#define KBD_MASK	0x0FFF

#define KBD_SHIFT	0x1000
#define KBD_ALT		0x2000
#define KBD_CTRL	0x4000

#define KBD_NOKEY 0
#define KBD_MORE -1
#define KBD_ERROR -2
#define KBD_INCOMPLETE -3 /* internal */

#define ALLOW_DEL_CH	1
