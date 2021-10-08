AM_CPPFLAGS = \
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DHELPERDIR=\"$(HELPER_PATH_PREFIX)/xfce4/settings\" \
	-DG_LOG_DOMAIN=\"xfce4-appearance-settings\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-appearance-settings

xfce4_appearance_settings_SOURCES = \
	main.c \
	images.h \
	appearance-dialog_ui.h

xfce4_appearance_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBBLADEUI_CFLAGS) \
	$(BLCONF_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfce4_appearance_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_appearance_settings_LDADD = \
	$(GTK_LIBS) \
	$(LIBBLADEUI_LIBS) \
	$(BLCONF_LIBS)

xfce4_appearance_scriptsdir = $(HELPER_PATH_PREFIX)/xfce4/settings
xfce4_appearance_scripts_SCRIPTS = appearance-install-theme

if MAINTAINER_MODE

BUILT_SOURCES = \
	appearance-dialog_ui.h

appearance-dialog_ui.h: appearance-dialog.glade
	blxo-csource --static --strip-comments --strip-content --name=appearance_dialog_ui $< >$@

endif

desktopdir = $(datadir)/applications
desktop_in_files = xfce-ui-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files) \
	appearance-install-theme \
	appearance-dialog.glade

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
