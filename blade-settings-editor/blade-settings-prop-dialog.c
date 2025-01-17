/*
 *  blade-settings-editor
 *
 *  Copyright (c) 2012      Nick Schermer <nick@xfce.org>
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
#include <dbus/dbus-glib.h>

#include <libbladeutil/libbladeutil.h>
#include <libbladeui/libbladeui.h>
#include <blconf/blconf.h>

#include "blade-settings-prop-dialog.h"



struct _XfceSettingsPropDialogClass
{
    GtkDialogClass __parent__;
};

struct _XfceSettingsPropDialog
{
    GtkDialog __parent__;

    BlconfChannel *channel;

    GValue         prop_value;

    GtkWidget     *prop_name;
    GtkWidget     *prop_type;
    GtkWidget     *prop_string;
    GtkWidget     *prop_integer;
    GtkWidget     *prop_bool;
};

typedef struct
{
    const gchar *name;
    GType        type;
}
ValueTypes;



static void     xfce_settings_prop_dialog_finalize             (GObject                   *object);
static void     xfce_settings_prop_dialog_response             (GtkDialog                 *widget,
                                                                gint                       response_id);
static void     xfce_settings_prop_dialog_visible_bind         (GtkWidget                 *widget,
                                                                GtkWidget                 *label);
static void     xfce_settings_prop_dialog_entry_validate       (GtkWidget                 *entry,
                                                                XfceSettingsPropDialog    *dialog);
static void     xfce_settings_prop_dialog_button_toggled       (GtkWidget                 *button);
static void     xfce_settings_prop_dialog_type_changed         (GtkWidget                 *combo,
                                                                XfceSettingsPropDialog    *dialog);



G_DEFINE_TYPE (XfceSettingsPropDialog, xfce_settings_prop_dialog, GTK_TYPE_DIALOG)



static ValueTypes value_types[] =
{
  { N_("Empty"), G_TYPE_NONE },
  { N_("String"), G_TYPE_STRING },
  { N_("Boolean"), G_TYPE_BOOLEAN },
  { N_("Int"), G_TYPE_INT },
  { N_("Double"), G_TYPE_DOUBLE },
  { N_("Unsigned Int"), G_TYPE_UINT },
  { N_("Int64"), G_TYPE_INT64 },
  { N_("Unsigned Int64"), G_TYPE_UINT64 }
};



enum
{
  COLUMN_NAME,
  COLUMN_ID,
  N_COLUMNS
};



static void
xfce_settings_prop_dialog_class_init (XfceSettingsPropDialogClass *klass)
{
    GObjectClass   *gobject_class;
    GtkDialogClass *gtkdialog_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_settings_prop_dialog_finalize;

    gtkdialog_class = GTK_DIALOG_CLASS (klass);
    gtkdialog_class->response = xfce_settings_prop_dialog_response;
}



static void
xfce_settings_prop_dialog_init (XfceSettingsPropDialog *dialog)
{
    GtkWidget       *table;
    GtkWidget       *content_area;
    GtkWidget       *label;
    GtkWidget       *entry;
    GtkWidget       *combo;
    GtkWidget       *spin;
    GtkWidget       *toggle;
    GtkListStore    *store;
    guint            i;
    GtkCellRenderer *render;
    GtkWidget       *save_button;

    gtk_window_set_title (GTK_WINDOW (dialog), _("New Property"));
    gtk_window_set_default_size (GTK_WINDOW (dialog), 300, 200);
    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_SAVE, GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    save_button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
    gtk_widget_set_sensitive (save_button, FALSE);

    table = gtk_table_new (5, 2, FALSE);
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_box_pack_start (GTK_BOX (content_area), table, TRUE, TRUE, 0);
    gtk_table_set_col_spacings (GTK_TABLE (table), 12);
    gtk_container_set_border_width (GTK_CONTAINER (table), 6);
    gtk_widget_show (table);

    label = gtk_label_new_with_mnemonic (_("_Property:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.00, 0.50);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                      GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_set_row_spacing (GTK_TABLE (table), 0, 6);
    gtk_widget_show (label);

    dialog->prop_name = entry = gtk_entry_new ();
    gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 0, 1,
                      GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
    xfce_settings_prop_dialog_visible_bind (entry, label);
    g_signal_connect (G_OBJECT (entry), "changed",
        G_CALLBACK (xfce_settings_prop_dialog_entry_validate), dialog);
    gtk_widget_show (entry);

    label = gtk_label_new_with_mnemonic (_("_Type:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.00, 0.50);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                      GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show (label);

    store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_UINT);
    for (i = 0; i < G_N_ELEMENTS (value_types); i++)
    {
        gtk_list_store_insert_with_values (store, NULL, i,
                                           COLUMN_NAME, _(value_types[i].name),
                                           COLUMN_ID, i, -1);
    }

    dialog->prop_type = combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
    gtk_table_attach (GTK_TABLE (table), combo, 1, 2, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
    gtk_table_set_row_spacing (GTK_TABLE (table), 1, 6);
    xfce_settings_prop_dialog_visible_bind (combo, label);
    gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
    g_signal_connect (G_OBJECT (combo), "changed",
        G_CALLBACK (xfce_settings_prop_dialog_type_changed), dialog);
    gtk_widget_show (combo);
    g_object_unref (G_OBJECT (store));

    render = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), render, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), render,
                                   "text", COLUMN_NAME);

    /* strings */
    label = gtk_label_new_with_mnemonic (_("_Value:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.00, 0.50);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
                      GTK_FILL, GTK_FILL, 0, 0);

    entry = dialog->prop_string = gtk_entry_new ();
    gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 2, 3,
                      GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
    xfce_settings_prop_dialog_visible_bind (entry, label);

    /* integers */
    label = gtk_label_new_with_mnemonic (_("_Value:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.00, 0.50);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
                      GTK_FILL, GTK_FILL, 0, 0);

    spin = dialog->prop_integer = gtk_spin_button_new_with_range (0.00, 0.00, 1.00);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin), TRUE);
    gtk_table_attach (GTK_TABLE (table), spin, 1, 2, 3, 4,
                      GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
    xfce_settings_prop_dialog_visible_bind (spin, label);

    /* bool */
    label = gtk_label_new_with_mnemonic (_("_Value:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.00, 0.50);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, 4, 5,
                      GTK_FILL, GTK_FILL, 0, 0);

    toggle = dialog->prop_bool = gtk_toggle_button_new_with_label ("FALSE");
    g_signal_connect (G_OBJECT (toggle), "toggled",
        G_CALLBACK (xfce_settings_prop_dialog_button_toggled), NULL);
    gtk_table_attach (GTK_TABLE (table), toggle, 1, 2, 4, 5,
                      GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
    xfce_settings_prop_dialog_visible_bind (toggle, label);
}



static void
xfce_settings_prop_dialog_finalize (GObject *object)
{
    XfceSettingsPropDialog *dialog = XFCE_SETTINGS_PROP_DIALOG (object);

    if (dialog->channel != NULL)
        g_object_unref (G_OBJECT (dialog->channel));

    if (G_IS_VALUE (&dialog->prop_value))
        g_value_unset (&dialog->prop_value);

    G_OBJECT_CLASS (xfce_settings_prop_dialog_parent_class)->finalize (object);
}



static void
xfce_settings_prop_dialog_response (GtkDialog *widget,
                                    gint       response_id)
{
    XfceSettingsPropDialog *dialog = XFCE_SETTINGS_PROP_DIALOG (widget);
    const gchar            *property;
    ValueTypes             *value_type;
    GValue                  value = { 0, };
    gdouble                 spin_value;
    gint                    active;

    g_return_if_fail (BLCONF_IS_CHANNEL (dialog->channel));

    if (response_id == GTK_RESPONSE_OK)
    {
        property = gtk_entry_get_text (GTK_ENTRY (dialog->prop_name));

        active = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->prop_type));
        g_assert (active >= 0 && active < (gint) G_N_ELEMENTS (value_types));
        value_type = &value_types[active];

        spin_value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (dialog->prop_integer));

        switch (value_type->type)
        {
            case G_TYPE_NONE:
                return;

            case G_TYPE_STRING:
                g_value_init (&value, G_TYPE_STRING);
                g_value_set_static_string (&value,
                    gtk_entry_get_text (GTK_ENTRY (dialog->prop_string)));
                break;

            case G_TYPE_BOOLEAN:
                g_value_init (&value, G_TYPE_BOOLEAN);
                g_value_set_boolean (&value,
                    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->prop_bool)));
                break;

            case G_TYPE_INT:
                g_value_init (&value, G_TYPE_INT);
                g_value_set_int (&value, spin_value);
                break;

            case G_TYPE_DOUBLE:
                g_value_init (&value, G_TYPE_DOUBLE);
                g_value_set_double (&value, spin_value);
                break;

            case G_TYPE_UINT:
                g_value_init (&value, G_TYPE_UINT);
                g_value_set_uint (&value, spin_value);
                break;

            case G_TYPE_INT64:
                g_value_init (&value, G_TYPE_INT64);
                g_value_set_int64 (&value, spin_value);
                break;

            case G_TYPE_UINT64:
                g_value_init (&value, G_TYPE_UINT64);
                g_value_set_uint64 (&value, spin_value);
                break;
        }

        if (G_IS_VALUE (&value))
        {
            blconf_channel_set_property (dialog->channel, property, &value);
            g_value_unset (&value);
        }
    }
}



