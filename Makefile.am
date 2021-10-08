ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = \
	dialogs \
	blade-settings-manager \
	blade-settings-editor \
	blsettingsd \
	po

EXTRA_DIST = \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update
