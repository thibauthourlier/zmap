/*  File: zmapappmanage.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) Sanger Institute, 2003
 *-------------------------------------------------------------------
 * ZMap is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * or see the on-line version at http://www.gnu.org/copyleft/gpl.txt
 *-------------------------------------------------------------------
 * This file is part of the ZMap genome database package
 * and was written by
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk,
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk and
 *	Simon Kelley (Sanger Institute, UK) srk@sanger.ac.uk
 *
 * Description: 
 * Exported functions: See XXXXXXXXXXXXX.h
 * HISTORY:
 * Last edited: Nov 13 12:29 2003 (edgrif)
 * Created: Thu Jul 24 14:36:47 2003 (edgrif)
 * CVS info:   $Id: zmapAppmanage.c,v 1.1 2003-11-13 14:58:39 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <stdlib.h>
#include <stdio.h>
#include <zmapApp_P.h>

static void loadThreadCB(GtkWidget *widget, gpointer cb_data) ;
static void killThreadCB(GtkWidget *widget, gpointer data) ;
static void checkThreadCB(GtkWidget *widget, gpointer cb_data) ;
static void selectRow(GtkCList *clist, gint row, gint column, GdkEventButton *event,
		      gpointer cb_data) ;
static void unselectRow(GtkCList *clist, gint row, gint column, GdkEventButton *event,
			gpointer cb_data) ;


static char *column_titles[ZMAP_NUM_COLS] = {"Window", "Status", "Last Request"} ;



GtkWidget *zmapMainMakeManage(ZMapAppContext app_context)
{
  GtkWidget *frame ;
  GtkWidget *vbox, *scrwin, *clist, *hbox, *load_button, *kill_button, *check_button ;

  frame = gtk_frame_new("Manage Connections") ;
  gtk_frame_set_label_align( GTK_FRAME( frame ), 0.0, 0.0 ) ;
  gtk_container_border_width(GTK_CONTAINER(frame), 5) ;
  
  vbox = gtk_vbox_new(FALSE, 0) ;
  gtk_container_border_width(GTK_CONTAINER(vbox), 5);
  gtk_container_add(GTK_CONTAINER (frame), vbox);

  scrwin = gtk_scrolled_window_new(NULL, NULL) ;
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC) ;
  gtk_widget_set_usize(scrwin, -2, 150) ;		    /* -2  =>  leave value unchanged. */
  gtk_box_pack_start(GTK_BOX(vbox), scrwin, FALSE, FALSE, 0) ;

  app_context->clist_widg = clist = gtk_clist_new_with_titles(3, column_titles) ;
  gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		     GTK_SIGNAL_FUNC(selectRow), (gpointer)app_context) ;
  gtk_signal_connect(GTK_OBJECT(clist), "unselect_row",
		     GTK_SIGNAL_FUNC(unselectRow), (gpointer)app_context) ;
  gtk_container_add (GTK_CONTAINER (scrwin), clist);


  hbox = gtk_hbox_new(FALSE, 0) ;
  gtk_container_border_width(GTK_CONTAINER(hbox), 5);
  gtk_container_add(GTK_CONTAINER(vbox), hbox);

  load_button = gtk_button_new_with_label("Load Thread Data") ;
  gtk_signal_connect(GTK_OBJECT(load_button), "clicked",
		     GTK_SIGNAL_FUNC(loadThreadCB), (gpointer)app_context) ;
  gtk_box_pack_start(GTK_BOX(hbox), load_button, FALSE, FALSE, 0) ;
  GTK_WIDGET_SET_FLAGS(load_button, GTK_CAN_DEFAULT) ;
  gtk_widget_grab_default(load_button) ;

  kill_button = gtk_button_new_with_label("Kill Thread") ;
  gtk_signal_connect(GTK_OBJECT(kill_button), "clicked",
		     GTK_SIGNAL_FUNC(killThreadCB), (gpointer)app_context) ;
  gtk_box_pack_start(GTK_BOX(hbox), kill_button, FALSE, FALSE, 0) ;
  GTK_WIDGET_SET_FLAGS(kill_button, GTK_CAN_DEFAULT) ;
  gtk_widget_grab_default(kill_button) ;

  check_button = gtk_button_new_with_label("Check Connections") ;
  gtk_signal_connect(GTK_OBJECT(check_button), "clicked",
		     GTK_SIGNAL_FUNC(checkThreadCB), (gpointer)app_context) ;
  gtk_box_pack_start(GTK_BOX(hbox), check_button, FALSE, FALSE, 0) ;
  GTK_WIDGET_SET_FLAGS(check_button, GTK_CAN_DEFAULT) ;
  gtk_widget_grab_default(check_button) ;

  return frame ;
}




static void loadThreadCB(GtkWidget *widget, gpointer cb_data)
{
  ZMapAppContext app_context = (ZMapAppContext)cb_data ;
  int row ;

  if (app_context->selected_zmap)
    {
      zMapManagerLoadData(app_context->selected_zmap) ;
    }


  return ;
}

static void killThreadCB(GtkWidget *widget, gpointer cb_data)
{
  ZMapAppContext app_context = (ZMapAppContext)cb_data ;
  int row ;

  if (app_context->selected_zmap)
    {
      row = gtk_clist_find_row_from_data(GTK_CLIST(app_context->clist_widg),
					 app_context->selected_zmap) ;

      ZMAP_DEBUG(("GUI: kill thread for row %d with connection pointer: %x\n",
		      row, app_context->selected_zmap)) ;

      zMapManagerKill(app_context->zmap_manager, app_context->selected_zmap) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      /* this all done by a callback now as we need to do this from several places and
       * at the right time. */

      app_context->selected_zmap = NULL ;

      /* The remove call actually sets my data which I attached to the row to NULL
       * which is v. naughty, so I reset my data in the widget to NULL to avoid it being
       * messed with. */
      gtk_clist_set_row_data(GTK_CLIST(app_context->clist_widg), row, NULL) ;
      gtk_clist_remove(GTK_CLIST(app_context->clist_widg), row) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

    }


  return ;
}

/* this function was for testing really and needs to go or be updated..... */
static void checkThreadCB(GtkWidget *widget, gpointer cb_data)
{
  ZMapAppContext app_context = (ZMapAppContext)cb_data ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE

  /* REPLACE WITH A CALL TO DISPLAY STATE OF CONNECTIONS.....was for testing only anyway. */
  ZMapCheckConnections(app_context->connections,
		       zmapSignalData, app_context,
		       zmapCleanUpConnection, app_context) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return ;
}

static void selectRow(GtkCList *clist, gint row, gint column, GdkEventButton *event,
		      gpointer cb_data)
{
  ZMapAppContext app_context = (ZMapAppContext)cb_data ;

  app_context->selected_zmap = (ZMapWinConn)gtk_clist_get_row_data(clist, row) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  ZMAP_DEBUG(("GUI: select row %d with connection pointer: %x\n", row, connection)) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return ;
}

static void unselectRow(GtkCList *clist, gint row, gint column, GdkEventButton *event,
			gpointer cb_data)
{
  ZMapAppContext app_context = (ZMapAppContext)cb_data ;

  app_context->selected_zmap = NULL ;

  ZMAP_DEBUG(("GUI: unselect row %d\n", row)) ;

  return ;
}
