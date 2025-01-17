/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <unistd.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <gtk/gtk.h>

#include <libbladeui/libbladeui.h>
#include <libbladeutil/libbladeutil.h>
#include <blconf/blconf.h>

#include "appearance-dialog_ui.h"
#include "images.h"

#define INCH_MM      25.4

/* Use a fallback DPI of 96 which should be ok-ish on most systems
 * and is only applied on rare occasions */
#define FALLBACK_DPI 96

/* Increase this number if new gtk settings have been added */
#define INITIALIZE_UINT (1)

#define COLOR_SCHEME_SYMBOL ((gpointer) 1)
#define GTK_RC_TOKEN_FG ((gpointer) 2)
#define GTK_RC_TOKEN_BG ((gpointer) 3)
#define GTK_RC_TOKEN_NORMAL ((gpointer) 4)
#define GTK_RC_TOKEN_SELECTED ((gpointer) 5)

gchar *gtkrc_get_color_scheme_for_theme (const gchar *gtkrc_filename);
gboolean color_scheme_parse_colors (const gchar *scheme, GdkColor *colors);
static GdkPixbuf *theme_create_preview (GdkColor *colors);

enum
{
    COLUMN_THEME_PREVIEW,
    COLUMN_THEME_NAME,
    COLUMN_THEME_DISPLAY_NAME,
    COLUMN_THEME_COMMENT,
    COLUMN_THEME_NO_CACHE,
    N_THEME_COLUMNS
};

enum
{
    COLUMN_RGBA_PIXBUF,
    COLUMN_RGBA_NAME,
    N_RGBA_COLUMNS
};

enum {
	COLOR_FG,
	COLOR_BG,
	COLOR_SELECTED_BG,
	NUM_SYMBOLIC_COLORS
};

/* String arrays with the settings in combo boxes */
static const gchar* toolbar_styles_array[] =
{
    "icons", "text", "both", "both-horiz"
};

static const gchar* xft_hint_styles_array[] =
{
    "hintnone", "hintslight", "hintmedium", "hintfull"
};

static const gchar* xft_rgba_array[] =
{
    "none", "rgb", "bgr", "vrgb", "vbgr"
};

static const GtkTargetEntry theme_drop_targets[] =
{
  { "text/uri-list", 0, 0 }
};

/* Option entries */
static GdkNativeWindow opt_socket_id = 0;
static gboolean opt_version = FALSE;
static GOptionEntry option_entries[] =
{
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};

/* Global blconf channel */
static BlconfChannel *xsettings_channel;

typedef struct
{
    GtkListStore *list_store;
    GtkTreeView *tree_view;
} preview_data;


static preview_data *
preview_data_new (GtkListStore *list_store,
                  GtkTreeView *tree_view)
{
    preview_data *pd;

    g_return_val_if_fail (list_store != NULL, NULL);
    g_return_val_if_fail (tree_view != NULL, NULL);
    g_return_val_if_fail (GTK_IS_LIST_STORE (list_store), NULL);
    g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

    pd = g_slice_new0 (preview_data);
    g_return_val_if_fail (pd != NULL, NULL);

    pd->list_store = list_store;
    pd->tree_view = tree_view;

    g_object_ref (G_OBJECT (pd->list_store));
    g_object_ref (G_OBJECT (pd->tree_view));

    return pd;
}

static void
preview_data_free (preview_data *pd)
{
    if (G_UNLIKELY (pd == NULL))
        return;
    g_object_unref (G_OBJECT (pd->list_store));
    g_object_unref (G_OBJECT (pd->tree_view));
    g_slice_free (preview_data, pd);
}

static int
compute_xsettings_dpi (GtkWidget *widget)
{
    GdkScreen *screen;
    int width_mm, height_mm;
    int width, height;
    int dpi;

    screen = gtk_widget_get_screen (widget);
    width_mm = gdk_screen_get_width_mm (screen);
    height_mm = gdk_screen_get_height_mm (screen);
    dpi = FALLBACK_DPI;

    if (width_mm > 0 && height_mm > 0)
    {
        width = gdk_screen_get_width (screen);
        height = gdk_screen_get_height (screen);
        dpi = MIN (INCH_MM * width  / width_mm,
                   INCH_MM * height / height_mm);
    }

    return dpi;
}

