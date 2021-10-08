AM_CPPFLAGS = \
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DBINDIR=\"$(bindir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-keyboard-settings\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-keyboard-settings

xfce4_keyboard_settings_SOURCES = \
	main.c \
	xfce-keyboard-settings.c \
	xfce-keyboard-settings.h \
	command-dialog.c \
	command-dialog.h \
	keyboard-dialog_ui.h

xfce4_keyboard_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXKLAVIER_CFLAGS) \
	$(LIBXKLAVIER_0_4_CFLAGS) \
	$(LIBBLADEUTIL_CFLAGS) \
	$(LIBBLADEUI_CFLAGS) \
	$(LIBXFCE4KBD_PRIVATE_CFLAGS) \
	$(BLXO_CFLAGS) \
	$(BLCONF_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfce4_keyboard_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_keyboard_settings_LDADD = \
	$(GTK_LIBS) \
	$(LIBXKLAVIER_LIBS) \
	$(LIBXKLAVIER_0_4_LIBS) \
	$(LIBBLADEUTIL_LIBS) \
	$(LIBBLADEUI_LIBS) \
	$(LIBXFCE4KBD_PRIVATE_LIBS) \
	$(BLXO_LIBS) \
	$(BLCONF_LIBS)

if MAINTAINER_MODE

BUILT_SOURCES = \
	keyboard-dialog_ui.h

keyboard-dialog_ui.h: keyboard-dialog.glade
	blxo-csource --static --strip-comments --strip-content --name=keyboard_dialog_ui $< >$@

endif

desktopdir = $(datadir)/applications
desktop_in_files = xfce-keyboard-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files) \
	keyboard-dialog.glade

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
