/*
 *  blade-settings-manager
 *
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *                     Jannis Pohlmann <jannis@xfce.org>
 *  Copyright (c) 2012 Nick Schermer <nick@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libbladeutil/libbladeutil.h>
#include <libbladeui/libbladeui.h>
#include <blconf/blconf.h>
#include <pojk/pojk.h>
#include <blxo/blxo.h>

#include "blade-settings-manager-dialog.h"
#include "xfce-text-renderer.h"

#define TEXT_WIDTH (128)
#define ICON_WIDTH (48)



struct _XfceSettingsManagerDialogClass
{
    XfceTitledDialogClass __parent__;
};

struct _XfceSettingsManagerDialog
{
    XfceTitledDialog __parent__;

    BlconfChannel  *channel;
    PojkMenu     *menu;

    GtkListStore   *store;

    GtkWidget      *filter_entry;
    gchar          *filter_text;

    GtkWidget      *category_viewport;
    GtkWidget      *category_scroll;
    GtkWidget      *category_box;

    GList          *categories;

    GtkWidget      *socket_scroll;
    GtkWidget      *socket_viewport;
    PojkMenuItem *socket_item;

    GtkWidget      *button_back;
    GtkWidget      *button_help;

    gchar          *help_page;
    gchar          *help_component;
    gchar          *help_version;
};

typedef struct
{
    PojkMenuDirectory       *directory;
    XfceSettingsManagerDialog *dialog;
    GtkWidget                 *iconview;
    GtkWidget                 *box;
}
DialogCategory;



enum
{
    COLUMN_NAME,
    COLUMN_ICON_NAME,
    COLUMN_TOOLTIP,
    COLUMN_MENU_ITEM,
    COLUMN_MENU_DIRECTORY,
    COLUMN_FILTER_TEXT,
    N_COLUMNS
};



static void     xfce_settings_manager_dialog_finalize        (GObject                   *object);
static void     xfce_settings_manager_dialog_style_set       (GtkWidget                 *widget,
                                                              GtkStyle                  *old_style);
static void     xfce_settings_manager_dialog_response        (GtkDialog                 *widget,
                                                              gint                       response_id);
static void     xfce_settings_manager_dialog_header_style    (GtkWidget                 *header,
                                                              GtkStyle                  *old_style,
                                                              GtkWidget                 *ebox);
static void     xfce_settings_manager_dialog_set_title       (XfceSettingsManagerDialog *dialog,
                                                              const gchar               *title,
                                                              const gchar               *icon_name,
                                                              const gchar               *subtitle);
static void     xfce_settings_manager_dialog_go_back         (XfceSettingsManagerDialog *dialog);
static void     xfce_settings_manager_dialog_entry_changed   (GtkWidget                 *entry,
                                                              XfceSettingsManagerDialog *dialog);
static gboolean xfce_settings_manager_dialog_entry_key_press (GtkWidget                 *entry,
                                                              GdkEventKey               *event,
                                                              XfceSettingsManagerDialog *dialog);
static void     xfce_settings_manager_dialog_entry_clear     (GtkWidget                 *entry,
                                                              GtkEntryIconPosition       icon_pos,
                                                              GdkEvent                  *event);
static void     xfce_settings_manager_dialog_menu_reload     (XfceSettingsManagerDialog *dialog);
static void     xfce_settings_manager_dialog_scroll_to_item  (GtkWidget                 *iconview,
                                                              XfceSettingsManagerDialog *dialog);



G_DEFINE_TYPE (XfceSettingsManagerDialog, xfce_settings_manager_dialog, XFCE_TYPE_TITLED_DIALOG)



static void
xfce_settings_manager_dialog_class_init (XfceSettingsManagerDialogClass *klass)
{
    GObjectClass   *gobject_class;
    GtkDialogClass *gtkdialog_class;
    GtkWidgetClass *gtkwiget_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_settings_manager_dialog_finalize;

    gtkwiget_class = GTK_WIDGET_CLASS (klass);
    gtkwiget_class->style_set = xfce_settings_manager_dialog_style_set;

    gtkdialog_class = GTK_DIALOG_CLASS (klass);
    gtkdialog_class->response = xfce_settings_manager_dialog_response;
}



static void
xfce_settings_manager_dialog_init (XfceSettingsManagerDialog *dialog)
{
    GtkWidget *align;
    GtkWidget *bbox;
    GtkWidget *dialog_vbox;
    GtkWidget *ebox;
    GtkWidget *entry;
    GtkWidget *hbox;
    GtkWidget *header;
    GtkWidget *scroll;
    GtkWidget *viewport;
    GList     *children;
    gchar     *path;

    dialog->channel = blconf_channel_get ("blade-settings-manager");

    dialog->store = gtk_list_store_new (N_COLUMNS,
                                        G_TYPE_STRING,
                                        G_TYPE_STRING,
                                        G_TYPE_STRING,
                                        POJK_TYPE_MENU_ITEM,
                                        POJK_TYPE_MENU_DIRECTORY,
                                        G_TYPE_STRING);

    path = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, "menus/blade-settings-manager.menu");
    dialog->menu = pojk_menu_new_for_path (path != NULL ? path : MENUFILE);
    g_free (path);

    gtk_window_set_default_size (GTK_WINDOW (dialog),
      blconf_channel_get_int (dialog->channel, "/last/window-width", 640),
      blconf_channel_get_int (dialog->channel, "/last/window-height", 500));
    xfce_settings_manager_dialog_set_title (dialog, NULL, NULL, NULL);

    dialog->button_back = xfce_gtk_button_new_mixed (GTK_STOCK_GO_BACK, _("All _Settings"));
    bbox = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
    gtk_container_add (GTK_CONTAINER (bbox), dialog->button_back);
    gtk_widget_set_sensitive (dialog->button_back, FALSE);
    gtk_widget_show (dialog->button_back);
    g_signal_connect_swapped (G_OBJECT (dialog->button_back), "clicked",
        G_CALLBACK (xfce_settings_manager_dialog_go_back), dialog);

    dialog->button_help = gtk_dialog_add_button (GTK_DIALOG (dialog),
                                                 GTK_STOCK_HELP, GTK_RESPONSE_HELP);
    gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

    /* add box at start of the main box */
    hbox = gtk_hbox_new (FALSE, 0);
    dialog_vbox = gtk_bin_get_child (GTK_BIN (dialog));
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, TRUE, 0);
    gtk_box_reorder_child (GTK_BOX (dialog_vbox), hbox, 0);
    gtk_widget_show (hbox);

    /* move the xfce-header in the hbox */
    children = gtk_container_get_children (GTK_CONTAINER (dialog_vbox));
    header = g_list_nth_data (children, 1);
    g_object_ref (G_OBJECT (header));
    gtk_container_remove (GTK_CONTAINER (dialog_vbox), header);
    gtk_box_pack_start (GTK_BOX (hbox), header, TRUE, TRUE, 0);
    g_object_unref (G_OBJECT (header));
    g_list_free (children);

    ebox = gtk_event_box_new ();
    gtk_box_pack_start (GTK_BOX (hbox), ebox, FALSE, TRUE, 0);
    g_signal_connect (header, "style-set",
        G_CALLBACK (xfce_settings_manager_dialog_header_style), ebox);
    gtk_widget_show (ebox);

    align = gtk_alignment_new (0.0f, 1.0f, 0.0f, 0.0f);
    gtk_container_add (GTK_CONTAINER (ebox), align);
    gtk_container_set_border_width (GTK_CONTAINER (align), 6);
    gtk_widget_show (align);

    dialog->filter_entry = entry = gtk_entry_new ();
    gtk_container_add (GTK_CONTAINER (align), entry);
    gtk_entry_set_icon_from_stock (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_FIND);
    gtk_entry_set_icon_activatable (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, FALSE);
    g_signal_connect (G_OBJECT (entry), "changed",
        G_CALLBACK (xfce_settings_manager_dialog_entry_changed), dialog);
    g_signal_connect (G_OBJECT (entry), "icon-release",
        G_CALLBACK (xfce_settings_manager_dialog_entry_clear), NULL);
    g_signal_connect (G_OBJECT (entry), "key-press-event",
        G_CALLBACK (xfce_settings_manager_dialog_entry_key_press), dialog);
    gtk_widget_show (entry);

    dialog_vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    dialog->category_scroll = scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), scroll, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (scroll), 6);
    gtk_widget_show (scroll);

    viewport = dialog->category_viewport = gtk_viewport_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scroll), viewport);
    gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
    gtk_widget_show (viewport);

    dialog->category_box = gtk_vbox_new (FALSE, 6);
    gtk_container_add (GTK_CONTAINER (viewport), dialog->category_box);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->category_box), 6);
    gtk_widget_show (dialog->category_box);
    gtk_widget_set_size_request (dialog->category_box,
                                 TEXT_WIDTH   /* text */
                                 + ICON_WIDTH /* icon */
                                 + (5 * 6)    /* borders */, -1);

    /* pluggable dialog scrolled window and viewport */
    dialog->socket_scroll = scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_NONE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), scroll, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (scroll), 0);

    dialog->socket_viewport = viewport = gtk_viewport_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scroll), viewport);
    gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
    gtk_widget_show (viewport);

    xfce_settings_manager_dialog_menu_reload (dialog);

    g_signal_connect_swapped (G_OBJECT (dialog->menu), "reload-required",
        G_CALLBACK (xfce_settings_manager_dialog_menu_reload), dialog);
}