static void
xfce_settings_prop_dialog_visible_changed (GtkWidget  *widget,
                                           GParamSpec *pspec,
                                           GtkWidget  *label)
{
    g_return_if_fail (GTK_IS_WIDGET (widget));
    g_return_if_fail (GTK_IS_LABEL (label));

    gtk_widget_set_visible (label, gtk_widget_get_visible (widget));
}



static void
xfce_settings_prop_dialog_sensitive_changed (GtkWidget  *widget,
                                             GParamSpec *pspec,
                                             GtkWidget  *label)
{
    g_return_if_fail (GTK_IS_WIDGET (widget));
    g_return_if_fail (GTK_IS_LABEL (label));

    gtk_widget_set_sensitive (label, gtk_widget_get_sensitive (widget));
}



static void
xfce_settings_prop_dialog_visible_bind (GtkWidget *widget,
                                        GtkWidget *label)
{
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
    g_signal_connect (G_OBJECT (widget), "notify::visible",
        G_CALLBACK (xfce_settings_prop_dialog_visible_changed), label);
    g_signal_connect (G_OBJECT (widget), "notify::sensitive",
        G_CALLBACK (xfce_settings_prop_dialog_sensitive_changed), label);
}



/* Copied from blconfd/blconf-backend.c */
static gboolean
blconf_property_is_valid (const gchar  *property,
                          GError      **error)
{
    const gchar *p = property;

    if (!p || *p != '/')
    {
        if (error != NULL)
        {
            g_set_error (error, BLCONF_ERROR, BLCONF_ERROR_INVALID_PROPERTY,
                         _("Property names must start with a '/' character"));
        }
        return FALSE;
    }

    p++;
    if (!*p)
    {
        if (error != NULL)
        {
            g_set_error (error, BLCONF_ERROR, BLCONF_ERROR_INVALID_PROPERTY,
                         _("The root element ('/') is not a valid property name"));
        }
        return FALSE;
    }

    while (*p)
    {
        if (!(*p >= 'A' && *p <= 'Z') && !(*p >= 'a' && *p <= 'z')
            && !(*p >= '0' && *p <= '9')
            && *p != '_' && *p != '-' && *p != '/' && *p != '{' && *p != '}'
            && !(*p == '<' || *p == '>') && *p != '|' && *p != ','
            && *p != '[' && *p != ']' && *p != '.' && *p != ':')
        {
            if (error != NULL)
            {
                g_set_error (error, BLCONF_ERROR,
                             BLCONF_ERROR_INVALID_PROPERTY,
                             _("Property names can only include the ASCII "
                               "characters A-Z, a-z, 0-9, '_', '-', ':', '.', "
                               "',', '[', ']', '{', '}', '<' and '>', as well "
                               "as '/' as a separator"));
            }
            return FALSE;
        }

        if ('/' == *p && '/' == *(p - 1))
        {
            if (error != NULL)
            {
                g_set_error (error, BLCONF_ERROR,
                             BLCONF_ERROR_INVALID_PROPERTY,
                             _("Property names cannot have two or more "
                               "consecutive '/' characters"));
            }
            return FALSE;
        }

        p++;
    }

    if (*(p - 1) == '/')
    {
        if (error != NULL)
        {
            g_set_error (error, BLCONF_ERROR, BLCONF_ERROR_INVALID_PROPERTY,
                         _("Property names cannot end with a '/' character"));
        }

        return FALSE;
    }

    return TRUE;
}



