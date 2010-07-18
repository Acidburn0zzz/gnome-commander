/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#ifndef __GNOME_CMD_FILE_SELECTOR_H__
#define __GNOME_CMD_FILE_SELECTOR_H__

#define GNOME_CMD_FILE_SELECTOR(obj)          GTK_CHECK_CAST (obj, gnome_cmd_file_selector_get_type (), GnomeCmdFileSelector)
#define GNOME_CMD_FILE_SELECTOR_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gnome_cmd_file_selector_get_type (), GnomeCmdFileSelectorClass)
#define GNOME_CMD_IS_FILE_SELECTOR(obj)       GTK_CHECK_TYPE (obj, gnome_cmd_file_selector_get_type ())

struct GnomeCmdMainWin;

#include "gnome-cmd-file-list.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-notebook.h"
#include "gnome-cmd-xml-config.h"


typedef enum
{
    LEFT,
    RIGHT,
    ACTIVE,
    INACTIVE
} FileSelectorID;


struct GnomeCmdCombo;


struct GnomeCmdFileSelector
{
    GtkVBox vbox;

    GtkWidget *con_btns_hbox;
    GtkWidget *con_hbox;
    GtkWidget *dir_indicator;
    GtkWidget *dir_label;
    GtkWidget *info_label;
    GnomeCmdCombo *con_combo;
    GtkWidget *vol_label;

    GnomeCmdNotebook *notebook;
    GnomeCmdFileList *list;

  public:

    class Private;

    Private *priv;

    operator GObject * () const             {  return G_OBJECT (this);    }
    operator GtkWidget * () const           {  return GTK_WIDGET (this);  }
    operator GtkBox * () const              {  return GTK_BOX (this);     }

    GnomeCmdFileList *file_list() const     {  return list;               }
    GnomeCmdFileList *file_list(gint n) const;

    GnomeCmdDir *get_directory() const      {  return list->cwd;          }
    void goto_directory(const gchar *dir)   {  return list->goto_directory(dir);  }

    void first();
    void back();
    void forward();
    void last();

    gboolean can_back();
    gboolean can_forward();

    void set_active(gboolean value);

    GnomeCmdCon *get_connection() const     {  return file_list()->con;   }
    void set_connection(GnomeCmdCon *con, GnomeCmdDir *start_dir=NULL);

    gboolean is_local() const               {  return gnome_cmd_con_is_local (get_connection ());  }
    gboolean is_active();

    GtkWidget *new_tab();
    GtkWidget *new_tab(GnomeCmdDir *dir, gboolean activate=TRUE);
    GtkWidget *new_tab(GnomeCmdDir *dir, GnomeCmdFileList::ColumnID sort_col, GtkSortType sort_order, gboolean activate=TRUE);
    void close_tab()                        {  if (notebook->size()>1)  notebook->remove_page();   }
    void close_tab(gint n)                  {  if (notebook->size()>1)  notebook->remove_page(n);  }

    void show_filter();
    void update_files();
    void update_direntry();
    void update_vol_label();
    void update_selected_files_label();
    void update_style();
    void update_connections();
    void update_conbuttons_visibility();
    void update_concombo_visibility();

    gboolean key_pressed(GdkEventKey *event);

    friend XML::xstream &operator << (XML::xstream &xml, GnomeCmdFileSelector &fs);
};

inline GnomeCmdFileList *GnomeCmdFileSelector::file_list(gint n) const
{
    return (GnomeCmdFileList *) gtk_bin_get_child (GTK_BIN (notebook->page(n)));
}

inline void GnomeCmdFileSelector::set_connection(GnomeCmdCon *con, GnomeCmdDir *start_dir)
{
    file_list()->set_connection(con, start_dir);
}

inline GtkWidget *GnomeCmdFileSelector::new_tab()
{
    return new_tab(NULL, GnomeCmdFileList::COLUMN_NAME, GTK_SORT_ASCENDING, TRUE);
}

inline GtkWidget *GnomeCmdFileSelector::new_tab(GnomeCmdDir *dir, gboolean activate)
{
    return new_tab(dir, file_list()->get_sort_column(), file_list()->get_sort_order(), activate);
}

GtkType gnome_cmd_file_selector_get_type ();
GtkWidget *gnome_cmd_file_selector_new ();

void gnome_cmd_file_selector_show_new_textfile_dialog (GnomeCmdFileSelector *fs);

void gnome_cmd_file_selector_cap_paste (GnomeCmdFileSelector *fs);

void gnome_cmd_file_selector_create_symlink (GnomeCmdFileSelector *fs, GnomeCmdFile *f);
void gnome_cmd_file_selector_create_symlinks (GnomeCmdFileSelector *fs, GList *files);

inline FileSelectorID operator ! (FileSelectorID id)
{
    switch (id)
    {
        case LEFT:      return RIGHT;
        case RIGHT:     return LEFT;
        case INACTIVE:  return ACTIVE;
        case ACTIVE:    return INACTIVE;

        default:        return id;
    }
}

#endif // __GNOME_CMD_FILE_SELECTOR_H__