static void
xfce_settings_manager_dialog_finalize (GObject *object)
{
    XfceSettingsManagerDialog *dialog = XFCE_SETTINGS_MANAGER_DIALOG (object);

    g_free (dialog->help_page);
    g_free (dialog->help_component);
    g_free (dialog->help_version);

    g_free (dialog->filter_text);

    if (dialog->socket_item != NULL)
        g_object_unref (G_OBJECT (dialog->socket_item));

    g_object_unref (G_OBJECT (dialog->menu));
    g_object_unref (G_OBJECT (dialog->store));

    G_OBJECT_CLASS (xfce_settings_manager_dialog_parent_class)->finalize (object);
}



static void
xfce_settings_manager_dialog_style_set (GtkWidget *widget,
                                        GtkStyle  *old_style)
{
    XfceSettingsManagerDialog *dialog = XFCE_SETTINGS_MANAGER_DIALOG (widget);

    GTK_WIDGET_CLASS (xfce_settings_manager_dialog_parent_class)->style_set (widget, old_style);

    /* set viewport to color icon view uses for background */
    gtk_widget_modify_bg (dialog->category_viewport,
                          GTK_STATE_NORMAL,
                          &widget->style->base[GTK_STATE_NORMAL]);
}



static void
xfce_settings_manager_dialog_response (GtkDialog *widget,
                                       gint       response_id)
{
    XfceSettingsManagerDialog *dialog = XFCE_SETTINGS_MANAGER_DIALOG (widget);
    const gchar               *help_component;

    if (response_id == GTK_RESPONSE_HELP)
    {
        if (dialog->help_component != NULL)
            help_component = dialog->help_component;
        else
            help_component = "blade-settings";

        xfce_dialog_show_help_with_version (GTK_WINDOW (widget),
                                            help_component,
                                            dialog->help_page,
                                            NULL,
                                            dialog->help_version);
    }
    else
    {
        GdkWindowState state;
        gint           width, height;

        /* Don't save the state for full-screen windows */
        state = gdk_window_get_state (GTK_WIDGET (widget)->window);

        if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0)
        {
            /* Save window size */
            gtk_window_get_size (GTK_WINDOW (widget), &width, &height);
            blconf_channel_set_int (dialog->channel, "/last/window-width", width),
            blconf_channel_set_int (dialog->channel, "/last/window-height", height);
        }

        gtk_widget_destroy (GTK_WIDGET (widget));
        gtk_main_quit ();
    }
}