static void
xfce_settings_prop_dialog_entry_validate (GtkWidget              *entry,
                                          XfceSettingsPropDialog *dialog)
{
    GtkWidget   *save_button;
    const gchar *text;
    gboolean     is_valid = FALSE;
    GError      *error = NULL;

    text = gtk_entry_get_text (GTK_ENTRY (entry));

    if (text != NULL && *text != '\0')
    {
        is_valid = blconf_property_is_valid (text, &error);

        gtk_entry_set_icon_from_stock (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY,
                                       is_valid ? NULL : GTK_STOCK_DIALOG_ERROR);
        gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY,
                                         is_valid ? NULL : error->message);

        if (error != NULL)
            g_error_free (error);
    }
    else
    {
        gtk_entry_set_icon_from_stock (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, NULL);
    }

    save_button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
    gtk_widget_set_sensitive (save_button, is_valid);
}



static void
xfce_settings_prop_dialog_button_toggled (GtkWidget *button)
{
    const gchar *label;

    label = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)) ? "TRUE" : "FALSE";
    gtk_button_set_label (GTK_BUTTON (button), label);
}



static void
xfce_settings_prop_dialog_type_changed (GtkWidget              *combo,
                                        XfceSettingsPropDialog *dialog)
{
    gint          active;
    ValueTypes   *value_type;
    const GValue *value = &dialog->prop_value;

    gtk_widget_hide (dialog->prop_string);
    gtk_widget_hide (dialog->prop_integer);
    gtk_widget_hide (dialog->prop_bool);

    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (dialog->prop_integer), 0);
    gtk_entry_set_text (GTK_ENTRY (dialog->prop_string), "");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->prop_integer), 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->prop_bool), FALSE);

    active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
    if (active < 0 || active >= (gint) G_N_ELEMENTS (value_types))
        return;

    value_type = &value_types[active];

    switch (value_type->type)
    {
        case G_TYPE_NONE:
            gtk_widget_grab_focus (dialog->prop_type);
            return;

        case G_TYPE_STRING:
            gtk_widget_show (dialog->prop_string);
            gtk_widget_grab_focus (dialog->prop_string);

            if (G_VALUE_HOLDS_STRING (value))
            {
                gtk_entry_set_text (GTK_ENTRY (dialog->prop_string),
                                    g_value_get_string (value));
            }
            break;

        case G_TYPE_BOOLEAN:
            gtk_widget_show (dialog->prop_bool);
            gtk_widget_grab_focus (dialog->prop_bool);

            if (G_VALUE_HOLDS_BOOLEAN (value))
            {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->prop_bool),
                                              g_value_get_boolean (value));
            }
            break;

        case G_TYPE_INT:
            gtk_widget_show (dialog->prop_integer);
            gtk_widget_grab_focus (dialog->prop_integer);
            gtk_spin_button_set_range (GTK_SPIN_BUTTON (dialog->prop_integer),
                                       G_MININT, G_MAXINT);

            if (G_VALUE_HOLDS_INT (value))
            {
                gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->prop_integer),
                                           g_value_get_int (value));
            }
            break;

        case G_TYPE_DOUBLE:
            gtk_widget_show (dialog->prop_integer);
            gtk_widget_grab_focus (dialog->prop_integer);
            gtk_spin_button_set_digits (GTK_SPIN_BUTTON (dialog->prop_integer), 4);
            gtk_spin_button_set_range (GTK_SPIN_BUTTON (dialog->prop_integer),
                                       G_MINDOUBLE, G_MAXDOUBLE);

            if (G_VALUE_HOLDS_DOUBLE (value))
            {
                gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->prop_integer),
                                           g_value_get_double (value));
            }
            break;

        case G_TYPE_UINT:
            gtk_widget_show (dialog->prop_integer);
            gtk_widget_grab_focus (dialog->prop_integer);
            gtk_spin_button_set_range (GTK_SPIN_BUTTON (dialog->prop_integer),
                                       0, G_MAXUINT);

            if (G_VALUE_HOLDS_UINT (value))
            {
                gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->prop_integer),
                                           g_value_get_uint (value));
            }
            break;

        case G_TYPE_INT64:
            gtk_widget_show (dialog->prop_integer);
            gtk_widget_grab_focus (dialog->prop_integer);
            gtk_spin_button_set_range (GTK_SPIN_BUTTON (dialog->prop_integer),
                                       G_MININT64, G_MAXINT64);

            if (G_VALUE_HOLDS_INT64 (value))
            {
                gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->prop_integer),
                                           g_value_get_int64 (value));
            }
            break;

        case G_TYPE_UINT64:
            gtk_widget_show (dialog->prop_integer);
            gtk_widget_grab_focus (dialog->prop_integer);
            gtk_spin_button_set_range (GTK_SPIN_BUTTON (dialog->prop_integer),
                                       0, G_MAXUINT64);

            if (G_VALUE_HOLDS_UINT64 (value))
            {
                gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->prop_integer),
                                           g_value_get_uint64 (value));
            }
            break;
    }
}



