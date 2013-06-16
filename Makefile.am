SUBDIRS = src

dist_man_MANS = whowatch.1

dist-hook:
	if test -d .git -a \( ! -e $(distdir)/ChangeLog -o -w $(distdir)/ChangeLog \) ; then \
		LC_ALL=C git log --pretty --numstat --summary $(VERSION) | git2cl > $(distdir)/ChangeLog ; \
	fi

EXTRA_DIST = \
	PLUGINS.readme
