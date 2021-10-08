AM_CPPFLAGS = \
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-display-settings\" \
	-DPNP_IDS=\"$(PNP_IDS)\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-display-settings

xfce4_display_settings_SOURCES = \
	main.c \
	xfce-randr.c \
	xfce-randr.h \
	confirmation-dialog_ui.h \
	display-dialog_ui.h \
	minimal-display-dialog_ui.h \
	identity-popup_ui.h \
	display-name.c \
	edid-parse.c \
	edid.h \
	scrollarea.c \
	scrollarea.h \
	foo-marshal.c \
	foo-marshal.h

xfce4_display_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBBLADEUI_CFLAGS) \
	$(BLCONF_CFLAGS) \
	$(BLXO_CFLAGS) \
	$(XRANDR_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfce4_display_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_display_settings_LDADD = \
	$(GTK_LIBS) \
	$(LIBBLADEUI_LIBS) \
	$(BLCONF_LIBS) \
	$(BLXO_LIBS) \
	$(LIBX11_LIBS) -lm \
	$(XRANDR_LIBS) \
	$(GLIB_LIBS)

if MAINTAINER_MODE

BUILT_SOURCES = \
	confirmation-dialog_ui.h \
	display-dialog_ui.h \
	minimal-display-dialog_ui.h \
	identity-popup_ui.h

confirmation-dialog_ui.h: confirmation-dialog.glade
	blxo-csource --static --strip-comments --strip-content --name=confirmation_dialog_ui $< >$@

display-dialog_ui.h: display-dialog.glade
	blxo-csource --static --strip-comments --strip-content --name=display_dialog_ui $< >$@

minimal-display-dialog_ui.h: minimal-display-dialog.glade
	blxo-csource --static --strip-comments --strip-content --name=minimal_display_dialog_ui $< >$@
	
identity-popup_ui.h: identity-popup.glade
	blxo-csource --static --strip-comments --strip-content --name=identity_popup_ui $< >$@

endif

desktopdir = $(datadir)/applications
desktop_in_files = xfce-display-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

iconsdir = $(datadir)/icons/hicolor/128x128/devices
icons_DATA = \
	xfce-display-extend.png \
	xfce-display-external.png \
	xfce-display-internal.png \
	xfce-display-mirror.png

EXTRA_DIST = \
	$(icons_DATA) \
	$(desktop_in_files) \
	confirmation-dialog.glade \
	display-dialog.glade \
	minimal-display-dialog.glade \
	identity-popup.glade

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
