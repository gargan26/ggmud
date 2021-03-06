/*  GGMud
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Logger
 * Copyright (C) 1999 Doc.Day aka Fredrik Andersson.
 *
 * This file contains the most functions for the Logger but there is code in
 * window.c, ansi.c. The call to this function is made by clicking on the
 * Tools/Logger menu item, when you want to stop Lgging just click the menu
 * item again to close the Log file. The file is closed on exit as well.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include "config.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "support.h"
#include "ggmud.h"
/* ToolBar Loggerbutton */

/*
 * This function will open the file mud->log_filename for
 * writing.
 */
void on_btnOverwrite_clicked (GtkWidget *btn, gpointer data)
{
    /* Put code here for file writing */
    if ((mud->LOG_FILE = fopen(mud->log_filename, "w")) == NULL) {
         popup_window(ERR, "Can't open file %s for writing:\n\n %s", mud->log_filename, strerror (errno));
    }
    else
        mud->LOGGING = TRUE;

    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                lookup_widget(mud->window, "menuitem_logger")), mud->LOGGING);

    gtk_widget_destroy(gtk_widget_get_toplevel(btn));
}

/*
 * This function will open the file mud->log_filename for
 * appending.
 */
void on_btnAppend_clicked (GtkWidget *btn, gpointer data)
{
    /* Put code here for file writing */
    if ((mud->LOG_FILE = fopen(mud->log_filename, "a")) == NULL) {
         popup_window(ERR, "Can't open file %s for appending:\n\n %s", mud->log_filename, strerror (errno));
    }
    else
        mud->LOGGING = TRUE;

    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                lookup_widget(mud->window, "menuitem_logger")), mud->LOGGING);

    gtk_widget_destroy(gtk_widget_get_toplevel(btn));
}

void append_dialog (const gchar *filename)
{
    GtkWidget *window;
    GtkWidget *hbuttonbox;
    GtkWidget *btnAppend;
    GtkWidget *btnOverwrite;
    GtkWidget *btnCancel;
    GtkWidget *box1;
    GtkWidget *label;
    GtkWidget *separator;

    gchar text[1024];
    gchar dialog_text[1024];
    gchar file_text[1024];

    sprintf (file_text, "The following file already exists:\n%s\n\n", mud->log_filename);
    sprintf (text, "You can either Overwrite the file, erasing any contents it may had,\n"
                   "or Append to the file, keeping whatever is there, and adding to the end of it,\n"
                   "or cancel the operation.");
    sprintf (dialog_text, "%s%s", file_text, text);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "File already exists");
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    gtk_window_set_policy (GTK_WINDOW (window), FALSE, FALSE, FALSE);

    box1 = gtk_vbox_new(FALSE, 0);
    gtk_container_add (GTK_CONTAINER (window), box1);
    gtk_widget_show(box1);

    label = gtk_label_new (dialog_text);
    gtk_box_pack_start (GTK_BOX(box1), label, TRUE, TRUE, 0);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
    gtk_widget_show(label);

    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 10);
    gtk_widget_show(separator);

    hbuttonbox = gtk_hbutton_box_new ();
    gtk_box_pack_end (GTK_BOX (box1), hbuttonbox, FALSE, TRUE, 0);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox), GTK_BUTTONBOX_END);
    gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox), 10);
    gtk_button_box_set_child_size (GTK_BUTTON_BOX (hbuttonbox), 75, 20);
    gtk_widget_show (hbuttonbox);

    btnOverwrite = gtk_button_new_with_label ("Overwrite");
    gtk_signal_connect_object (GTK_OBJECT(btnOverwrite), "clicked",
                        GTK_SIGNAL_FUNC(on_btnOverwrite_clicked),
                        NULL);
    gtk_container_add (GTK_CONTAINER (hbuttonbox), btnOverwrite);
    gtk_widget_show (btnOverwrite);

    btnAppend = gtk_button_new_from_stock (GTK_STOCK_ADD);
    gtk_signal_connect (GTK_OBJECT(btnAppend), "clicked",
                        GTK_SIGNAL_FUNC(on_btnAppend_clicked),
                        NULL);
    gtk_container_add (GTK_CONTAINER (hbuttonbox), btnAppend);
    gtk_widget_show (btnAppend);

    btnCancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
    gtk_signal_connect_object (GTK_OBJECT(btnCancel), "clicked",
                        GTK_SIGNAL_FUNC(gtk_widget_destroy),
                        GTK_OBJECT (window));
    gtk_container_add (GTK_CONTAINER (hbuttonbox), btnCancel);
    gtk_widget_show (btnCancel);

    gtk_widget_show (window);
    gtk_grab_add (window);
}


void do_log ()
{
    GtkWidget *filew;
    const gchar *home;
    gchar path[256];

    /* To Toggle the menu item */

    if (mud->LOGGING) {
        fclose (mud->LOG_FILE);
        mud->LOGGING = FALSE;
        popup_window(INFO, "The log file '%s' is closed.", mud->log_filename);
        return;
    }

    home = g_get_home_dir();
    if (home == NULL) {
        strcpy (path, "untitled.log");
    } else {
        strcpy (path, home);
        strcat (path, "/untitled.log");
    }

    /* Create a new file selection widget */
    filew = gtk_file_chooser_dialog_new ("Save Log file as...", 
                                  GTK_WINDOW(mud->window), 
                                  GTK_FILE_CHOOSER_ACTION_SAVE,
                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                  GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                  NULL);

    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER(filew), 
				     path);

    gtk_widget_show(filew);

    if ( gtk_dialog_run(GTK_DIALOG(filew)) == GTK_RESPONSE_ACCEPT) {
        FILE *f;

        if (mud->log_filename)
            g_free(mud->log_filename);

        mud->log_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filew));
        
        if ((f = fopen (mud->log_filename, "r"))) {
            fclose(f);
            append_dialog(mud->log_filename);
        } else {
            if ((mud->LOG_FILE = fopen(mud->log_filename, "w")) == NULL) {
                popup_window (ERR, "Can't open file %s for writing:\n\n %s",
                        mud->log_filename, strerror (errno));
            }
            else mud->LOGGING = TRUE;
        }
    }

    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                lookup_widget(mud->window, "menuitem_logger")), mud->LOGGING);

    gtk_widget_destroy(filew);
}
