/*  File: zmapWindow.h
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
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk and
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk
 *
 * Description: Defines interface to code that creates/handles a
 *              window displaying genome data.
 *              
 * HISTORY:
 * Last edited: Feb 21 11:21 2006 (rds)
 * Created: Thu Jul 24 15:21:56 2003 (edgrif)
 * CVS info:   $Id: zmapWindow.h,v 1.50 2006-02-21 18:40:02 rds Exp $
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_WINDOW_H
#define ZMAP_WINDOW_H

#include <glib.h>


/* I think the canvas and features headers should not be in here...they need to be removed
 * in time.... */

/* SHOULD CANVAS BE HERE...MAYBE, MAYBE NOT...... */
#include <libfoocanvas/libfoocanvas.h>

#include <ZMap/zmapFeature.h>


/*! @addtogroup zmapwindow
 * @{
 *  */


/* Opaque type, represents an individual ZMap window. */
typedef struct _ZMapWindowStruct *ZMapWindow ;



/* indicates how far the zmap is zoomed, n.b. ZMAP_ZOOM_FIXED implies that the whole sequence
 * is displayed at the maximum zoom. */
typedef enum {ZMAP_ZOOM_INIT, ZMAP_ZOOM_MIN, ZMAP_ZOOM_MID, ZMAP_ZOOM_MAX,
	      ZMAP_ZOOM_FIXED} ZMapWindowZoomStatus ;

/* Should the original window and the new window be locked together for scrolling and zooming.
 * vertical means that the vertical scrollbars should be locked together, specifying vertical
 * or horizontal means locking of zoom as well. */
typedef enum {ZMAP_WINLOCK_NONE, ZMAP_WINLOCK_VERTICAL, ZMAP_WINLOCK_HORIZONTAL} ZMapWindowLockType ;


/* Data returned to the visibilityChange callback routine. */
typedef struct
{
  ZMapWindowZoomStatus zoom_status ;

  /* Top/bottom coords for section of sequence that can be scrolled currently in window. */
  double scrollable_top ;
  double scrollable_bot ;
} ZMapWindowVisibilityChangeStruct, *ZMapWindowVisibilityChange ;


/* Data returned to the focus callback routine. */
typedef struct
{
  char *text ;						    /* Describes selected item. */

  FooCanvasItem *item ;					    /* The feature selected, may be null
							       if a column was selected. */

} ZMapWindowSelectStruct, *ZMapWindowSelect ;


/* Callback functions that can be registered with ZMapWindow, functions are registered all in one.
 * go via the ZMapWindowCallbacksStruct. */
typedef void (*ZMapWindowCallbackFunc)(ZMapWindow window, void *caller_data, void *window_data) ;

typedef struct _ZMapWindowCallbacksStruct
{
  ZMapWindowCallbackFunc enter ;
  ZMapWindowCallbackFunc leave ;
  ZMapWindowCallbackFunc scroll ;
  ZMapWindowCallbackFunc focus ;
  ZMapWindowCallbackFunc select ;
  ZMapWindowCallbackFunc setZoomStatus;
  ZMapWindowCallbackFunc visibilityChange ;
  ZMapWindowCallbackFunc destroy ;
} ZMapWindowCallbacksStruct, *ZMapWindowCallbacks ;


typedef enum
  {
    ZMAP_FTOI_QUERY_INVALID = 0,
    ZMAP_FTOI_QUERY_ALIGN_ITEM, /* Get the align group item */
    ZMAP_FTOI_QUERY_BLOCK_ITEM, /* Get the block group item */
    ZMAP_FTOI_QUERY_SET_ITEM,   /* Get the column group item (strand sensitive) */
    ZMAP_FTOI_QUERY_FEATURE_ITEM,

    ZMAP_FTOI_QUERY_ALIGN_LIST, /* Get the align group item */
    ZMAP_FTOI_QUERY_BLOCK_LIST, /* Get the block group item */
    ZMAP_FTOI_QUERY_SET_LIST,   /* Get the column group item (strand sensitive) */
    ZMAP_FTOI_QUERY_FEATURE_LIST,

    ZMAP_FTOI_QUERY_FEATURE_REGEXP
  } ZMapFToIQueryType;

typedef enum
  {
    ZMAP_FTOI_RETURN_ERROR = 0,
    ZMAP_FTOI_RETURN_LIST,
    ZMAP_FTOI_RETURN_ITEM
  } ZMapFToIReturnType;