static void
xfce_settings_manager_dialog_header_style (GtkWidget *header,
                                           GtkStyle  *old_style,
                                           GtkWidget *ebox)
{
    /* use the header background */
    gtk_widget_modify_bg (ebox, GTK_STATE_NORMAL, &header->style->base[GTK_STATE_NORMAL]);
}



static void
xfce_settings_manager_dialog_set_title (XfceSettingsManagerDialog *dialog,
                                        const gchar               *title,
                                        const gchar               *icon_name,
                                        const gchar               *subtitle)
{
    g_return_if_fail (XFCE_IS_SETTINGS_MANAGER_DIALOG (dialog));

    if (icon_name == NULL)
        icon_name = "preferences-desktop";
    if (title == NULL)
        title = _("Settings");
    if (subtitle == NULL)
        subtitle = _("Customize your desktop");

    gtk_window_set_title (GTK_WINDOW (dialog), title);
    xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dialog), subtitle);
    gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);
}



static gint
xfce_settings_manager_dialog_iconview_find (gconstpointer a,
                                            gconstpointer b)
{
    const DialogCategory *category = a;

    return category->iconview == b ? 0 : 1;
}



static gboolean
xfce_settings_manager_dialog_iconview_keynav_failed (BlxoIconView               *current_view,
                                                     GtkDirectionType           direction,
                                                     XfceSettingsManagerDialog *dialog)
{
    GList          *li;
    GtkTreePath    *path;
    BlxoIconView    *new_view;
    gboolean        result = FALSE;
    GtkTreeModel   *model;
    GtkTreeIter     iter;
    gint            col_old, col_new;
    gint            dist_prev, dist_new;
    GtkTreePath    *sel_path;
    DialogCategory *category;

    if (direction == GTK_DIR_UP || direction == GTK_DIR_DOWN)
    {
        /* find this category in the list */
        li = g_list_find_custom (dialog->categories, current_view,
            xfce_settings_manager_dialog_iconview_find);

        /* find the next of previous visible item */
        for (; li != NULL; )
        {
            if (direction == GTK_DIR_DOWN)
                li = g_list_next (li);
            else
                li = g_list_previous (li);

            if (li != NULL)
            {
                category = li->data;
                if (gtk_widget_get_visible (category->box))
                    break;
            }
        }

        /* leave there is no view above or below this one */
        if (li == NULL)
            return FALSE;

        category = li->data;
        new_view = BLXO_ICON_VIEW (category->iconview);

        if (blxo_icon_view_get_cursor (current_view, &path, NULL))
        {
            col_old = blxo_icon_view_get_item_column (current_view, path);
            gtk_tree_path_free (path);

            dist_prev = 1000;
            sel_path = NULL;

            model = blxo_icon_view_get_model (new_view);
            if (gtk_tree_model_get_iter_first (model, &iter))
            {
                do
                {
                     path = gtk_tree_model_get_path (model, &iter);
                     col_new = blxo_icon_view_get_item_column (new_view, path);
                     dist_new = ABS (col_new - col_old);

                     if ((direction == GTK_DIR_UP && dist_new <= dist_prev)
                         || (direction == GTK_DIR_DOWN  && dist_new < dist_prev))
                     {
                         if (sel_path != NULL)
                             gtk_tree_path_free (sel_path);

                         sel_path = path;
                         dist_prev = dist_new;
                     }
                     else
                     {
                         gtk_tree_path_free (path);
                     }
                }
                while (gtk_tree_model_iter_next (model, &iter));
            }

            if (G_LIKELY (sel_path != NULL))
            {
                /* move cursor, grab-focus will handle the selection */
                blxo_icon_view_set_cursor (new_view, sel_path, NULL, FALSE);
                xfce_settings_manager_dialog_scroll_to_item (GTK_WIDGET (new_view), dialog);
                gtk_tree_path_free (sel_path);

                gtk_widget_grab_focus (GTK_WIDGET (new_view));

                result = TRUE;
            }
        }
    }

    return result;
}



