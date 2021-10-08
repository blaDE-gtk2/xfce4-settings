
AM_CPPFLAGS = \
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-I${top_srcdir} \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"blade-settings-editor\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	blade-settings-editor

blade_settings_editor_built_sources = \
	blade-settings-marshal.c \
	blade-settings-marshal.h

blade_settings_editor_SOURCES = \
	$(blade_settings_editor_built_sources) \
	main.c \
	blade-settings-cell-renderer.c \
	blade-settings-cell-renderer.h \
	blade-settings-editor-box.c \
	blade-settings-editor-box.h \
	blade-settings-prop-dialog.c \
	blade-settings-prop-dialog.h

blade_settings_editor_CFLAGS = \
	$(GTK_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(LIBBLADEUTIL_CFLAGS) \
	$(LIBBLADEUI_CFLAGS) \
	$(BLCONF_CFLAGS) \
	$(PLATFORM_CFLAGS)

blade_settings_editor_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

blade_settings_editor_LDADD = \
	$(GTK_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(LIBBLADEUTIL_LIBS) \
	$(LIBBLADEUI_LIBS) \
	$(BLCONF_LIBS)

desktopdir = $(datadir)/applications
desktop_in_files = blade-settings-editor.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

#
# Build sources
#
if MAINTAINER_MODE

blade-settings-marshal.h: blade-settings-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=_xfce_settings_marshal --internal --header $< > $@

blade-settings-marshal.c: blade-settings-marshal.list Makefile
	$(AM_V_GEN) echo "#include \"blade-settings-marshal.h\"" > $@ \
	&& glib-genmarshal --prefix=_xfce_settings_marshal --body $< >> $@

DISTCLEANFILES = \
	$(blade_settings_editor_built_sources) \
	$(desktop_DATA)

BUILT_SOURCES = \
	$(blade_settings_editor_built_sources)

else

DISTCLEANFILES = \
	$(desktop_DATA)

endif

EXTRA_DIST = \
	blade-settings-marshal.list \
	$(desktop_in_files)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
