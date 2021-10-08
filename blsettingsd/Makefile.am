AM_CPPFLAGS = \
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DBINDIR=\"$(bindir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DSYSCONFIGDIR=\"$(sysconfdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"blsettingsd\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	blsettingsd

blsettingsd_SOURCES = \
	main.c \
	accessibility.c \
	accessibility.h \
	debug.c \
	debug.h \
	clipboard-manager.c \
	clipboard-manager.h \
	gtk-decorations.c \
	gtk-decorations.h \
	keyboards.c \
	keyboards.h \
	keyboard-shortcuts.c \
	keyboard-shortcuts.h \
	keyboard-layout.c \
	keyboard-layout.h \
	pointers.c \
	pointers.h \
	pointers-defines.h \
	workspaces.c \
	workspaces.h \
	xsettings.c \
	xsettings.h

blsettingsd_CFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(GIO_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(BLCONF_CFLAGS) \
	$(LIBBLADEUTIL_CFLAGS) \
	$(LIBBLADEUI_CFLAGS) \
	$(LIBXFCE4KBD_PRIVATE_CFLAGS) \
	$(LIBXKLAVIER_CFLAGS) \
	$(XI_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(FONTCONFIG_CFLAGS) \
	$(LIBINPUT_CFLAGS) \
	$(PLATFORM_CFLAGS)

blsettingsd_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

blsettingsd_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(GTHREAD_LIBS) \
	$(GIO_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(BLCONF_LIBS) \
	$(LIBBLADEUTIL_LIBS) \
	$(LIBBLADEUI_LIBS) \
	$(LIBXFCE4KBD_PRIVATE_LIBS) \
	$(LIBXKLAVIER_LIBS) \
	$(XI_LIBS) \
	$(LIBX11_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(FONTCONFIG_LIBS) \
	$(LIBINPUT_LIBS) \
	-lm

#
# Optional support for the display settings
#
if HAVE_XRANDR
blsettingsd_SOURCES += \
	displays.c \
	displays.h

blsettingsd_CFLAGS += \
	$(XRANDR_CFLAGS)

blsettingsd_LDADD += \
	$(XRANDR_LIBS)

if HAVE_UPOWERGLIB
blsettingsd_SOURCES += \
	displays-upower.c \
	displays-upower.h

blsettingsd_CFLAGS += \
	$(UPOWERGLIB_CFLAGS)

blsettingsd_LDADD += \
	$(UPOWERGLIB_LIBS)
endif
endif

settingsdir = $(sysconfdir)/xdg/xfce4/blconf/xfce-perchannel-xml
settings_DATA = xsettings.xml

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = blsettingsd.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(settings_DATA) \
	$(autostart_in_files)

DISTCLEANFILES = \
	$(autostart_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