static gboolean
xfce_settings_manager_dialog_query_tooltip (GtkWidget                 *iconview,
                                            gint                       x,
                                            gint                       y,
                                            gboolean                   keyboard_mode,
                                            GtkTooltip                *tooltip,
                                            XfceSettingsManagerDialog *dialog)
{
    GtkTreePath    *path;
    GValue          value = { 0, };
    GtkTreeModel   *model;
    GtkTreeIter     iter;
    PojkMenuItem *item;
    const gchar    *comment;

    if (keyboard_mode)
    {
        if (!blxo_icon_view_get_cursor (BLXO_ICON_VIEW (iconview), &path, NULL))
            return FALSE;
    }
    else
    {
        path = blxo_icon_view_get_path_at_pos (BLXO_ICON_VIEW (iconview), x, y);
        if (G_UNLIKELY (path == NULL))
            return FALSE;
    }

    model = blxo_icon_view_get_model (BLXO_ICON_VIEW (iconview));
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gtk_tree_model_get_value (model, &iter, COLUMN_MENU_ITEM, &value);
        item = g_value_get_object (&value);
        g_assert (POJK_IS_MENU_ITEM (item));

        comment = pojk_menu_item_get_comment (item);
        if (!blxo_str_is_empty (comment))
            gtk_tooltip_set_text (tooltip, comment);

        g_value_unset (&value);
    }

    gtk_tree_path_free (path);

    return TRUE;
}



static gboolean
xfce_settings_manager_dialog_iconview_focus (GtkWidget                 *iconview,
                                             GdkEventFocus             *event,
                                             XfceSettingsManagerDialog *dialog)
{
    GtkTreePath *path;

    if (event->in)
    {
        /* a mouse click will have focus, tab events not */
        if (!blxo_icon_view_get_cursor (BLXO_ICON_VIEW (iconview), &path, NULL))
        {
           path = gtk_tree_path_new_from_indices (0, -1);
           blxo_icon_view_set_cursor (BLXO_ICON_VIEW (iconview), path, NULL, FALSE);
           xfce_settings_manager_dialog_scroll_to_item (iconview, dialog);
        }

        blxo_icon_view_select_path (BLXO_ICON_VIEW (iconview), path);
        gtk_tree_path_free (path);
    }
    else
    {
        blxo_icon_view_unselect_all (BLXO_ICON_VIEW (iconview));
    }

    return FALSE;
}



static void
xfce_settings_manager_dialog_go_back (XfceSettingsManagerDialog *dialog)
{
    GtkWidget *socket;

    /* make sure no cursor is shown */
    gdk_window_set_cursor (GTK_WIDGET (dialog)->window, NULL);

    /* reset dialog info */
    xfce_settings_manager_dialog_set_title (dialog, NULL, NULL, NULL);

    gtk_widget_show (dialog->category_scroll);
    gtk_widget_hide (dialog->socket_scroll);

    g_free (dialog->help_page);
    dialog->help_page = NULL;
    g_free (dialog->help_component);
    dialog->help_component = NULL;
    g_free (dialog->help_version);
    dialog->help_version = NULL;

    gtk_widget_set_sensitive (dialog->button_back, FALSE);
    gtk_widget_set_sensitive (dialog->button_help, TRUE);

    gtk_widget_set_sensitive (dialog->filter_entry, TRUE);
    gtk_entry_set_text (GTK_ENTRY (dialog->filter_entry), "");
    gtk_widget_grab_focus (dialog->filter_entry);

    socket = gtk_bin_get_child (GTK_BIN (dialog->socket_viewport));
    if (G_LIKELY (socket != NULL))
        gtk_widget_destroy (socket);

    if (dialog->socket_item != NULL)
    {
        g_object_unref (G_OBJECT (dialog->socket_item));
        dialog->socket_item = NULL;
    }
}



static void
xfce_settings_manager_dialog_entry_changed (GtkWidget                 *entry,
                                            XfceSettingsManagerDialog *dialog)
{
    const gchar    *text;
    gchar          *normalized;
    gchar          *filter_text;
    GList          *li;
    GtkTreeModel   *model;
    gint            n_children;
    DialogCategory *category;

    text = gtk_entry_get_text (GTK_ENTRY (entry));
    if (text == NULL || *text == '\0')
    {
        filter_text = NULL;
    }
    else
    {
        /* create independent search string */
        normalized = g_utf8_normalize (text, -1, G_NORMALIZE_DEFAULT);
        filter_text = g_utf8_casefold (normalized, -1);
        g_free (normalized);
    }

    /* check if we need to update */
    if (g_strcmp0 (dialog->filter_text, filter_text) != 0)
    {
        /* update entry */
        if (dialog->filter_text == NULL || filter_text == NULL)
        {
            gtk_entry_set_icon_from_stock (GTK_ENTRY (dialog->filter_entry),
                GTK_ENTRY_ICON_SECONDARY,
                filter_text == NULL ? GTK_STOCK_FIND : GTK_STOCK_CLEAR);
            gtk_entry_set_icon_activatable (GTK_ENTRY (dialog->filter_entry),
                GTK_ENTRY_ICON_SECONDARY, filter_text != NULL);
        }

        /* set new filter */
        g_free (dialog->filter_text);
        dialog->filter_text = filter_text;

        /* update the category models */
        for (li = dialog->categories; li != NULL; li = li->next)
        {
            category = li->data;

            /* update model filters */
            model = blxo_icon_view_get_model (BLXO_ICON_VIEW (category->iconview));
            gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (model));

            /* set visibility of the category */
            n_children = gtk_tree_model_iter_n_children (model, NULL);
            gtk_widget_set_visible (category->box, n_children > 0);
        }
    }
    else
    {
        g_free (dialog->filter_text);
        dialog->filter_text = NULL;
        g_free (filter_text);
    }
}