/* Try to retrieve the color scheme and alternatively parse the colors from the gtkrc */
gchar *
gtkrc_get_color_scheme_for_theme (const gchar *gtkrc_filename)
{
    gint file = -1;
    GString *result_string = g_string_new("");
    GString *fallback_string = g_string_new("");
    GString *tmp_string = g_string_new("");
    gchar *result = NULL;
    gboolean bg_normal = FALSE;
    gboolean bg_selected = FALSE;
    gboolean fg_normal = FALSE;
    GSList *files = NULL;
    GSList *read_files = NULL;
    GTokenType token;
    GScanner *scanner = gtk_rc_scanner_new ();

    g_return_val_if_fail (gtkrc_filename != NULL, "");

    g_scanner_scope_add_symbol (scanner, 0, "gtk_color_scheme", COLOR_SCHEME_SYMBOL);
    g_scanner_scope_add_symbol (scanner, 0, "gtk-color-scheme", COLOR_SCHEME_SYMBOL);
    g_scanner_scope_add_symbol (scanner, 0, "fg", GTK_RC_TOKEN_FG);
    g_scanner_scope_add_symbol (scanner, 0, "bg", GTK_RC_TOKEN_BG);
    g_scanner_scope_add_symbol (scanner, 0, "NORMAL", GTK_RC_TOKEN_NORMAL);
    g_scanner_scope_add_symbol (scanner, 0, "SELECTED", GTK_RC_TOKEN_SELECTED);

    files = g_slist_prepend (files, g_strdup (gtkrc_filename));
    while (files != NULL)
    {
        gchar *filename = files->data;
        files = g_slist_delete_link (files, files);

        if (filename == NULL)
            continue;

        if (g_slist_find_custom (read_files, filename, (GCompareFunc) strcmp))
        {
            g_warning ("Recursion in the gtkrc detected!");
            g_free (filename);
            continue; /* skip this file since we've done it before... */
        }

        read_files = g_slist_prepend (read_files, filename);

        file = g_open (filename, O_RDONLY);
        if (file == -1)
            g_warning ("Could not open file \"%s\"", filename);
        else
        {
            g_scanner_input_file (scanner, file);
            while ((token = g_scanner_get_next_token (scanner)) != G_TOKEN_EOF)
            {
                /* Scan the gtkrc file for the gtk-color-scheme */
                if (GINT_TO_POINTER (token) == COLOR_SCHEME_SYMBOL)
                {
                    if (g_scanner_get_next_token (scanner) == '=')
                    {
                        token = g_scanner_get_next_token (scanner);
                        if (token == G_TOKEN_STRING)
                        {
                            g_string_append_printf(result_string, "\n%s", scanner->value.v_string);
                        }
                        bg_normal = g_strstr_len(result_string->str, -1, "bg_color") != NULL;
                        bg_selected = g_strstr_len(result_string->str, -1, "selected_bg_color") != NULL;
                        fg_normal = g_strstr_len(result_string->str, -1, "fg_color") != NULL;
                    }
                }
                /* Scan the gtkrc file for first occurences of bg[NORMAL], bg[SELECTED] and fg[NORMAL] in case
                 * it doesn't provide a gtk-color-scheme */
                if (result_string->len == 0)
                {
                    if (GINT_TO_POINTER (token) == GTK_RC_TOKEN_BG)
                    {
                        if (g_scanner_get_next_token (scanner) == G_TOKEN_LEFT_BRACE)
                        {
                            /* bg[SELECTED] */
                            if (GINT_TO_POINTER (token = g_scanner_get_next_token (scanner)) == GTK_RC_TOKEN_SELECTED
                                && g_scanner_get_next_token (scanner) == G_TOKEN_RIGHT_BRACE
                                && g_scanner_get_next_token (scanner) == '=')
                            {
                                token = g_scanner_get_next_token (scanner);
                                /* Parse colors in hex #rrggbb format */
                                if (token == G_TOKEN_STRING)
                                {
                                    if (!g_strrstr (fallback_string->str, "selected_bg_color"))
                                    {
                                        g_string_append_printf(fallback_string, "\nselected_bg_color:%s", scanner->value.v_string);
                                        bg_selected = TRUE;
                                    }
                                }
                                /* Parse colors in { r, g, b } format */
                                else if (token == G_TOKEN_LEFT_CURLY)
                                {
                                    if (!g_strrstr (fallback_string->str, "selected_bg_color"))
                                    {
                                        g_string_erase(tmp_string, 0, -1);
                                        while (token != G_TOKEN_RIGHT_CURLY)
                                        {
                                            token = g_scanner_get_next_token (scanner);
                                            if (token == G_TOKEN_FLOAT)
                                                g_string_append_printf(tmp_string, "%02x", (uint) (scanner->value.v_float*255));
                                        }
                                        g_string_append_printf(fallback_string, "\nselected_bg_color:#%s", tmp_string->str);
                                        bg_selected = TRUE;
                                    }
                                }
                            }
                            /* bg[NORMAL] */
                            else if (GINT_TO_POINTER (token) == GTK_RC_TOKEN_NORMAL
                                     && g_scanner_get_next_token (scanner) == G_TOKEN_RIGHT_BRACE
                                     && g_scanner_get_next_token (scanner) == '=')
                            {
                                token = g_scanner_get_next_token (scanner);
                                /* Parse colors in hex #rrggbb format */
                                if (token == G_TOKEN_STRING)
                                {
                                    if (!g_strrstr (fallback_string->str, "bg_color"))
                                    {
                                        g_string_append_printf(fallback_string, "\nbg_color:%s", scanner->value.v_string);
                                        bg_normal = TRUE;
                                    }
                                }
                                /* Parse colors in { r, g, b } format */
                                else if (token == G_TOKEN_LEFT_CURLY)
                                {
                                    if (!g_strrstr (fallback_string->str, "bg_color"))
                                    {
                                        g_string_erase(tmp_string, 0, -1);
                                        while (token != G_TOKEN_RIGHT_CURLY)
                                        {
                                            token = g_scanner_get_next_token (scanner);
                                            if (token == G_TOKEN_FLOAT)
                                                g_string_append_printf(tmp_string, "%02x", (uint) (scanner->value.v_float*255));
                                        }
                                        g_string_append_printf(fallback_string, "\nbg_color:#%s", tmp_string->str);
                                        bg_normal = TRUE;
                                    }
                                }
                            }
                        }
                    }
                    else if (GINT_TO_POINTER (token) == GTK_RC_TOKEN_FG)
                    {
                        /* fg[NORMAL] */
                        if (g_scanner_get_next_token (scanner) == G_TOKEN_LEFT_BRACE
                            && GINT_TO_POINTER (token = g_scanner_get_next_token (scanner)) == GTK_RC_TOKEN_NORMAL
                                && g_scanner_get_next_token (scanner) == G_TOKEN_RIGHT_BRACE
                                && g_scanner_get_next_token (scanner) == '=')
                        {
                            token = g_scanner_get_next_token (scanner);
                            /* Parse colors in hex #rrggbb format */
                            if (token == G_TOKEN_STRING)
                            {
                                if (!g_strrstr (fallback_string->str, "fg_color"))
                                {
                                    g_string_append_printf(fallback_string, "\nfg_color:%s", scanner->value.v_string);
                                    fg_normal = TRUE;
                                }
                            }
                            /* Parse colors in { r, g, b } format */
                            else if (token == G_TOKEN_LEFT_CURLY)
                            {
                                if (!g_strrstr (fallback_string->str, "fg_color"))
                                {
                                    g_string_erase(tmp_string, 0, -1);
                                    while (token != G_TOKEN_RIGHT_CURLY)
                                    {
                                        token = g_scanner_get_next_token (scanner);
                                        if (token == G_TOKEN_FLOAT)
                                            g_string_append_printf(tmp_string, "%02x", (uint) (scanner->value.v_float*255));
                                    }
                                    g_string_append_printf(fallback_string, "\nfg_color:#%s", tmp_string->str);
                                    fg_normal = TRUE;
                                }
                            }
                        }
                    }
                }
                /* Check whether we can stop parsing because all colors have been retrieved somehow */
                if (bg_normal && bg_selected && fg_normal)
                    break;
            }
        close (file);
        }
    }

    g_slist_foreach (read_files, (GFunc) (void (*)(void)) g_free, NULL);
    g_slist_free (read_files);

    g_scanner_destroy (scanner);

    /* Use the fallback colors parsed from the theme if gtk-color-scheme is not defined */
    /*
    if (!result)
        result = fallback;
    */
    if (result_string->len == 0)
        result = g_strdup(fallback_string->str);
    else
        result = g_strdup(result_string->str);

    g_string_free(result_string, TRUE);
    g_string_free(fallback_string, TRUE);
    g_string_free(tmp_string, TRUE);

    return result;
}

