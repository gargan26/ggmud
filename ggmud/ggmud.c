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

#include <gtk/gtk.h>
#include "ggmud.h"
#include "tintin.h"
#include "include/ticks.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>

/* Initialize the list of keys */
gchar *keys[] = {"F1", "F2", "F3", "F4", "F5",
	         "F6", "F7", "F8", "F9", "F10", "F11", "F12", NULL};

void
quitmsg(char *s)
{
    fputs(s,stderr);
    
    exit(0);
}

int timetilltick(void)
{
  int ttt;

  ttt = (time(0) - time0) % tick_size;
  ttt = (tick_size - ttt) % tick_size;
  return(ttt);
}

int checktick(void)
{
    extern int show_pretick;
    extern struct session *activesession;
    static int last = -1, ttt = -1; /* ttt = time to tick */
    int now, found = 0;
    char buffer[20];

    if(time0 <= 0)
        return(100);	/* big number */

    now = time(0);

    if(last > 0) {
        while(last <= now) {
            ttt = (++last - time0) % tick_size;
            ttt = (tick_size - ttt) % tick_size;
            if(!ttt || ttt == 10)
                if(activesession && activesession->tickstatus)
                {
                    if (!ttt)
                        tintin_puts("#TICK!!!", activesession);
                    else if (show_pretick)
                        tintin_puts("#10 SECONDS TO TICK!!!", activesession);
                    /*	    tintin_puts(!ttt ? "#TICK!!!" : "#10 SECONDS TO TICK!!!", s); */
                }
        }
    } else {
        last = now + 1;
        ttt = (now - time0) % tick_size;
        ttt = (tick_size - ttt) % tick_size;
    }

    sprintf(buffer, "%d",ttt);

    // aggiornamento del campo coi secondi con ttt
    if(activesession)
        gtk_label_set_text(mud->tick_counter, buffer);

    return TRUE;
}

int main(int argc, char **argv)
{
    extern int checktick(void);

    gtk_set_locale ();
    gtk_init (&argc, &argv);
    gdk_init (&argc, &argv);
    
    mud=g_malloc(sizeof(ggmud));
    memset(mud, 0, sizeof(ggmud));

    mud->hist=g_malloc(sizeof(struct ggmud_history));
    mud->hist->size=0;
    mud->hist->max=20;
    mud->hist->cur=0;
    mud->hist->pos=0;
    mud->hist->cyclic=1;
    mud->hist->list=g_malloc(sizeof(gpointer)*(mud->hist->max+1));
    memset(mud->hist->list, 0, sizeof(gpointer)*(mud->hist->max+1));
    mud->disp_font_name="fixed";
    mud->lines=0;
    mud->maxlines = 300 * 70;	// This will be an option


    /* load the stuff that needs to be loaded before the GUI comes up! */
    load_misc_prefs();

    /* Spawn the Graphical User Interface */
    load_macro();
    mud->window = spawn_gui();
    init_colors();
    load_font();
    load_wizard();
    load_prefs();
    load_aliases();
    load_triggers();

    hist_add(""); /* Needed to get rid of a blank line in history list */

    mud->curr_color=prefs.DefaultColor;


    gtk_widget_show(mud->window);

    g_timeout_add(500, checktick, NULL);

    gdk_window_set_background(mud->text->text_area,
			      &(prefs.BackgroundColor));

    ttmain(argc, argv);


    gtk_main();
	
    return 0;
}			

#ifdef WIN32

int _stdcall
WinMain (int hInstance, int hPrevInstance, char *lpszCmdLine, int nCmdShow)
{
  return main (__argc, __argv);
}

#endif
