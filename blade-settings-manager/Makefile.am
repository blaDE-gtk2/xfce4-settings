AM_CPPFLAGS = \
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-I${top_srcdir} \
	-DMENUFILE=\"$(sysconfdir)/xdg/menus/blade-settings-manager.menu\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"blade-settings-manager\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	blade-settings-manager

blade_settings_manager_SOURCES = \
	main.c \
	blade-settings-manager-dialog.c \
	blade-settings-manager-dialog.h \
	xfce-text-renderer.c \
	xfce-text-renderer.h

blade_settings_manager_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBBLADEUTIL_CFLAGS) \
	$(LIBBLADEUI_CFLAGS) \
	$(POJK_CFLAGS) \
	$(BLCONF_CFLAGS) \
	$(BLXO_CFLAGS) \
	$(PLATFORM_CFLAGS)

blade_settings_manager_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

blade_settings_manager_LDADD = \
	$(GTK_LIBS) \
	$(LIBBLADEUTIL_LIBS) \
	$(LIBBLADEUI_LIBS) \
	$(BLCONF_LIBS) \
	$(BLXO_LIBS) \
	$(POJK_LIBS)

menudir = $(sysconfdir)/xdg/menus
menu_DATA = blade-settings-manager.menu

desktopdir = $(datadir)/applications
desktop_in_files = blade-settings-manager.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files) \
	$(menu_DATA)

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
