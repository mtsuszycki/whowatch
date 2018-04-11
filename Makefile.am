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