typedef struct _ZMapWindowFToIQueryStruct
{
  GQuark alignId,               /* alignment string as quark */
    blockId,                    /* block string as id */
    originalId,                 /* feature original id */
    suId,                       /* session unique id */
    columnId,                   /* column id e.g. columnGeneNEvidence */
    styleId;                    /* style id e.g. curated This should
                                 * be a valid style name i.e. created
                                 * by zMapStyleCreateID(style_name); */

  int start, end;               /* [Target] start and end */
  int query_start, query_end;   /* Query start and end (optional ZMAPFEATURE_HOMOL)*/

  ZMapFeatureType type;         /* Feature type */
  ZMapStrand strand;            /* Feature strand */

  ZMapFToIQueryType  query_type;
  ZMapFToIReturnType return_type;

  union
  {
    GList         *list_answer;
    FooCanvasItem *item_answer;
  } ans;                        /* The answer */

} ZMapWindowFToIQueryStruct, *ZMapWindowFToIQuery;


/*! @} end of zmapwindow docs. */



void zMapWindowInit(ZMapWindowCallbacks callbacks) ;
ZMapWindow zMapWindowCreate(GtkWidget *parent_widget, char *sequence, void *app_data) ;
ZMapWindow zMapWindowCopy(GtkWidget *parent_widget, char *sequence, 
			  void *app_data, ZMapWindow old,
			  ZMapFeatureContext features, ZMapWindowLockType window_locking) ;
void zMapWindowDisplayData(ZMapWindow window,
			   ZMapFeatureContext current_features, ZMapFeatureContext new_features) ;
void zMapWindowZoom(ZMapWindow window, double zoom_factor) ;
void zMapWindowMove(ZMapWindow window, double start, double end) ;
void zMapWindowReset(ZMapWindow window) ;
void zMapWindowRedraw(ZMapWindow window) ;
void zMapWindowFeatureRedraw(ZMapWindow window, ZMapFeatureContext feature_context,
			     gboolean reversed) ;
GtkWidget *zMapWindowGetWidget(ZMapWindow window);
ZMapWindowZoomStatus zMapWindowGetZoomStatus(ZMapWindow window) ;
double zMapWindowGetZoomFactor(ZMapWindow window);
double zMapWindowGetZoomMax(ZMapWindow window) ;
double zMapWindowGetZoomMagnification(ZMapWindow window);
gboolean zMapWindowIsLocked(ZMapWindow window) ;

/* Remove this to use Ed's version */
PangoFont *zMapWindowGetFixedWidthFont(ZMapWindow window);
PangoFontDescription *zMapWindowGetFixedWidthFontDescription(ZMapWindow window);



void zMapWindowGetVisible(ZMapWindow window, double *top_out, double *bottom_out) ;

ZMapWindowFToIQuery zMapWindowFToINewQuery(void);
gboolean zMapWindowFToIFetchByQuery(ZMapWindow window, ZMapWindowFToIQuery query);
void zMapWindowFToIDestroyQuery(ZMapWindowFToIQuery query);

FooCanvasItem *zMapWindowFindFeatureItemByItem(ZMapWindow window, FooCanvasItem *item) ;
GList *zMapWindowFeatureAllStyles(ZMapWindow window);

/* Add, modify, draw, remove features from the canvas. */
FooCanvasItem *zMapWindowFeatureAdd(ZMapWindow window,
			      FooCanvasGroup *feature_group, ZMapFeature feature) ;
FooCanvasItem *zMapWindowFeatureReplace(ZMapWindow zmap_window,
				 FooCanvasItem *curr_feature_item, ZMapFeature new_feature) ;
gboolean zMapWindowFeatureRemove(ZMapWindow zmap_window, FooCanvasItem *feature_item) ;


void zMapWindowScrollToWindowPos(ZMapWindow window, int window_y_pos) ;

gboolean zMapWindowScrollToItem(ZMapWindow window, FooCanvasItem *feature_item) ;

void zMapWindowHighlightObject(ZMapWindow window, FooCanvasItem *feature) ;


void zMapWindowDestroyLists(ZMapWindow window) ;

void zMapWindowUnlock(ZMapWindow window) ;

void zMapWindowDestroy(ZMapWindow window) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* TEST SCAFFOLDING............... */
ZMapFeatureContext testGetGFF(void) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


#endif /* !ZMAP_WINDOW_H */