gboolean
color_scheme_parse_colors (const gchar *scheme, GdkColor *colors)
{
    gchar **color_scheme_strings, **color_scheme_pair, *current_string;
    gboolean found = FALSE;
    gint i;

    if (!scheme || !strcmp (scheme, ""))
        return FALSE;

    /* Initialise the array, fallback color is white */
    for (i = 0; i < NUM_SYMBOLIC_COLORS; i++)
        colors[i].red = colors[i].green = colors[i].blue = 65535.0;

    /* The color scheme string consists of name:color pairs, separated by
     * either semicolons or newlines, so first we split the string up by delimiter */
    if (g_strrstr (scheme, ";"))
        color_scheme_strings = g_strsplit (scheme, ";", 0);
    else
        color_scheme_strings = g_strsplit (scheme, "\n", 0);

    /* Loop through the name:color pairs, and save the color if we recognise the name */
    i = 0;
    while ((current_string = color_scheme_strings[i++]))
    {
        color_scheme_pair = g_strsplit (current_string, ":", 0);

        if (color_scheme_pair[0] != NULL && color_scheme_pair[1] != NULL)
        {
            g_strstrip (color_scheme_pair[0]);
            g_strstrip (color_scheme_pair[1]);

            if (!strcmp ("fg_color", color_scheme_pair[0]))
            {
              gdk_color_parse (color_scheme_pair[1], &colors[COLOR_FG]);
              found = TRUE;
            }
            else if (!strcmp ("bg_color", color_scheme_pair[0]))
            {
              gdk_color_parse (color_scheme_pair[1], &colors[COLOR_BG]);
              found = TRUE;
            }
            else if (!strcmp ("selected_bg_color", color_scheme_pair[0]))
            {
              gdk_color_parse (color_scheme_pair[1], &colors[COLOR_SELECTED_BG]);
              found = TRUE;
            }
        }

        g_strfreev (color_scheme_pair);
    }

    g_strfreev (color_scheme_strings);

    return found;
}

static GdkPixbuf *
theme_create_preview (GdkColor *colors)
{
    GdkPixbuf *theme_preview;
    GdkPixmap *drawable;
    GdkColormap *cmap = gdk_colormap_get_system ();
    cairo_t *cr;
    gint width = 44;
    gint height = 22;

    drawable = gdk_pixmap_new (gdk_get_default_root_window(), width, height,
                               gdk_drawable_get_depth (gdk_get_default_root_window ()));
    cr = gdk_cairo_create (drawable);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    /* Draw three rectangles showcasing the background, foreground and selected background colors */
    cairo_rectangle (cr, 0, 0, 16, 22);
    cairo_set_source_rgb (cr, colors[COLOR_BG].red / 65535.0, colors[COLOR_BG].green / 65535.0, colors[COLOR_BG].blue / 65535.0);
    cairo_fill (cr);

    cairo_rectangle (cr, 15, 0, 30, 22);
    cairo_set_source_rgb (cr, colors[COLOR_FG].red / 65535.0, colors[COLOR_FG].green / 65535.0, colors[COLOR_FG].blue / 65535.0);
    cairo_fill (cr);

    cairo_rectangle (cr, 29, 0, 42, 22);
    cairo_set_source_rgb (cr, colors[COLOR_SELECTED_BG].red / 65535.0, colors[COLOR_SELECTED_BG].green / 65535.0, colors[COLOR_SELECTED_BG].blue / 65535.0);
    cairo_fill (cr);

    theme_preview = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
    gdk_pixbuf_get_from_drawable (theme_preview, drawable, cmap, 0, 0, 0, 0, width, height);

    g_object_unref (drawable);
    cairo_destroy (cr);

    return theme_preview;
}

static void
cb_theme_tree_selection_changed (GtkTreeSelection *selection,
                                 const gchar      *property)
{
    GtkTreeModel *model;
    gboolean      has_selection;
    gchar        *name;
    GtkTreeIter   iter;

    /* Get the selected list iter */
    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        /* Get the theme name */
        gtk_tree_model_get (model, &iter, COLUMN_THEME_NAME, &name, -1);

        /* Store the new theme */
        blconf_channel_set_string (xsettings_channel, property, name);

        /* Cleanup */
        g_free (name);
    }
}

static void
cb_icon_theme_tree_selection_changed (GtkTreeSelection *selection)
{
    /* Set the new icon theme */
    cb_theme_tree_selection_changed (selection, "/Net/IconThemeName");
}

static void
cb_ui_theme_tree_selection_changed (GtkTreeSelection *selection)
{
    /* Set the new UI theme */
    cb_theme_tree_selection_changed (selection, "/Net/ThemeName");
}

static void
cb_toolbar_style_combo_changed (GtkComboBox *combo)
{
    gint active;

    /* Get active item, prevent number outside the array (stay within zero-index) */
    active = CLAMP (gtk_combo_box_get_active (combo), 0, (gint) G_N_ELEMENTS (toolbar_styles_array)-1);

    /* Save setting */
    blconf_channel_set_string (xsettings_channel, "/Gtk/ToolbarStyle", toolbar_styles_array[active]);
}

static void
cb_antialias_check_button_toggled (GtkToggleButton *toggle)
{
    gint active;

    /* Don't allow an inconsistent button anymore */
    gtk_toggle_button_set_inconsistent (toggle, FALSE);

    /* Get active */
    active = gtk_toggle_button_get_active (toggle) ? 1 : 0;

    /* Save setting */
    blconf_channel_set_int (xsettings_channel, "/Xft/Antialias", active);
}

static void
cb_hinting_style_combo_changed (GtkComboBox *combo)
{
    gint active;

    /* Get active item, prevent number outside the array (stay within zero-index) */
    active = CLAMP (gtk_combo_box_get_active (combo), 0, (gint) G_N_ELEMENTS (xft_hint_styles_array)-1);

    /* Save setting */
    blconf_channel_set_string (xsettings_channel, "/Xft/HintStyle", xft_hint_styles_array[active]);

    /* Also update /Xft/Hinting to match */
    blconf_channel_set_int (xsettings_channel, "/Xft/Hinting", active > 0 ? 1 : 0);
}

static void
cb_rgba_style_combo_changed (GtkComboBox *combo)
{
    gint active;

    /* Get active item, prevent number outside the array (stay within zero-index) */
    active = CLAMP (gtk_combo_box_get_active (combo), 0, (gint) G_N_ELEMENTS (xft_rgba_array)-1);

    /* Save setting */
    blconf_channel_set_string (xsettings_channel, "/Xft/RGBA", xft_rgba_array[active]);
}

static void
cb_custom_dpi_check_button_toggled (GtkToggleButton *custom_dpi_toggle,
                                    GtkSpinButton   *custom_dpi_spin)
{
    gint dpi;

    if (gtk_toggle_button_get_active (custom_dpi_toggle))
    {
        /* Custom DPI is activated, so restore the last custom DPI we know about */
        dpi = blconf_channel_get_int (xsettings_channel, "/Xfce/LastCustomDPI", -1);

        /* Unfortunately, we don't have a valid custom DPI value to use, so compute it */
        if (dpi <= 0)
            dpi = compute_xsettings_dpi (GTK_WIDGET (custom_dpi_toggle));

        /* Apply the computed custom DPI value */
        blconf_channel_set_int (xsettings_channel, "/Xft/DPI", dpi);

        gtk_widget_set_sensitive (GTK_WIDGET (custom_dpi_spin), TRUE);
    }
    else
    {
        /* Custom DPI is deactivated, so remember the current value as the last custom DPI */
        dpi = gtk_spin_button_get_value_as_int (custom_dpi_spin);
        blconf_channel_set_int (xsettings_channel, "/Xfce/LastCustomDPI", dpi);

        /* Tell blsettingsd to compute the value itself */
        blconf_channel_set_int (xsettings_channel, "/Xft/DPI", -1);

        /* Make the spin button insensitive */
        gtk_widget_set_sensitive (GTK_WIDGET (custom_dpi_spin), FALSE);
    }
}

