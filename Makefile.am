## Copyright 2013 Paul Wise <pabs3@bonedaddy.net>
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <https://www.gnu.org/licenses/>.

SUBDIRS = src

dist_man_MANS = whowatch.1

menu_DATA = whowatch.desktop
menudir = $(datarootdir)/applications

dist-hook:
	if test -d .git -a \( ! -e $(distdir)/ChangeLog -o -w $(distdir)/ChangeLog \) ; then \
		LC_ALL=C git log --pretty --numstat --summary whowatch-$(VERSION) -- | git2cl > $(distdir)/ChangeLog ; \
	fi

EXTRA_DIST = \
	ChangeLog.old \
	PLUGINS.readme
