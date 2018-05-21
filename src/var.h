/*

SPDX-License-Identifier: GPL-2.0+

Copyright 2006 Michal Suszycki <mt_suszycki@yahoo.com>

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

#define DU(B) typedef u_int ## B ## _t u ## B
DU(8); DU(16); DU(32); DU(64);
#undef DU

#define MIN(x,y)	(x)<(y)?(x):(y)

//#if 1
#ifdef DEBUG
#define DBG(f,a...)	dolog("%s %d: " f, __FUNCTION__, __LINE__ , ##a)
#else
#define DBG(f,a...)	/* */
#endif

#define elemof(x)	(sizeof (x) / sizeof*(x))
#define endof(x)	((x) + elemof(x))