static void
cb_custom_dpi_spin_button_changed (GtkSpinButton   *custom_dpi_spin,
                                   GtkToggleButton *custom_dpi_toggle)
{
    gint dpi = gtk_spin_button_get_value_as_int (custom_dpi_spin);

    if (GTK_WIDGET_IS_SENSITIVE (custom_dpi_spin) && gtk_toggle_button_get_active (custom_dpi_toggle))
    {
        /* Custom DPI is turned on and the spin button has changed, so remember the value */
        blconf_channel_set_int (xsettings_channel, "/Xfce/LastCustomDPI", dpi);
    }

    /* Tell blsettingsd to apply the custom DPI value */
    blconf_channel_set_int (xsettings_channel, "/Xft/DPI", dpi);
}

#ifdef ENABLE_SOUND_SETTINGS
static void
cb_enable_event_sounds_check_button_toggled (GtkToggleButton *toggle, GtkWidget *button)
{
    gboolean active;

    active = gtk_toggle_button_get_active (toggle);
    gtk_widget_set_sensitive (button, active);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), active);
}
#endif

static gboolean
appearance_settings_load_icon_themes (preview_data *pd)
{
    GtkListStore *list_store;
    GtkTreeView  *tree_view;
    GDir         *dir;
    GtkTreePath  *tree_path;
    GtkTreeIter   iter;
    XfceRc       *index_file;
    const gchar  *file;
    gchar       **icon_theme_dirs;
    gchar        *index_filename;
    const gchar  *theme_name;
    const gchar  *theme_comment;
    gchar        *name_escaped;
    gchar        *comment_escaped;
    gchar        *visible_name;
    gchar        *active_theme_name;
    gsize         i;
    gsize         p;
    GSList       *check_list = NULL;
    gchar        *cache_filename;
    gboolean      has_cache;
    gchar        *cache_tooltip;
    GtkIconTheme *icon_theme;
    GdkPixbuf    *preview;
    GdkPixbuf    *icon;
    gchar*        preview_icons[4] = { "folder", "go-down", "audio-volume-high", "web-browser" };
    int           coords[4][2] = { { 4, 4 }, { 24, 4 }, { 4, 24 }, { 24, 24 } };

    g_return_val_if_fail (pd != NULL, FALSE);

    list_store = pd->list_store;
    tree_view = pd->tree_view;

    /* Determine current theme */
    active_theme_name = blconf_channel_get_string (xsettings_channel, "/Net/IconThemeName", "Rodent");

    /* Determine directories to look in for icon themes */
    xfce_resource_push_path (XFCE_RESOURCE_ICONS, DATADIR G_DIR_SEPARATOR_S "icons");
    icon_theme_dirs = xfce_resource_dirs (XFCE_RESOURCE_ICONS);
    xfce_resource_pop_path (XFCE_RESOURCE_ICONS);

    /* Iterate over all base directories */
    for (i = 0; icon_theme_dirs[i] != NULL; ++i)
    {
        /* Open directory handle */
        dir = g_dir_open (icon_theme_dirs[i], 0, NULL);

        /* Try next base directory if this one cannot be read */
        if (G_UNLIKELY (dir == NULL))
            continue;

        /* Iterate over filenames in the directory */
        while ((file = g_dir_read_name (dir)) != NULL)
        {
            /* Build filename for the index.theme of the current icon theme directory */
            index_filename = g_build_filename (icon_theme_dirs[i], file, "index.theme", NULL);

            /* Try to open the theme index file */
            index_file = xfce_rc_simple_open (index_filename, TRUE);

            if (index_file != NULL
                && g_slist_find_custom (check_list, file, (GCompareFunc) g_utf8_collate) == NULL)
            {
                /* Set the icon theme group */
                xfce_rc_set_group (index_file, "Icon Theme");

                /* Check if the icon theme is valid and visible to the user */
                if (G_LIKELY (xfce_rc_has_entry (index_file, "Directories")
                              && !xfce_rc_read_bool_entry (index_file, "Hidden", FALSE)))
                {
                    /* Insert the theme in the check list */
                    check_list = g_slist_prepend (check_list, g_strdup (file));

                    /* Create the icon-theme preview */
                    preview = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 44, 44);
                    gdk_pixbuf_fill (preview, 0x00);
                    icon_theme = gtk_icon_theme_new ();
                    gtk_icon_theme_set_custom_theme (icon_theme, file);

                    for (p = 0; p < 4; p++)
                    {
                        icon = NULL;
                        if (gtk_icon_theme_has_icon (icon_theme, preview_icons[p]))
                            icon = gtk_icon_theme_load_icon (icon_theme, preview_icons[p], 16, 0, NULL);
                        else if (gtk_icon_theme_has_icon (icon_theme, "image-missing"))
                            icon = gtk_icon_theme_load_icon (icon_theme, "image-missing", 16, 0, NULL);

                        if (icon)
                        {
                            gdk_pixbuf_copy_area (icon, 0, 0, 16, 16, preview, coords[p][0], coords[p][1]);
                            g_object_unref (icon);
                        }
                    }

                    /* Get translated icon theme name and comment */
                    theme_name = xfce_rc_read_entry (index_file, "Name", file);
                    theme_comment = xfce_rc_read_entry (index_file, "Comment", NULL);

                    /* Escape the theme's name and comment, since they are markup, not text */
                    name_escaped = g_markup_escape_text (theme_name, -1);
                    comment_escaped = theme_comment ? g_markup_escape_text (theme_comment, -1) : NULL;
                    visible_name = g_strdup_printf ("<b>%s</b>\n%s", name_escaped, comment_escaped);
                    g_free (name_escaped);
                    g_free (comment_escaped);

                    /* Cache filename */
                    cache_filename = g_build_filename (icon_theme_dirs[i], file, "icon-theme.cache", NULL);
                    has_cache = g_file_test (cache_filename, G_FILE_TEST_IS_REGULAR);
                    g_free (cache_filename);

                    /* If the theme has no cache, mention this in the tooltip */
                    if (!has_cache)
                        cache_tooltip = g_strdup_printf (_("Warning: this icon theme has no cache file. You can create this by "
                                                           "running <i>gtk-update-icon-cache %s/%s/</i> in a terminal emulator."),
                                                         icon_theme_dirs[i], file);
                    else
                        cache_tooltip = NULL;

                    /* Append icon theme to the list store */
                    gtk_list_store_append (list_store, &iter);
                    gtk_list_store_set (list_store, &iter,
                                        COLUMN_THEME_PREVIEW, preview,
                                        COLUMN_THEME_NAME, file,
                                        COLUMN_THEME_DISPLAY_NAME, visible_name,
                                        COLUMN_THEME_NO_CACHE, !has_cache,
                                        COLUMN_THEME_COMMENT, cache_tooltip,
                                        -1);

                    /* Check if this is the active theme, if so, select it */
                    if (G_UNLIKELY (g_utf8_collate (file, active_theme_name) == 0))
                    {
                        tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_store), &iter);
                        gtk_tree_selection_select_path (gtk_tree_view_get_selection (tree_view), tree_path);
                        gtk_tree_view_scroll_to_cell (tree_view, tree_path, NULL, TRUE, 0.5, 0);
                        gtk_tree_path_free (tree_path);
                    }

                    g_object_unref (icon_theme);
                    g_object_unref (preview);
                }
            }

            /* Close theme index file */
            if (G_LIKELY (index_file))
                xfce_rc_close (index_file);

            /* Free theme index filename */
            g_free (index_filename);
        }

        /* Close directory handle */
        g_dir_close (dir);
    }

    /* Free active theme name */
    g_free (active_theme_name);

    /* Free list of base directories */
    g_strfreev (icon_theme_dirs);

    /* Free the check list */
    if (G_LIKELY (check_list))
    {
        g_slist_foreach (check_list, (GFunc) (void (*)(void)) g_free, NULL);
        g_slist_free (check_list);
    }

    return FALSE;
}

