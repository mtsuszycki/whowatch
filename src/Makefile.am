bin_PROGRAMS = whowatch

whowatch_SOURCES = \
	whowatch.c \
	whowatch.h \
	plist.c \
	procinfo.c \
	procinfo.h \
	proctree.c \
	block.c \
	owner.c \
	screen.c \
	ulist.c \
	ulist.h \
	list.h \
	exti.c \
	proctree.h \
	pluglib.c \
	pluglib.h \
	wdgts.c \
	proc_plugin.c \
	user_plugin.c \
	var.h \
	kbd.c \
	kbd.h

OS_SPECIFIC = procinfo.c proc_plugin.c

$(OS_SPECIFIC):
	ln -sf $(srcdir)/dialects/$(os)/$@ $@

EXTRA_DIST = \
	dialects/linux/proc_plugin.c \
	dialects/linux/procinfo.c \
	dialects/bsd/proc_plugin.c \
	dialects/bsd/procinfo.c

DISTCLEANFILES = $(OS_SPECIFIC)