static gboolean
xfce_settings_manager_dialog_entry_key_press (GtkWidget                 *entry,
                                              GdkEventKey               *event,
                                              XfceSettingsManagerDialog *dialog)
{
    GList          *li;
    DialogCategory *category;
    GtkTreePath    *path;
    gint            n_visible_items;
    GtkTreeModel   *model;
    const gchar    *text;

    if (event->keyval == GDK_Escape)
    {
        text = gtk_entry_get_text (GTK_ENTRY (entry));
        if (text != NULL && *text != '\0')
        {
            gtk_entry_set_text (GTK_ENTRY (entry), "");
            return TRUE;
        }
    }
    else if (event->keyval == GDK_Return)
    {
        /* count visible children */
        n_visible_items = 0;
        for (li = dialog->categories; li != NULL; li = li->next)
        {
            category = li->data;
            if (gtk_widget_get_visible (category->box))
            {
                model = blxo_icon_view_get_model (BLXO_ICON_VIEW (category->iconview));
                n_visible_items += gtk_tree_model_iter_n_children (model, NULL);

                /* stop searching if there are more then 1 items */
                if (n_visible_items > 1)
                    break;
            }
        }

        for (li = dialog->categories; li != NULL; li = li->next)
        {
            category = li->data;

            /* find the first visible category */
            if (!gtk_widget_get_visible (category->box))
                continue;

            path = gtk_tree_path_new_first ();
            if (n_visible_items == 1)
            {
                /* activate this one item */
                blxo_icon_view_item_activated (BLXO_ICON_VIEW (category->iconview), path);
            }
            else
            {
                /* select first item in view */
                blxo_icon_view_set_cursor (BLXO_ICON_VIEW (category->iconview),
                                          path, NULL, FALSE);
                gtk_widget_grab_focus (category->iconview);
            }
            gtk_tree_path_free (path);
            break;
        }

        return TRUE;
    }

    return FALSE;
}



static void
xfce_settings_manager_dialog_entry_clear (GtkWidget            *entry,
                                          GtkEntryIconPosition  icon_pos,
                                          GdkEvent             *event)
{
    if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
        gtk_entry_set_text (GTK_ENTRY (entry), "");
}



static void
xfce_settings_manager_dialog_plug_added (GtkWidget                 *socket,
                                         XfceSettingsManagerDialog *dialog)
{
    /* set dialog information from desktop file */
    xfce_settings_manager_dialog_set_title (dialog,
        pojk_menu_item_get_name (dialog->socket_item),
        pojk_menu_item_get_icon_name (dialog->socket_item),
        pojk_menu_item_get_comment (dialog->socket_item));

    /* show socket and hide the categories view */
    gtk_widget_show (dialog->socket_scroll);
    gtk_widget_hide (dialog->category_scroll);

    /* button sensitivity */
    gtk_widget_set_sensitive (dialog->button_back, TRUE);
    gtk_widget_set_sensitive (dialog->button_help, dialog->help_page != NULL);
    gtk_widget_set_sensitive (dialog->filter_entry, FALSE);

    /* plug startup complete */
    gdk_window_set_cursor (GTK_WIDGET (dialog)->window, NULL);
}



static void
xfce_settings_manager_dialog_plug_removed (GtkWidget                 *socket,
                                           XfceSettingsManagerDialog *dialog)
{
    /* this shouldn't happen */
    g_critical ("pluggable dialog \"%s\" crashed",
                pojk_menu_item_get_command (dialog->socket_item));

    /* restore dialog */
    xfce_settings_manager_dialog_go_back (dialog);
}