static gboolean
appearance_settings_load_ui_themes (preview_data *pd)
{
    GtkListStore *list_store;
    GtkTreeView  *tree_view;
    GDir         *dir;
    GtkTreePath  *tree_path;
    GtkTreeIter   iter;
    XfceRc       *index_file;
    const gchar  *file;
    gchar       **ui_theme_dirs;
    gchar        *index_filename;
    const gchar  *theme_name;
    const gchar  *theme_comment;
    gchar        *active_theme_name;
    gchar        *gtkrc_filename;
    gchar        *comment_escaped;
    gint          i;
    GSList       *check_list = NULL;
    gchar        *color_scheme = NULL;
    GdkPixbuf    *preview;
    GdkColor      colors[NUM_SYMBOLIC_COLORS];

    g_return_val_if_fail (pd != NULL, FALSE);

    list_store = pd->list_store;
    tree_view = pd->tree_view;

    /* Determine current theme */
    active_theme_name = blconf_channel_get_string (xsettings_channel, "/Net/ThemeName", "Default");

    /* Determine directories to look in for ui themes */
    xfce_resource_push_path (XFCE_RESOURCE_THEMES, DATADIR G_DIR_SEPARATOR_S "themes");
    ui_theme_dirs = xfce_resource_dirs (XFCE_RESOURCE_THEMES);
    xfce_resource_pop_path (XFCE_RESOURCE_THEMES);

    /* Iterate over all base directories */
    for (i = 0; ui_theme_dirs[i] != NULL; ++i)
    {
        /* Open directory handle */
        dir = g_dir_open (ui_theme_dirs[i], 0, NULL);

        /* Try next base directory if this one cannot be read */
        if (G_UNLIKELY (dir == NULL))
            continue;

        /* Iterate over filenames in the directory */
        while ((file = g_dir_read_name (dir)) != NULL)
        {
            /* Build the theme style filename */
            gtkrc_filename = g_build_filename (ui_theme_dirs[i], file, "gtk-2.0", "gtkrc", NULL);

            /* Check if the gtkrc file exists and the theme is not already in the list */
            if (g_file_test (gtkrc_filename, G_FILE_TEST_EXISTS)
                && g_slist_find_custom (check_list, file, (GCompareFunc) g_utf8_collate) == NULL)
            {
                /* Insert the theme in the check list */
                check_list = g_slist_prepend (check_list, g_strdup (file));

                /* Build filename for the index.theme of the current ui theme directory */
                index_filename = g_build_filename (ui_theme_dirs[i], file, "index.theme", NULL);

                /* Try to open the theme index file */
                index_file = xfce_rc_simple_open (index_filename, TRUE);

                if (G_LIKELY (index_file != NULL))
                {
                    /* Get translated ui theme name and comment */
                    theme_name = xfce_rc_read_entry (index_file, "Name", file);
                    theme_comment = xfce_rc_read_entry (index_file, "Comment", NULL);

                    /* Escape the comment because tooltips are markup, not text */
                    comment_escaped = theme_comment ? g_markup_escape_text (theme_comment, -1) : NULL;
                }
                else
                {
                    /* Set defaults */
                    theme_name = file;
                    comment_escaped = NULL;
                }

                /* Retrieve the color values from the theme, parse them and create the palette preview pixbuf */
                color_scheme = gtkrc_get_color_scheme_for_theme (gtkrc_filename);
                if (color_scheme_parse_colors (color_scheme, colors))
                    preview = theme_create_preview (colors);
                /* If the color scheme parsing doesn't return anything useful, show a blank pixbuf */
                else
                {
                    preview = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 44, 22);
                    gdk_pixbuf_fill (preview, 0x00);
                }
                g_free (color_scheme);

                /* Append ui theme to the list store */
                gtk_list_store_append (list_store, &iter);
                gtk_list_store_set (list_store, &iter,
                                    COLUMN_THEME_PREVIEW, preview,
                                    COLUMN_THEME_NAME, file,
                                    COLUMN_THEME_DISPLAY_NAME, theme_name,
                                    COLUMN_THEME_COMMENT, comment_escaped, -1);

                /* Cleanup */
                if (G_LIKELY (index_file != NULL))
                    xfce_rc_close (index_file);
                g_free (comment_escaped);
                g_object_unref (preview);

                /* Check if this is the active theme, if so, select it */
                if (G_UNLIKELY (g_utf8_collate (file, active_theme_name) == 0))
                {
                    tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_store), &iter);
                    gtk_tree_selection_select_path (gtk_tree_view_get_selection (tree_view), tree_path);
                    gtk_tree_view_scroll_to_cell (tree_view, tree_path, NULL, TRUE, 0.5, 0);
                    gtk_tree_path_free (tree_path);
                }

                /* Free theme index filename */
                g_free (index_filename);
            }

            /* Free gtkrc filename */
            g_free (gtkrc_filename);
        }

        /* Close directory handle */
        g_dir_close (dir);
    }

    /* Free active theme name */
    g_free (active_theme_name);

    /* Free list of base directories */
    g_strfreev (ui_theme_dirs);

    /* Free the check list */
    if (G_LIKELY (check_list))
    {
        g_slist_foreach (check_list, (GFunc) (void (*)(void)) g_free, NULL);
        g_slist_free (check_list);
    }

    return FALSE;
}