static void
xfce_settings_prop_dialog_type_set_active (XfceSettingsPropDialog *dialog,
                                           GType                   value_type)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (value_types); i++)
    {
        if (value_types[i].type == value_type)
        {
            gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->prop_type), i);
            break;
        }
    }
}



GtkWidget *
xfce_settings_prop_dialog_new (GtkWindow     *parent,
                               BlconfChannel *channel,
                               const gchar   *property)
{
    XfceSettingsPropDialog *dialog;

    g_return_val_if_fail (BLCONF_IS_CHANNEL (channel), NULL);
    g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);

    dialog = g_object_new (XFCE_TYPE_SETTINGS_PROP_DIALOG, NULL);

    dialog->channel = (BlconfChannel *) g_object_ref (G_OBJECT (channel));

    if (property != NULL)
    {
        gtk_entry_set_text (GTK_ENTRY (dialog->prop_name), property);
        gtk_editable_set_editable (GTK_EDITABLE (dialog->prop_name), FALSE);
        gtk_window_set_title (GTK_WINDOW (dialog), _("Edit Property"));

        if (blconf_channel_get_property (channel, property, &dialog->prop_value))
        {
            xfce_settings_prop_dialog_type_set_active (dialog,
                G_VALUE_TYPE (&dialog->prop_value));
            gtk_widget_set_sensitive (dialog->prop_type, FALSE);
        }
    }

    /* set the transient parent (if any) */
    if (G_LIKELY (parent != NULL))
    {
        gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
        gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
    }

    return GTK_WIDGET (dialog);
}



void
xfce_settings_prop_dialog_set_parent_property (XfceSettingsPropDialog *dialog,
                                               const gchar            *property)
{
    gchar *p;
    gint   length = -1;
    gint   pos = 0;

    g_return_if_fail (XFCE_IS_SETTINGS_PROP_DIALOG (dialog));

    if (property != NULL && *property == '/')
    {
        p = strrchr (property, '/');
        if (G_LIKELY (p != NULL))
          length = (p - property) + 1;

        gtk_editable_insert_text (GTK_EDITABLE (dialog->prop_name),
                                  property, length, &pos);
    }
}
