/*  File: zmapView.h
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) Sanger Institute, 2004
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
 * originated by
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk
 *
 * Description: Interface for controlling a single "view", a view
 *              comprises one or more windowsw which display data
 *              collected from one or more servers. Hence the view
 *              interface controls both windows and connections to
 *              servers.
 *              
 * HISTORY:
 * Last edited: Feb 17 15:04 2006 (rds)
 * Created: Thu May 13 14:59:14 2004 (edgrif)
 * CVS info:   $Id: zmapView.h,v 1.23 2006-02-17 17:53:05 rds Exp $
 *-------------------------------------------------------------------
 */
#ifndef ZMAPVIEW_H
#define ZMAPVIEW_H

#include <gtk/gtk.h>
#include <ZMap/zmapWindow.h>


/* Opaque type, represents an instance of a ZMapView. */
typedef struct _ZMapViewStruct *ZMapView ;


/* Opaque type, represents an instance of a ZMapView window. */
typedef struct _ZMapViewWindowStruct *ZMapViewWindow ;


/* Callers can specify callback functions which get called with ZMapView which made the call
 * and the applications own data pointer. If the callback is to made for a window event
 * then the ZMapViewWindow where the event took place will be returned as the first
 * parameter. If the callback is for an event that involves the whole view (e.g. destroy)
 * then the ZMapView where the event took place is returned. */
typedef void (*ZMapViewWindowCallbackFunc)(ZMapViewWindow view_window, void *app_data, void *view_data) ;
typedef void (*ZMapViewCallbackFunc)(ZMapView zmap_view, void *app_data, void *view_data) ;


/* Set of callback routines that allow the caller to be notified when events happen
 * to a window. */
typedef struct _ZMapViewCallbacksStruct
{
  ZMapViewWindowCallbackFunc enter ;
  ZMapViewWindowCallbackFunc leave ;
  ZMapViewCallbackFunc load_data ;
  ZMapViewWindowCallbackFunc focus ;
  ZMapViewWindowCallbackFunc select ;
  ZMapViewWindowCallbackFunc visibility_change ;
  ZMapViewCallbackFunc destroy ;
} ZMapViewCallbacksStruct, *ZMapViewCallbacks ;




/* DOES THIS STATE NEED TO BE EXPOSED LIKE THIS...REVISIT.... */
/* The overall state of the zmapView, we need this because both the zmap window and the its threads
 * will die asynchronously so we need to block further operations while they are in this state. */


/* AGH muckiness here....if last window is deleted, what state are we in ??? */

typedef enum {
  ZMAPVIEW_INIT,					    /* No window(s) and no threads. */
  ZMAPVIEW_NOT_CONNECTED,				    /* Window(s), but no threads. */

  /* AGH, I think I don't like this....can happen if all view windows get closed.... */
  ZMAPVIEW_NO_WINDOW,					    /* Threads but no windows. */

  ZMAPVIEW_CONNECTING,					    /* Connecting threads. */
  ZMAPVIEW_CONNECTED,					    /* Threads but no data. */

  ZMAPVIEW_LOADING,					    /* Loading data. */
  ZMAPVIEW_LOADED,					    /* Full view. */

  ZMAPVIEW_RESETTING,					    /* Returning to ZMAPVIEW_NOT_CONNECTED. */

  ZMAPVIEW_DYING					    /* View is dying for some reason,
							       cannot do anything in this state. */
} ZMapViewState ;




#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* I THINK WE MAY NEED SOMETHING MORE LIKE THIS.... */
typedef enum {
  ZMAPVIEW_INIT,					    /* No window(s) and no threads. */

  ZMAPVIEW_NOT_CONNECTED,				    /* Window(s) with no threads. */
  ZMAPVIEW_CONNECTING,

  ZMAPVIEW_WAITING,					    /* Window(s) + thread(s) in normal
							       state. */
  ZMAPVIEW_DOING_REQUEST,


  ZMAPVIEW_RESETTING,					    /* Window(s) that is closing its threads
							       and returning to INIT state. */



  ZMAPVIEW_DYING,					    /* ZMap is dying for some reason,
							       cannot do anything in this state. */
  ZMAPVIEW_DEAD						    /* DON'T KNOW IF WE NEED THIS.... */
} ZMapViewState ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



void zMapViewInit(ZMapViewCallbacks callbacks) ;
ZMapView zMapViewCreate(char *sequence,	int start, int end, void *app_data) ;

ZMapViewWindow zMapViewMakeWindow(ZMapView zmap_view, GtkWidget *parent_widget) ;
ZMapViewWindow zMapViewCopyWindow(ZMapView zmap_view, GtkWidget *parent_widget,
				  ZMapWindow copy_window, ZMapWindowLockType window_locking) ;
void zMapViewRedraw(ZMapViewWindow view_window) ;
void zMapViewRemoveWindow(ZMapViewWindow view_window, gboolean *no_windows_left) ;
gboolean zMapViewConnect(ZMapView zmap_view, char *config_str) ;

gboolean zMapViewLoad (ZMapView zmap_view) ;
gboolean zMapViewReset(ZMapView zmap_view) ;
gboolean zMapViewReverseComplement(ZMapView zmap_view) ;
ZMapStrand zMapViewGetRevCompStatus(ZMapView zmap_view) ;
void zMapViewZoom(ZMapView zmap_view, ZMapViewWindow view_window, double zoom) ;
char *zMapViewGetSequence(ZMapView zmap_view) ;
ZMapFeatureContext zMapViewGetFeatures(ZMapView zmap_view) ;
void zMapViewGetVisible(ZMapViewWindow view_window, double *top, double *bottom) ;
ZMapViewState zMapViewGetStatus(ZMapView zmap_view) ;
char *zMapViewGetStatusStr(ZMapViewState zmap_state) ;
ZMapWindow zMapViewGetWindow(ZMapViewWindow view_window) ;
ZMapView zMapViewGetView(ZMapViewWindow view_window) ;
GList *zMapViewGetWindowList(ZMapViewWindow view_window);
void   zMapViewSetWindowList(ZMapViewWindow view_window, GList *list);

void zmapViewFeatureDump(ZMapViewWindow view_window, char *file) ;

gboolean zMapViewDestroy(ZMapView zmap_view) ;




#endif /* !ZMAPVIEW_H */