static void
appearance_settings_dialog_channel_property_changed (BlconfChannel *channel,
                                                     const gchar   *property_name,
                                                     const GValue  *value,
                                                     GtkBuilder    *builder)
{
    GObject      *object;
    gchar        *str;
    guint         i;
    gint          antialias, dpi, custom_dpi;
    GtkTreeModel *model;

    g_return_if_fail (property_name != NULL);
    g_return_if_fail (GTK_IS_BUILDER (builder));

    if (strcmp (property_name, "/Xft/RGBA") == 0)
    {
        str = blconf_channel_get_string (xsettings_channel, property_name, xft_rgba_array[0]);
        for (i = 0; i < G_N_ELEMENTS (xft_rgba_array); i++)
        {
            if (strcmp (str, xft_rgba_array[i]) == 0)
            {
                object = gtk_builder_get_object (builder, "xft_rgba_combo_box");
                gtk_combo_box_set_active (GTK_COMBO_BOX (object), i);
                break;
            }
        }
        g_free (str);
    }
    else if (strcmp (property_name, "/Gtk/ToolbarStyle") == 0)
    {
        str = blconf_channel_get_string (xsettings_channel, property_name, toolbar_styles_array[2]);
        for (i = 0; i < G_N_ELEMENTS (toolbar_styles_array); i++)
        {
            if (strcmp (str, toolbar_styles_array[i]) == 0)
            {
                object = gtk_builder_get_object (builder, "gtk_toolbar_style_combo_box");
                gtk_combo_box_set_active (GTK_COMBO_BOX (object), i);
                break;
            }
        }
        g_free (str);
    }
    else if (strcmp (property_name, "/Xft/HintStyle") == 0)
    {
        str = blconf_channel_get_string (xsettings_channel, property_name, xft_hint_styles_array[0]);
        for (i = 0; i < G_N_ELEMENTS (xft_hint_styles_array); i++)
        {
            if (strcmp (str, xft_hint_styles_array[i]) == 0)
            {
                object = gtk_builder_get_object (builder, "xft_hinting_style_combo_box");
                gtk_combo_box_set_active (GTK_COMBO_BOX (object), i);
                break;
            }
        }
        g_free (str);
    }
    else if (strcmp (property_name, "/Xft/Antialias") == 0)
    {
        object = gtk_builder_get_object (builder, "xft_antialias_check_button");
        antialias = blconf_channel_get_int (xsettings_channel, property_name, -1);
        switch (antialias)
        {
            case 1:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), TRUE);
                break;

            case 0:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), FALSE);
                break;

            default: /* -1 */
                gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (object), TRUE);
                break;
        }
    }
    else if (strcmp (property_name, "/Xft/DPI") == 0)
    {
        /* The DPI has changed, so get its value and the last known custom value */
        dpi = blconf_channel_get_int (xsettings_channel, property_name, FALLBACK_DPI);
        custom_dpi = blconf_channel_get_int (xsettings_channel, "/Xfce/LastCustomDPI", -1);

        /* Activate the check button if we're using a custom DPI */
        object = gtk_builder_get_object (builder, "xft_custom_dpi_check_button");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), dpi >= 0);

        /* If we're not using a custom DPI, compute the future custom DPI automatically */
        if (custom_dpi == -1)
            custom_dpi = compute_xsettings_dpi (GTK_WIDGET (object));

        object = gtk_builder_get_object (builder, "xft_custom_dpi_spin_button");

        if (dpi > 0)
        {
            /* We're using a custom DPI, so use the current DPI setting for the spin value */
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (object), dpi);
        }
        else
        {
            /* Set the spin button value to the last custom DPI */
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (object), custom_dpi);
        }
    }
    else if (strcmp (property_name, "/Net/ThemeName") == 0)
    {
        GtkTreeIter iter;
        gboolean    reload;

        object = gtk_builder_get_object (builder, "gtk_theme_treeview");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (object));
        reload = TRUE;

        if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (object)),
                                             &model,
                                             &iter))
        {
            gchar *selected_name;
            gchar *new_name;

            gtk_tree_model_get (model, &iter, COLUMN_THEME_NAME, &selected_name, -1);

            new_name = blconf_channel_get_string (channel, property_name, NULL);

            reload = (strcmp (new_name, selected_name) != 0);

            g_free (selected_name);
            g_free (new_name);
        }

        if (reload)
        {
            preview_data *pd;

            gtk_list_store_clear (GTK_LIST_STORE (model));

            pd = preview_data_new (GTK_LIST_STORE (model), GTK_TREE_VIEW (object));
            g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                             (GSourceFunc) appearance_settings_load_ui_themes,
                             pd,
                             (GDestroyNotify) preview_data_free);
        }
    }
    else if (strcmp (property_name, "/Net/IconThemeName") == 0)
    {
        GtkTreeIter iter;
        gboolean    reload;

        reload = TRUE;

        object = gtk_builder_get_object (builder, "icon_theme_treeview");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (object));

        if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (object)),
                                             &model,
                                             &iter))
        {
            gchar *selected_name;
            gchar *new_name;

            gtk_tree_model_get (model, &iter, COLUMN_THEME_NAME, &selected_name, -1);

            new_name = blconf_channel_get_string (channel, property_name, NULL);

            reload = (strcmp (new_name, selected_name) != 0);

            g_free (selected_name);
            g_free (new_name);
        }


        if (reload)
        {
            preview_data *pd;

            gtk_list_store_clear (GTK_LIST_STORE (model));
            pd = preview_data_new (GTK_LIST_STORE (model), GTK_TREE_VIEW (object));
            g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                             (GSourceFunc) appearance_settings_load_icon_themes,
                             pd,
                             (GDestroyNotify) preview_data_free);
        }
    }
}