static void
xfce_settings_manager_dialog_spawn (XfceSettingsManagerDialog *dialog,
                                    PojkMenuItem            *item)
{
    const gchar    *command;
    gboolean        snotify;
    GdkScreen      *screen;
    GError         *error = NULL;
    GFile          *desktop_file;
    gchar          *filename;
    XfceRc         *rc;
    gboolean        pluggable = FALSE;
    gchar          *cmd;
    GtkWidget      *socket;
    GdkCursor      *cursor;

    g_return_if_fail (POJK_IS_MENU_ITEM (item));

    screen = gtk_window_get_screen (GTK_WINDOW (dialog));
    command = pojk_menu_item_get_command (item);

    /* we need to read some more info from the desktop
     *  file that is not supported by pojk */
    desktop_file = pojk_menu_item_get_file (item);
    filename = g_file_get_path (desktop_file);
    g_object_unref (desktop_file);

    rc = xfce_rc_simple_open (filename, TRUE);
    g_free (filename);
    if (G_LIKELY (rc != NULL))
    {
        pluggable = xfce_rc_read_bool_entry (rc, "X-XfcePluggable", FALSE);
        if (pluggable)
        {
            dialog->help_page = g_strdup (xfce_rc_read_entry (rc, "X-XfceHelpPage", NULL));
            dialog->help_component = g_strdup (xfce_rc_read_entry (rc, "X-XfceHelpComponent", NULL));
            dialog->help_version = g_strdup (xfce_rc_read_entry (rc, "X-XfceHelpVersion", NULL));
        }

        xfce_rc_close (rc);
    }

    if (pluggable)
    {
        /* fake startup notification */
        cursor = gdk_cursor_new (GDK_WATCH);
        gdk_window_set_cursor (GTK_WIDGET (dialog)->window, cursor);
        gdk_cursor_unref (cursor);

        /* create fresh socket */
        socket = gtk_socket_new ();
        gtk_container_add (GTK_CONTAINER (dialog->socket_viewport), socket);
        g_signal_connect (G_OBJECT (socket), "plug-added",
            G_CALLBACK (xfce_settings_manager_dialog_plug_added), dialog);
        g_signal_connect (G_OBJECT (socket), "plug-removed",
            G_CALLBACK (xfce_settings_manager_dialog_plug_removed), dialog);
        gtk_widget_show (socket);

        /* for info when the plug is attached */
        dialog->socket_item = g_object_ref (item);

        /* spawn dialog with socket argument */
        cmd = g_strdup_printf ("%s --socket-id=%d", command, gtk_socket_get_id (GTK_SOCKET (socket)));
        if (!xfce_spawn_command_line_on_screen (screen, cmd, FALSE, FALSE, &error))
        {
            gdk_window_set_cursor (GTK_WIDGET (dialog)->window, NULL);

            xfce_dialog_show_error (GTK_WINDOW (dialog), error,
                                    _("Unable to start \"%s\""), command);
            g_error_free (error);
        }
        g_free (cmd);
    }
    else
    {
        snotify = pojk_menu_item_supports_startup_notification (item);
        if (!xfce_spawn_command_line_on_screen (screen, command, FALSE, snotify, &error))
        {
            xfce_dialog_show_error (GTK_WINDOW (dialog), error,
                                    _("Unable to start \"%s\""), command);
            g_error_free (error);
        }
    }
}



static void
xfce_settings_manager_dialog_item_activated (BlxoIconView               *iconview,
                                             GtkTreePath               *path,
                                             XfceSettingsManagerDialog *dialog)
{
    GtkTreeModel   *model;
    GtkTreeIter     iter;
    PojkMenuItem *item;

    model = blxo_icon_view_get_model (iconview);
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gtk_tree_model_get (model, &iter, COLUMN_MENU_ITEM, &item, -1);
        g_assert (POJK_IS_MENU_ITEM (item));

        xfce_settings_manager_dialog_spawn (dialog, item);

        g_object_unref (G_OBJECT (item));
    }
}



static gboolean
xfce_settings_manager_dialog_filter_category (GtkTreeModel *model,
                                              GtkTreeIter  *iter,
                                              gpointer      data)
{
    GValue          cat_val = { 0, };
    GValue          filter_val = { 0, };
    gboolean        visible;
    DialogCategory *category = data;
    const gchar    *filter_text;

    /* filter only the active category */
    gtk_tree_model_get_value (model, iter, COLUMN_MENU_DIRECTORY, &cat_val);
    visible = g_value_get_object (&cat_val) == G_OBJECT (category->directory);
    g_value_unset (&cat_val);

    /* filter search string */
    if (visible && category->dialog->filter_text != NULL)
    {
        gtk_tree_model_get_value (model, iter, COLUMN_FILTER_TEXT, &filter_val);
        filter_text = g_value_get_string (&filter_val);
        visible = strstr (filter_text, category->dialog->filter_text) != NULL;
        g_value_unset (&filter_val);
    }

    return visible;
}



static void
xfce_settings_manager_dialog_scroll_to_item (GtkWidget                 *iconview,
                                             XfceSettingsManagerDialog *dialog)
{
    GtkAllocation *alloc;
    GtkTreePath   *path;
    gint           row, row_height;
    gdouble        rows;
    GtkAdjustment *adjustment;
    gdouble        lower, upper;

    if (blxo_icon_view_get_cursor (BLXO_ICON_VIEW (iconview), &path, NULL))
    {
        /* get item row */
        row = blxo_icon_view_get_item_row (BLXO_ICON_VIEW (iconview), path);
        gtk_tree_path_free (path);

        /* estinated row height */
        alloc = &iconview->allocation;
        rows = alloc->height / 56;
        row_height = alloc->height / MAX (1.0, (gint) rows);

        /* selected item boundries */
        lower = alloc->y + row_height * row;
        upper = alloc->y + row_height * (row + 1);

        /* scroll so item is visible */
        adjustment = gtk_viewport_get_vadjustment (GTK_VIEWPORT (dialog->category_viewport));
        gtk_adjustment_clamp_page (adjustment, lower, upper);
    }
}



static gboolean
xfce_settings_manager_dialog_key_press_event (GtkWidget                 *iconview,
                                              GdkEventKey               *event,
                                              XfceSettingsManagerDialog *dialog)
{
    gboolean result;

    /* let blxo handle the selection first */
    result = GTK_WIDGET_CLASS (G_OBJECT_GET_CLASS (iconview))->key_press_event (iconview, event);

    /* make sure the selected item is visible */
    if (result)
        xfce_settings_manager_dialog_scroll_to_item (iconview, dialog);

    return result;
}