static void
cb_theme_uri_dropped (GtkWidget        *widget,
                      GdkDragContext   *drag_context,
                      gint              x,
                      gint              y,
                      GtkSelectionData *data,
                      guint             info,
                      guint             timestamp,
                      GtkBuilder       *builder)
{
    gchar        **uris;
    gchar         *argv[3];
    guint          i;
    GError        *error = NULL;
    gint           status;
    GtkWidget     *toplevel = gtk_widget_get_toplevel (widget);
    gchar         *filename;
    GdkCursor     *cursor;
    GdkWindow     *gdkwindow;
    gboolean       something_installed = FALSE;
    GObject       *object;
    GtkTreeModel  *model;
    preview_data  *pd;

    uris = gtk_selection_data_get_uris (data);
    if (uris == NULL)
        return;

    argv[0] = HELPERDIR G_DIR_SEPARATOR_S "appearance-install-theme";
    argv[2] = NULL;

    /* inform the user we are installing the theme */
    gdkwindow = gtk_widget_get_window (widget);
    cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget), GDK_WATCH);
    gdk_window_set_cursor (gdkwindow, cursor);

    /* iterate main loop to show cursor */
    while (gtk_events_pending ())
        gtk_main_iteration ();

    for (i = 0; uris[i] != NULL; i++)
    {
        filename = g_filename_from_uri (uris[i], NULL, NULL);
        if (filename == NULL)
            continue;

        argv[1] = filename;

        if (g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, &status, &error)
            && status > 0)
        {
            switch (WEXITSTATUS (status))
            {
                case 2:
                    g_set_error (&error, G_SPAWN_ERROR, 0,
                        _("File is larger than %d MB, installation aborted"), 50);
                    break;

                case 3:
                    g_set_error_literal (&error, G_SPAWN_ERROR, 0,
                        _("Failed to create temporary directory"));
                    break;

                case 4:
                    g_set_error_literal (&error, G_SPAWN_ERROR, 0,
                        _("Failed to extract archive"));
                    break;

                case 5:
                    g_set_error_literal (&error, G_SPAWN_ERROR, 0,
                        _("Unknown format, only archives and directories are supported"));
                    break;

                default:
                    g_set_error (&error, G_SPAWN_ERROR,
                        0, _("An unknown error, exit code is %d"), WEXITSTATUS (status));
                    break;
            }
        }

        if (error != NULL)
        {
            xfce_dialog_show_error (GTK_WINDOW (toplevel), error, _("Failed to install theme"));
            g_clear_error (&error);
        }
        else
        {
            something_installed = TRUE;
        }

        g_free (filename);
    }

    g_strfreev (uris);
    gdk_window_set_cursor (gdkwindow, NULL);
    gdk_cursor_unref (cursor);

    if (something_installed)
    {
        /* reload icon theme treeview in an idle loop */
        object = gtk_builder_get_object (builder, "icon_theme_treeview");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (object));
        gtk_list_store_clear (GTK_LIST_STORE (model));
        pd = preview_data_new (GTK_LIST_STORE (model), GTK_TREE_VIEW (object));
        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                         (GSourceFunc) appearance_settings_load_icon_themes,
                         pd,
                         (GDestroyNotify) preview_data_free);

        /* reload gtk theme treeview */
        object = gtk_builder_get_object (builder, "gtk_theme_treeview");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (object));
        gtk_list_store_clear (GTK_LIST_STORE (model));

        pd = preview_data_new (GTK_LIST_STORE (model), GTK_TREE_VIEW (object));
        g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                         (GSourceFunc) appearance_settings_load_ui_themes,
                         pd,
                         (GDestroyNotify) preview_data_free);
    }
}

static void
appearance_settings_dialog_configure_widgets (GtkBuilder *builder)
{
    GObject           *object, *object2;
    GtkListStore      *list_store;
    GtkCellRenderer   *renderer;
    GdkPixbuf         *pixbuf;
    GtkTreeSelection  *selection;
    GtkTreeViewColumn *column;
    preview_data      *pd;

    /* Icon themes list */
    object = gtk_builder_get_object (builder, "icon_theme_treeview");

    list_store = gtk_list_store_new (N_THEME_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), COLUMN_THEME_DISPLAY_NAME, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (object), GTK_TREE_MODEL (list_store));
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (object), COLUMN_THEME_COMMENT);
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (object), TRUE);

    /* Single-column layout */
    column = gtk_tree_view_column_new ();

    /* Icon Previews */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_set_attributes (column, renderer, "pixbuf", COLUMN_THEME_PREVIEW, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (object), column);

    /* Theme Name and Description */
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer, "markup", COLUMN_THEME_DISPLAY_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    /* Warning Icon */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_set_attributes (column, renderer, "visible", COLUMN_THEME_NO_CACHE, NULL);
    g_object_set (G_OBJECT (renderer), "icon-name", GTK_STOCK_DIALOG_WARNING, NULL);

    pd = preview_data_new (GTK_LIST_STORE (list_store), GTK_TREE_VIEW (object));
    g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                     (GSourceFunc) appearance_settings_load_icon_themes,
                     pd,
                     (GDestroyNotify) preview_data_free);

    g_object_unref (G_OBJECT (list_store));

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (cb_icon_theme_tree_selection_changed), NULL);

    gtk_drag_dest_set (GTK_WIDGET (object), GTK_DEST_DEFAULT_ALL,
                       theme_drop_targets, G_N_ELEMENTS (theme_drop_targets),
                       GDK_ACTION_COPY);
    g_signal_connect (G_OBJECT (object), "drag-data-received", G_CALLBACK (cb_theme_uri_dropped), builder);

    /* Gtk (UI) themes */
    object = gtk_builder_get_object (builder, "gtk_theme_treeview");

    list_store = gtk_list_store_new (N_THEME_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), COLUMN_THEME_DISPLAY_NAME, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (object), GTK_TREE_MODEL (list_store));
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (object), COLUMN_THEME_COMMENT);

    /* Single-column layout */
    column = gtk_tree_view_column_new ();

    /* Icon Previews */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_set_attributes (column, renderer, "pixbuf", COLUMN_THEME_PREVIEW, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (object), column);

    /* Theme Name and Description */
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer, "text", COLUMN_THEME_DISPLAY_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    pd = preview_data_new (list_store, GTK_TREE_VIEW (object));
    g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                     (GSourceFunc) appearance_settings_load_ui_themes,
                     pd,
                     (GDestroyNotify) preview_data_free);

    g_object_unref (G_OBJECT (list_store));

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (cb_ui_theme_tree_selection_changed), NULL);

    gtk_drag_dest_set (GTK_WIDGET (object), GTK_DEST_DEFAULT_ALL,
                       theme_drop_targets, G_N_ELEMENTS (theme_drop_targets),
                       GDK_ACTION_COPY);
    g_signal_connect (G_OBJECT (object), "drag-data-received", G_CALLBACK (cb_theme_uri_dropped), builder);

    /* Subpixel (rgba) hinting Combo */
    object = gtk_builder_get_object (builder, "xft_rgba_store");

    pixbuf = gdk_pixbuf_new_from_xpm_data (rgba_image_none_xpm);
    gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 0, 0, pixbuf, 1, _("None"), -1);
    g_object_unref (G_OBJECT (pixbuf));

    pixbuf = gdk_pixbuf_new_from_xpm_data (rgba_image_rgb_xpm);
    gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 1, 0, pixbuf, 1, _("RGB"), -1);
    g_object_unref (G_OBJECT (pixbuf));

    pixbuf = gdk_pixbuf_new_from_xpm_data (rgba_image_bgr_xpm);
    gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 2, 0, pixbuf, 1, _("BGR"), -1);
    g_object_unref (G_OBJECT (pixbuf));

    pixbuf = gdk_pixbuf_new_from_xpm_data (rgba_image_vrgb_xpm);
    gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 3, 0, pixbuf, 1, _("Vertical RGB"), -1);
    g_object_unref (G_OBJECT (pixbuf));

    pixbuf = gdk_pixbuf_new_from_xpm_data (rgba_image_vbgr_xpm);
    gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 4, 0, pixbuf, 1, _("Vertical BGR"), -1);
    g_object_unref (G_OBJECT (pixbuf));

    object = gtk_builder_get_object (builder, "xft_rgba_combo_box");
    appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Xft/RGBA", NULL, builder);
    g_signal_connect (G_OBJECT (object), "changed", G_CALLBACK (cb_rgba_style_combo_changed), NULL);

    /* Enable editable menu accelerators */
    object = gtk_builder_get_object (builder, "gtk_caneditaccels_check_button");
    blconf_g_property_bind (xsettings_channel, "/Gtk/CanChangeAccels", G_TYPE_BOOLEAN,
                            G_OBJECT (object), "active");

    /* Show menu images */
    object = gtk_builder_get_object (builder, "gtk_menu_images_check_button");
    blconf_g_property_bind (xsettings_channel, "/Gtk/MenuImages", G_TYPE_BOOLEAN,
                            G_OBJECT (object), "active");

    /* Show button images */
    object = gtk_builder_get_object (builder, "gtk_button_images_check_button");
    blconf_g_property_bind (xsettings_channel, "/Gtk/ButtonImages", G_TYPE_BOOLEAN,
                            G_OBJECT (object), "active");

    /* Font name */
    object = gtk_builder_get_object (builder, "gtk_fontname_button");
    blconf_g_property_bind (xsettings_channel,  "/Gtk/FontName", G_TYPE_STRING,
                            G_OBJECT (object), "font-name");

    /* Monospace font name */
    object = gtk_builder_get_object (builder, "gtk_monospace_fontname_button");
    blconf_g_property_bind (xsettings_channel,  "/Gtk/MonospaceFontName", G_TYPE_STRING,
                            G_OBJECT (object), "font-name");

    /* Toolbar style */
    object = gtk_builder_get_object (builder, "gtk_toolbar_style_combo_box");
    appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Gtk/ToolbarStyle", NULL, builder);
    g_signal_connect (G_OBJECT (object), "changed", G_CALLBACK(cb_toolbar_style_combo_changed), NULL);

    /* Hinting style */
    object = gtk_builder_get_object (builder, "xft_hinting_style_combo_box");
    appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Xft/HintStyle", NULL, builder);
    g_signal_connect (G_OBJECT (object), "changed", G_CALLBACK (cb_hinting_style_combo_changed), NULL);

    /* Hinting */
    object = gtk_builder_get_object (builder, "xft_antialias_check_button");
    appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Xft/Antialias", NULL, builder);
    g_signal_connect (G_OBJECT (object), "toggled", G_CALLBACK (cb_antialias_check_button_toggled), NULL);

    /* DPI */
    object = gtk_builder_get_object (builder, "xft_custom_dpi_check_button");
    object2 = gtk_builder_get_object (builder, "xft_custom_dpi_spin_button");
    appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Xft/DPI", NULL, builder);
    gtk_widget_set_sensitive (GTK_WIDGET (object2), gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object)));
    g_signal_connect (G_OBJECT (object), "toggled", G_CALLBACK (cb_custom_dpi_check_button_toggled), object2);
    g_signal_connect (G_OBJECT (object2), "value-changed", G_CALLBACK (cb_custom_dpi_spin_button_changed), object);