static gboolean
xfce_settings_manager_start_search (GtkWidget                 *iconview,
                                    XfceSettingsManagerDialog *dialog)
{
    gtk_widget_grab_focus (dialog->filter_entry);
    return TRUE;
}



static void
xfce_settings_manager_dialog_category_free (gpointer data)
{
    DialogCategory            *category = data;
    XfceSettingsManagerDialog *dialog = category->dialog;

    dialog->categories = g_list_remove (dialog->categories, category);

    g_object_unref (G_OBJECT (category->directory));
    g_slice_free (DialogCategory, category);
}



static void
xfce_settings_manager_dialog_add_category (XfceSettingsManagerDialog *dialog,
                                           PojkMenuDirectory       *directory)
{
    GtkTreeModel    *filter;
    GtkWidget       *alignment;
    GtkWidget       *iconview;
    GtkWidget       *label;
    GtkWidget       *separator;
    GtkWidget       *vbox;
    PangoAttrList   *attrs;
    GtkCellRenderer *render;
    DialogCategory  *category;

    category = g_slice_new0 (DialogCategory);
    category->directory = (PojkMenuDirectory *) g_object_ref (G_OBJECT (directory));
    category->dialog = dialog;

    /* filter category from main store */
    filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (dialog->store), NULL);
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
        xfce_settings_manager_dialog_filter_category,
        category, xfce_settings_manager_dialog_category_free);

    category->box = vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog->category_box), vbox, FALSE, TRUE, 0);
    gtk_widget_show (vbox);

    /* create a label for the category title */
    label = gtk_label_new (pojk_menu_directory_get_name (directory));
    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes (GTK_LABEL (label), attrs);
    pango_attr_list_unref (attrs);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
    gtk_widget_show (label);

    /* separate title and content using a horizontal line */
    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 0);
    gtk_widget_show (separator);

    /* use an alignment to separate the category content from the title */
    alignment = gtk_alignment_new (0.0f, 0.0f, 1.0f, 1.0f);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 0, 0);
    gtk_container_add (GTK_CONTAINER (vbox), alignment);
    gtk_widget_show (alignment);

    category->iconview = iconview = blxo_icon_view_new_with_model (GTK_TREE_MODEL (filter));
    gtk_container_add (GTK_CONTAINER (alignment), iconview);
    blxo_icon_view_set_orientation (BLXO_ICON_VIEW (iconview), GTK_ORIENTATION_HORIZONTAL);
    blxo_icon_view_set_margin (BLXO_ICON_VIEW (iconview), 0);
    blxo_icon_view_set_single_click (BLXO_ICON_VIEW (iconview), TRUE);
    blxo_icon_view_set_enable_search (BLXO_ICON_VIEW (iconview), FALSE);
    blxo_icon_view_set_item_width (BLXO_ICON_VIEW (iconview), TEXT_WIDTH + ICON_WIDTH);
    gtk_widget_show (iconview);

    /* list used for unselecting */
    dialog->categories = g_list_append (dialog->categories, category);

    gtk_widget_set_has_tooltip (iconview, TRUE);
    g_signal_connect (G_OBJECT (iconview), "query-tooltip",
        G_CALLBACK (xfce_settings_manager_dialog_query_tooltip), dialog);
    g_signal_connect (G_OBJECT (iconview), "focus-in-event",
        G_CALLBACK (xfce_settings_manager_dialog_iconview_focus), dialog);
    g_signal_connect (G_OBJECT (iconview), "focus-out-event",
        G_CALLBACK (xfce_settings_manager_dialog_iconview_focus), dialog);
    g_signal_connect (G_OBJECT (iconview), "keynav-failed",
        G_CALLBACK (xfce_settings_manager_dialog_iconview_keynav_failed), dialog);
    g_signal_connect (G_OBJECT (iconview), "item-activated",
        G_CALLBACK (xfce_settings_manager_dialog_item_activated), dialog);
    g_signal_connect (G_OBJECT (iconview), "key-press-event",
        G_CALLBACK (xfce_settings_manager_dialog_key_press_event), dialog);
    g_signal_connect (G_OBJECT (iconview), "start-interactive-search",
        G_CALLBACK (xfce_settings_manager_start_search), dialog);

    render = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (iconview), render, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (iconview), render, "icon-name", COLUMN_ICON_NAME);
    g_object_set (G_OBJECT (render),
                  "stock-size", GTK_ICON_SIZE_DIALOG,
                  "follow-state", TRUE,
                  NULL);

    render = xfce_text_renderer_new ();
    gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (iconview), render, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (iconview), render, "text", COLUMN_NAME);
    g_object_set (G_OBJECT (render),
                  "wrap-mode", PANGO_WRAP_WORD,
                  "wrap-width", TEXT_WIDTH,
                  "follow-prelit", TRUE,
                  "follow-state", TRUE,
                  NULL);

    g_object_unref (G_OBJECT (filter));
}