#ifdef ENABLE_SOUND_SETTINGS
    /* Sounds */
    object = gtk_builder_get_object (builder, "event_sounds_frame");
    gtk_widget_show (GTK_WIDGET (object));

    object = gtk_builder_get_object (builder, "enable_event_sounds_check_button");
    object2  = gtk_builder_get_object (builder, "enable_input_feedback_sounds_button");

    g_signal_connect (G_OBJECT (object), "toggled",
                      G_CALLBACK (cb_enable_event_sounds_check_button_toggled), object2);

    blconf_g_property_bind (xsettings_channel, "/Net/EnableEventSounds", G_TYPE_BOOLEAN,
                            G_OBJECT (object), "active");
    blconf_g_property_bind (xsettings_channel, "/Net/EnableInputFeedbackSounds", G_TYPE_BOOLEAN,
                            G_OBJECT (object2), "active");

    gtk_widget_set_sensitive (GTK_WIDGET (object2), gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object)));
#endif
}

static void
appearance_settings_dialog_response (GtkWidget *dialog,
                                     gint       response_id)
{
    if (response_id == GTK_RESPONSE_HELP)
        xfce_dialog_show_help_with_version (GTK_WINDOW (dialog), "blade-settings", "appearance",
                                            NULL, BLADE_SETTINGS_VERSION_SHORT);
    else
        gtk_main_quit ();
}

gint
main (gint argc, gchar **argv)
{
    GObject    *dialog, *plug_child;
    GtkWidget  *plug;
    GtkBuilder *builder;
    GError     *error = NULL;

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize Gtk+ */
    if (!gtk_init_with_args (&argc, &argv, "", option_entries, GETTEXT_PACKAGE, &error))
    {
        if (G_LIKELY (error))
        {
            /* print error */
            g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print ("\n");

            /* cleanup */
            g_error_free (error);
        }
        else
        {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    /* print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2008-2018");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* initialize blconf */
    if (!blconf_init (&error))
    {
        /* print error and exit */
        g_error ("Failed to connect to blconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* open the xsettings channel */
    xsettings_channel = blconf_channel_new ("xsettings");
    if (G_LIKELY (xsettings_channel))
    {
        /* hook to make sure the libbladeui library is linked */
        if (xfce_titled_dialog_get_type () == 0)
            return EXIT_FAILURE;

        /* load the gtk user interface file*/
        builder = gtk_builder_new ();
        if (gtk_builder_add_from_string (builder, appearance_dialog_ui,
                                         appearance_dialog_ui_length, &error) != 0)
          {
            /* connect signal to monitor the channel */
            g_signal_connect (G_OBJECT (xsettings_channel), "property-changed",
                G_CALLBACK (appearance_settings_dialog_channel_property_changed), builder);

            appearance_settings_dialog_configure_widgets (builder);

            if (G_UNLIKELY (opt_socket_id == 0))
            {
                /* build the dialog */
                dialog = gtk_builder_get_object (builder, "dialog");

                g_signal_connect (dialog, "response",
                    G_CALLBACK (appearance_settings_dialog_response), NULL);
                gtk_window_present (GTK_WINDOW (dialog));

                /* To prevent the settings dialog to be saved in the session */
                gdk_set_sm_client_id ("FAKE ID");

                gtk_main ();
            }
            else
            {
                /* Create plug widget */
                plug = gtk_plug_new (opt_socket_id);
                g_signal_connect (plug, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
                gtk_widget_show (plug);

                /* Stop startup notification */
                gdk_notify_startup_complete ();

                /* Get plug child widget */
                plug_child = gtk_builder_get_object (builder, "plug-child");
                gtk_widget_reparent (GTK_WIDGET (plug_child), plug);
                gtk_widget_show (GTK_WIDGET (plug_child));

                /* To prevent the settings dialog to be saved in the session */
                gdk_set_sm_client_id ("FAKE ID");

                /* Enter main loop */
                gtk_main ();
            }
        }
        else
        {
            g_error ("Failed to load the UI file: %s.", error->message);
            g_error_free (error);
        }

        /* Release Builder */
        g_object_unref (G_OBJECT (builder));

        /* release the channel */
        g_object_unref (G_OBJECT (xsettings_channel));
    }

    /* shutdown blconf */
    blconf_shutdown ();

    return EXIT_SUCCESS;
}