static void
xfce_settings_manager_dialog_menu_collect (PojkMenu  *menu,
                                           GList      **items)
{
    GList *elements, *li;

    g_return_if_fail (POJK_IS_MENU (menu));

    elements = pojk_menu_get_elements (menu);

    for (li = elements; li != NULL; li = li->next)
    {
        if (POJK_IS_MENU_ITEM (li->data))
        {
            /* only add visible items */
            if (pojk_menu_element_get_visible (li->data))
                *items = g_list_prepend (*items, li->data);
        }
        else if (POJK_IS_MENU (li->data))
        {
            /* we collect only 1 level deep in a category, so
             * add the submenu items too (should never happen tho) */
            xfce_settings_manager_dialog_menu_collect (li->data, items);
        }
    }

    g_list_free (elements);
}



static gint
xfce_settings_manager_dialog_menu_sort (gconstpointer a,
                                        gconstpointer b)
{
    return g_utf8_collate (pojk_menu_item_get_name (POJK_MENU_ITEM (a)),
                           pojk_menu_item_get_name (POJK_MENU_ITEM (b)));
}



static void
xfce_settings_manager_dialog_menu_reload (XfceSettingsManagerDialog *dialog)
{
    GError              *error = NULL;
    GList               *elements, *li;
    GList               *lnext;
    PojkMenuDirectory *directory;
    GList               *items, *lp;
    gint                 i = 0;
    gchar               *item_text;
    gchar               *normalized;
    gchar               *filter_text;
    DialogCategory      *category;

    g_return_if_fail (XFCE_IS_SETTINGS_MANAGER_DIALOG (dialog));
    g_return_if_fail (POJK_IS_MENU (dialog->menu));

    if (dialog->categories != NULL)
    {
        for (li = dialog->categories; li != NULL; li = lnext)
        {
            lnext = li->next;
            category = li->data;

            gtk_widget_destroy (category->box);
        }

        g_assert (dialog->categories == NULL);

        gtk_list_store_clear (GTK_LIST_STORE (dialog->store));
    }

    if (pojk_menu_load (dialog->menu, NULL, &error))
    {
        /* get all menu elements (preserve layout) */
        elements = pojk_menu_get_elements (dialog->menu);
        for (li = elements; li != NULL; li = li->next)
        {
            /* only accept toplevel menus */
            if (!POJK_IS_MENU (li->data))
                continue;

            directory = pojk_menu_get_directory (li->data);
            if (G_UNLIKELY (directory == NULL))
                continue;

            items = NULL;

            xfce_settings_manager_dialog_menu_collect (li->data, &items);

            /* add the new category if it has visible items */
            if (G_LIKELY (items != NULL))
            {
                /* insert new items in main store */
                items = g_list_sort (items, xfce_settings_manager_dialog_menu_sort);
                for (lp = items; lp != NULL; lp = lp->next)
                {
                    /* create independent search string */
                    item_text = g_strdup_printf ("%s\n%s",
                        pojk_menu_item_get_name (lp->data),
                        pojk_menu_item_get_comment (lp->data));
                    normalized = g_utf8_normalize (item_text, -1, G_NORMALIZE_DEFAULT);
                    g_free (item_text);
                    filter_text = g_utf8_casefold (normalized, -1);
                    g_free (normalized);

                    gtk_list_store_insert_with_values (dialog->store, NULL, i++,
                        COLUMN_NAME, pojk_menu_item_get_name (lp->data),
                        COLUMN_ICON_NAME, pojk_menu_item_get_icon_name (lp->data),
                        COLUMN_TOOLTIP, pojk_menu_item_get_comment (lp->data),
                        COLUMN_MENU_ITEM, lp->data,
                        COLUMN_MENU_DIRECTORY, directory,
                        COLUMN_FILTER_TEXT, filter_text, -1);

                    g_free (filter_text);
                }
                g_list_free (items);

                /* add the new category to the box */
                xfce_settings_manager_dialog_add_category (dialog, directory);
            }
        }

        g_list_free (elements);
    }
    else
    {
        g_critical ("Failed to load menu: %s", error->message);
        g_error_free (error);
    }
}


GtkWidget *
xfce_settings_manager_dialog_new (void)
{
    return g_object_new (XFCE_TYPE_SETTINGS_MANAGER_DIALOG, NULL);
}



gboolean
xfce_settings_manager_dialog_show_dialog (XfceSettingsManagerDialog *dialog,
                                          const gchar               *dialog_name)
{
    GtkTreeModel   *model = GTK_TREE_MODEL (dialog->store);
    GtkTreeIter     iter;
    PojkMenuItem *item;
    const gchar    *desktop_id;
    gchar          *name;
    gboolean        found = FALSE;

    g_return_val_if_fail (XFCE_IS_SETTINGS_MANAGER_DIALOG (dialog), FALSE);

    name = g_strdup_printf ("%s.desktop", dialog_name);

    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        do
        {
             gtk_tree_model_get (model, &iter, COLUMN_MENU_ITEM, &item, -1);
             g_assert (POJK_IS_MENU_ITEM (item));

             desktop_id = pojk_menu_item_get_desktop_id (item);
             if (g_strcmp0 (desktop_id, name) == 0)
             {
                  xfce_settings_manager_dialog_spawn (dialog, item);
                  found = TRUE;
             }

             g_object_unref (G_OBJECT (item));
        }
        while (!found && gtk_tree_model_iter_next (model, &iter));
    }

    g_free (name);

    return found;
}
