/*  File: zmapWindowDraw.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006: Genome Research Ltd.
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
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *
 * Description: General drawing functions for zmap window, e.g.
 *              repositioning columns after one has been bumped
 *              or removed etc.
 *
 * Exported functions: See zmapWindow_P.h
 * HISTORY:
 * Last edited: May  9 11:57 2008 (rds)
 * Created: Thu Sep  8 10:34:49 2005 (edgrif)
 * CVS info:   $Id: zmapWindowDraw.c,v 1.95 2008-05-09 10:59:34 rds Exp $
 *-------------------------------------------------------------------
 */

#include <math.h>
#include <glib.h>
#include <ZMap/zmapUtils.h>
#include <ZMap/zmapGLibUtils.h>
#include <zmapWindow_P.h>
#include <zmapWindowContainer.h>



/* For straight forward bumping. */
typedef struct
{
  ZMapWindow window ;
  ZMapWindowItemFeatureSetData set_data ;

  GHashTable *pos_hash ;
  GList *pos_list ;

  double offset ;
  double incr ;

  int start, end ;

  gboolean bump_all ;

  ZMapFeatureTypeStyle bumped_style ;
} BumpColStruct, *BumpCol ;


typedef struct
{
  double y1, y2 ;
  double offset ;
  double incr ;
} BumpColRangeStruct, *BumpColRange ;



/* THERE IS A BUG IN THE CODE THAT USES THIS...IT MOVES FEATURES OUTSIDE OF THE BACKGROUND SO
 * IT ALL LOOKS NAFF, PROBABLY WE GET AWAY WITH IT BECAUSE COLS ARE SO WIDELY SPACED OTHERWISE
 * THEY MIGHT OVERLAP. I THINK THE MOVING CODE MAY BE AT FAULT. */
/* For complex bump users seem to want columns to overlap a bit, if this is set to 1.0 there is
 * no overlap, if you set it to 0.5 they will overlap by a half and so on. */
#define COMPLEX_BUMP_COMPRESS 1.0

typedef struct
{
  ZMapWindow window ;
  GList **gaps_added_items ;
} AddGapsDataStruct, *AddGapsData ;

typedef gboolean (*OverLapListFunc)(GList *curr_features, GList *new_features) ;

typedef GList* (*GListTraverseFunc)(GList *list) ;

typedef struct
{
  ZMapWindow window ;
  ZMapFeatureTypeStyle bumped_style ;
  gboolean bump_all ;
  GHashTable *name_hash ;
  GList *bumpcol_list ;
  double curr_offset ;
  double incr ;
  OverLapListFunc overlap_func ;
  gboolean protein ;
} ComplexBumpStruct, *ComplexBump ;


typedef struct
{
  ZMapWindow window ;
  double incr ;
  double offset ;
  GList *feature_list ;
} ComplexColStruct, *ComplexCol ;


typedef struct
{
  GList **name_list ;
  OverLapListFunc overlap_func ;
} FeatureDataStruct, *FeatureData ;

typedef struct
{
  gboolean overlap ;
  int start, end ;
  int feature_diff ;
  ZMapFeature feature ;
  double width ;
} RangeDataStruct, *RangeData ;



typedef struct
{
  ZMapWindow window ;
  double zoom ;

} ZoomDataStruct, *ZoomData ;


typedef struct execOnChildrenStruct_
{
  ZMapContainerLevelType stop ;

  GFunc                  down_func_cb ;
  gpointer               down_func_data ;

  GFunc                  up_func_cb ;
  gpointer               up_func_data ;

} execOnChildrenStruct, *execOnChildren ;


/* For 3 frame display/normal display. */
typedef struct
{
  FooCanvasGroup *forward_group, *reverse_group ;
  ZMapFeatureBlock block ;
  ZMapWindow window ;

  FooCanvasGroup *curr_forward_col ;
  FooCanvasGroup *curr_reverse_col ;

  int frame3_pos ;
  ZMapFrame frame ;

} RedrawDataStruct, *RedrawData ;



typedef enum {COLINEAR_INVALID, COLINEAR_NOT, COLINEAR_IMPERFECT, COLINEAR_PERFECT} ColinearityType ;




typedef struct
{
  ZMapWindowItemFeatureBlockData block_data ;
  int start, end ;
  gboolean in_view ;
  double left, right ;
  GList *columns_hidden ;
  ZMapWindowCompressMode compress_mode ;
} VisCoordsStruct, *VisCoords ;


typedef struct
{
  ZMapWindow window;

  /* Records which alignment, block, set, type we are processing. */
  ZMapFeatureContext full_context ;
  ZMapFeatureAlignment curr_alignment ;
  ZMapFeatureBlock curr_block ;
  ZMapFeatureSet curr_set ;

  FooCanvasGroup *curr_root_group ;
  FooCanvasGroup *curr_align_group ;
  FooCanvasGroup *curr_block_group ;
  FooCanvasGroup *curr_forward_group ;
  FooCanvasGroup *curr_reverse_group ;

  FooCanvasGroup *curr_forward_col ;
  FooCanvasGroup *curr_reverse_col ;

}SeparatorCanvasDataStruct, *SeparatorCanvasData;


static void preZoomCB(FooCanvasGroup *data, FooCanvasPoints *points, 
                      ZMapContainerLevelType level, gpointer user_data) ;
static void resetWindowWidthCB(FooCanvasGroup *data, FooCanvasPoints *points, 
                               ZMapContainerLevelType level, gpointer user_data);
static void columnZoomChanged(FooCanvasGroup *container, double new_zoom, ZMapWindow window) ;


static gint horizPosCompare(gconstpointer a, gconstpointer b) ;




static void remove3Frame(ZMapWindow window) ;
static void remove3FrameCol(FooCanvasGroup *container, FooCanvasPoints *points, 
                            ZMapContainerLevelType level, gpointer user_data) ;

static void redraw3FrameNormal(ZMapWindow window) ;
static void redraw3FrameCol(FooCanvasGroup *container, FooCanvasPoints *points, 
                            ZMapContainerLevelType level, gpointer user_data) ;
static void createSetColumn(gpointer data, gpointer user_data) ;
static void drawSetFeatures(GQuark key_id, gpointer data, gpointer user_data) ;

static void redrawAs3Frames(ZMapWindow window) ;
static void redrawAs3FrameCols(FooCanvasGroup *container, FooCanvasPoints *points, 
                               ZMapContainerLevelType level, gpointer user_data) ;
static void create3FrameCols(gpointer data, gpointer user_data) ;
static void draw3FrameSetFeatures(GQuark key_id, gpointer data, gpointer user_data) ;
#ifdef UNUSED_FUNCTIONS
static gint compareNameToColumn(gconstpointer list_data, gconstpointer user_data) ;
#endif

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static void printChild(gpointer data, gpointer user_data) ;
static void printQuarks(gpointer data, gpointer user_data) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



static void hideColsCB(FooCanvasGroup *data, FooCanvasPoints *points, 
		       ZMapContainerLevelType level, gpointer user_data) ;
static void featureInViewCB(void *data, void *user_data) ;
static void showColsCB(void *data, void *user_data) ;
static void rebumpColsCB(void *data, void *user_data) ;

static ZMapFeatureContextExecuteStatus draw_separator_features(GQuark key_id,
							       gpointer feature_data,
							       gpointer user_data,
							       char **error_out);
static void drawSeparatorFeatures(SeparatorCanvasData canvas_data, ZMapFeatureContext context);


/*! @addtogroup zmapwindow
 * @{
 *  */


/*!
 * Toggles the display of the Reading Frame columns, these columns show frame senstive
 * features in separate sets of columns according to their reading frame.
 *
 * @param window     The zmap window to display the reading frame in.
 * @return  <nothing>
 *  */
void zMapWindowToggle3Frame(ZMapWindow window)
{

  zMapWindowBusy(window, TRUE) ;

  /* Remove all col. configuration windows as columns will be destroyed/recreated and the column
   * list will be out of date. */
  zmapWindowColumnConfigureDestroy(window) ;

  /* Remove all frame sensitive cols as they must all be redisplayed. */
  remove3Frame(window) ;


  /* Now redraw the columns either as "normal" or as "3 frame" display.
   * NOTE the dna/protein call is a hack and the code needs to be changed to
   * draw the 3 frame stuff as 3 properly separate columns. */
  if (window->display_3_frame)
    {
      redraw3FrameNormal(window) ;

      zMapWindowToggleDNAProteinColumns(window, 0, 0, FALSE, TRUE, FALSE, TRUE) ;
    }
  else
    {
      redrawAs3Frames(window) ;

      //zMapWindowToggleDNAProteinColumns(window, 0, 0, FALSE, TRUE, TRUE, TRUE) ;
    }

  window->display_3_frame = !window->display_3_frame ;

  zMapWindowBusy(window, FALSE) ;

  return ;
}



/*! @} end of zmapwindow docs. */




/* Sorts the children of a group by horizontal position, sorts all children that
 * are groups and leaves children that are items untouched, this is because item children
 * are background items that do not need to be ordered.
 */
void zmapWindowCanvasGroupChildSort(FooCanvasGroup *group_inout)
{

  zMapAssert(FOO_IS_CANVAS_GROUP(group_inout)) ;

  group_inout->item_list = g_list_sort(group_inout->item_list, horizPosCompare) ;
  group_inout->item_list_end = g_list_last(group_inout->item_list);

  /* Now reset the last item as well !!!! */
  group_inout->item_list_end = g_list_last(group_inout->item_list) ;

  return ;
}




/* some column functions.... */

/* This function sets the current visibility of a column according to the column state,
 * this may be easy (off or on) or may depend on current mag state/mark or compress state.
 * 
 *  */
void zmapWindowColumnSetState(ZMapWindow window, FooCanvasGroup *column_group,
			      ZMapStyleColumnDisplayState new_col_state, gboolean redraw_if_needed)
{
  ZMapWindowItemFeatureSetData set_data ;
  ZMapFeatureTypeStyle style ;
  ZMapStyleColumnDisplayState curr_col_state ;

  set_data = g_object_get_data(G_OBJECT(column_group), ITEM_FEATURE_SET_DATA) ;
  zMapAssert(set_data) ;

  style = set_data->style ;

  curr_col_state = zMapStyleGetDisplay(style) ;

  /* Do we need a redraw....not every time..... */
  if (new_col_state != curr_col_state)
    {
      gboolean redraw = FALSE ;

      switch(new_col_state)
	{
	case ZMAPSTYLE_COLDISPLAY_HIDE:
	  {
	    /* Always hide column. */
	    zmapWindowContainerSetVisibility(column_group, FALSE) ;

	    redraw = TRUE ;
	    break ;
	  }
	case ZMAPSTYLE_COLDISPLAY_SHOW_HIDE:
	  {
	    gboolean mag_visible ;

	    mag_visible = zmapWindowColumnIsMagVisible(window, column_group) ;

	    /* Check mag, mark, compress etc. etc....probably need some funcs in compress/mark/mag
	     * packages to return whether a column should be hidden.... */
	    if (curr_col_state == ZMAPSTYLE_COLDISPLAY_HIDE && mag_visible)
	      {
		zmapWindowContainerSetVisibility(column_group, TRUE) ;
		redraw = TRUE ;
	      }
	    else if (curr_col_state == ZMAPSTYLE_COLDISPLAY_SHOW && !mag_visible)
	      {
		zmapWindowContainerSetVisibility(column_group, FALSE) ;
		redraw = TRUE ;
	      }

	    break ;
	  }
	default: /* ZMAPSTYLE_COLDISPLAY_SHOW */
	  {
	    /* Always show column. */
	    zmapWindowContainerSetVisibility(column_group, TRUE) ;

	    redraw = TRUE ;
	    break ;
	  }
	}

      zMapStyleSetDisplay(style, new_col_state) ;

      /* Only do redraw if it was requested _and_ state change needs it. */
      if (redraw_if_needed && redraw)
	zmapWindowFullReposition(window) ;
    }

  
  return ;
}



/* Set the hidden status of a column group, currently this depends on the mag setting in the
 * style for the column but we could allow the user to switch this on and off as well. */
void zmapWindowColumnSetMagState(ZMapWindow window, FooCanvasGroup *col_group)
{
  ZMapWindowItemFeatureSetData set_data ;
  ZMapFeatureTypeStyle style = NULL;

  zMapAssert(window && FOO_IS_CANVAS_GROUP(col_group)) ;

  set_data = zmapWindowContainerGetData(col_group, ITEM_FEATURE_SET_DATA) ;
  zMapAssert(set_data) ;
  style = set_data->style ;

  /* Only check the mag factor if the column is visible. (wrong) */

  /* 
   * This test needs to be thought about.  We can't test if a column
   * is visible as it'll never get shown on zoom in. We don't want to 
   * mess with the columns that have been hidden by the user though 
   * (as happens now). I'm not sure we have a record of this.
   */

  if (zMapStyleGetDisplay(style) == ZMAPSTYLE_COLDISPLAY_SHOW_HIDE)
    {
      double min_mag, max_mag ;

      min_mag = zMapStyleGetMinMag(style) ;
      max_mag = zMapStyleGetMaxMag(style) ;


      if (min_mag > 0.0 || max_mag > 0.0)
	{
	  double curr_zoom ;

	  curr_zoom = zMapWindowGetZoomMagnification(window) ;

	  if ((min_mag && curr_zoom < min_mag)
	      || (max_mag && curr_zoom > max_mag))
	    {
	      zmapWindowColumnHide(col_group) ;
	    }
	  else
	    {
	      zmapWindowColumnShow(col_group) ; 
	    }
	}
    }

  return ;
}


/* Checks to see if column would be visible according to current mag state.
 */
gboolean zmapWindowColumnIsMagVisible(ZMapWindow window, FooCanvasGroup *col_group)
{
  gboolean visible = TRUE ;
  ZMapWindowItemFeatureSetData set_data ;
  ZMapFeatureTypeStyle style ;
  double min_mag, max_mag ;
  double curr_zoom ;

  zMapAssert(window && FOO_IS_CANVAS_GROUP(col_group)) ;

  set_data = zmapWindowContainerGetData(col_group, ITEM_FEATURE_SET_DATA) ;
  zMapAssert(set_data) ;
  style = set_data->style ;

  curr_zoom = zMapWindowGetZoomMagnification(window) ;

  if (zMapStyleIsMinMag(style, &min_mag) && curr_zoom < min_mag)
    visible = FALSE ;

  if (zMapStyleIsMaxMag(style, &max_mag) && curr_zoom > max_mag)
    visible = FALSE ;

  return visible ;
}




/* These next two calls should be followed in the user's code
 * by a call to zmapWindowNewReposition()...
 * Doing 
 *  g_list_foreach(columns_to_hide, call_column_hide, NULL);
 *  zmapWindowNewReposition(window);
 * Is a lot quicker than putting the Reposition call in these.
 */
void zmapWindowColumnHide(FooCanvasGroup *column_group)
{
  zMapAssert(column_group && FOO_IS_CANVAS_GROUP(column_group)) ;

  zmapWindowContainerSetVisibility(column_group, FALSE) ;

  return ;
}

void zmapWindowColumnShow(FooCanvasGroup *column_group)
{
  zMapAssert(column_group && FOO_IS_CANVAS_GROUP(column_group)) ;

  zmapWindowContainerSetVisibility(column_group, TRUE) ;

  return ;
}


/* Function toggles compression on and off. */
void zmapWindowColumnsCompress(FooCanvasItem *column_item, ZMapWindow window, ZMapWindowCompressMode compress_mode)
{
  ZMapWindowItemFeatureType feature_type ;
  ZMapContainerLevelType container_type ;
  FooCanvasGroup *column_group =  NULL, *block_group = NULL ;
  ZMapWindowItemFeatureBlockData block_data ;
  VisCoordsStruct coords ;
  double wx1, wy1, wx2, wy2 ;

  /* Decide if the column_item is a column group or a feature within that group. */
  if ((feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(column_item), ITEM_FEATURE_TYPE)))
      != ITEM_FEATURE_INVALID)
    {
      column_group = zmapWindowContainerGetParentContainerFromItem(column_item) ;
    }
  else if ((container_type = zmapWindowContainerGetLevel(FOO_CANVAS_GROUP(column_item)))
	   == ZMAPCONTAINER_LEVEL_FEATURESET)
    {
      column_group = FOO_CANVAS_GROUP(column_item) ;
    }
  else
    zMapAssertNotReached() ;

  block_group = zmapWindowContainerGetSuperGroup(column_group) ;
  container_type = zmapWindowContainerGetLevel(FOO_CANVAS_GROUP(block_group)) ;
  block_group = zmapWindowContainerGetSuperGroup(block_group) ;
  container_type = zmapWindowContainerGetLevel(FOO_CANVAS_GROUP(block_group)) ;
  
  block_data = g_object_get_data(G_OBJECT(block_group), ITEM_FEATURE_BLOCK_DATA) ;
  zMapAssert(block_data) ;

  if (block_data->compressed_cols || block_data->bumped_cols)
    {
      if (block_data->compressed_cols)
	{
	  g_list_foreach(block_data->compressed_cols, showColsCB, NULL) ;
	  g_list_free(block_data->compressed_cols) ;
	  block_data->compressed_cols = NULL ;
	}

      if (block_data->bumped_cols)
	{
	  g_list_foreach(block_data->bumped_cols, rebumpColsCB, GUINT_TO_POINTER(compress_mode)) ;
	  g_list_free(block_data->bumped_cols) ;
	  block_data->bumped_cols = NULL ;
	}

      zmapWindowFullReposition(window) ;
    }
  else
    {
      coords.block_data = block_data ;
      coords.compress_mode = compress_mode ;

      /* If there is no mark or user asked for visible area only then do that. */
      if (compress_mode == ZMAPWWINDOW_COMPRESS_VISIBLE)
	{
	  zmapWindowItemGetVisibleCanvas(window, 
					 &wx1, &wy1,
					 &wx2, &wy2) ;
        
	  /* should really clamp to seq. start/end..... */
	  coords.start = (int)wy1 ;
	  coords.end = (int)wy2 ;
	}
      else if (compress_mode == ZMAPWWINDOW_COMPRESS_MARK)
	{
	  /* we know mark is set so no need to check result of range check. But should check
	   * that col to be bumped and mark are in same block ! */
	  zmapWindowMarkGetSequenceRange(window->mark, &coords.start, &coords.end) ;
	}
      else
	{
	  coords.start = window->min_coord ;
	  coords.end = window->max_coord ;
	}

      /* Note that curretly we need to start at the top of the tree or redraw does
       * not work properly. */
      zmapWindowreDrawContainerExecute(window, hideColsCB, &coords);
    }

  return ;
}


void zmapWindowContainerMoveEvent(FooCanvasGroup *super_root, ZMapWindow window)
{
  ContainerType type = CONTAINER_INVALID ;

  type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(super_root), CONTAINER_TYPE_KEY)) ;
  zMapAssert(type = CONTAINER_ROOT);
  /* pre callback was set to zmapWindowContainerRegionChanged */
  zmapWindowContainerExecuteFull(FOO_CANVAS_GROUP(super_root),
                                 ZMAPCONTAINER_LEVEL_FEATURESET,
                                 NULL, NULL,
                                 NULL, NULL, TRUE) ;
  return ;
}

/*!
 * It's easy to forget, or not have access to set the window
 * scrollregion using resetWindowWidthCB, so here's a function
 * to do it.
 *
 * It does what zmapWindowFullReposition does, but in the same
 * cycle of recursion as the user's. Most calls to 
 * zmapWindowContainerExecuteFull which had TRUE for the last
 * parameter were missing doing this...
 *
 * N.B. All containers are traversed, so if speed is an issue...
 *
 * @param window     ZMapWindow to reset width for
 * @param enter_cb   Callback to get called at each container.
 * @param enter_data Data passed to callback as last parameter.
 * @return void
 */
void zmapWindowreDrawContainerExecute(ZMapWindow             window,
				      ZMapContainerExecFunc  enter_cb,
				      gpointer               enter_data)
{
  zMapPrintTimer(NULL, "About to do some work - including a reposition") ;

  window->interrupt_expose = TRUE ;

  zmapWindowContainerExecuteFull(FOO_CANVAS_GROUP(window->feature_root_group),
				 ZMAPCONTAINER_LEVEL_FEATURESET,
				 enter_cb,
				 enter_data,
				 resetWindowWidthCB, window,
				 TRUE) ;

  window->interrupt_expose = FALSE ;

  zMapPrintTimer(NULL, "Finished the work - including a reposition") ;

  return ;
}


/* Makes sure all the things that need to be redrawn when the canvas needs redrawing. */
void zmapWindowFullReposition(ZMapWindow window)
{
  FooCanvasGroup *super_root ;
  ContainerType type = CONTAINER_INVALID ;

  zMapPrintTimer(NULL, "About to resposition") ;



  super_root = FOO_CANVAS_GROUP(zmapWindowFToIFindItemFull(window->context_to_item,
							   0,0,0,
							   ZMAPSTRAND_NONE, ZMAPFRAME_NONE,
							   0)) ;
  zMapAssert(super_root) ;

  type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(super_root), CONTAINER_TYPE_KEY)) ;
  zMapAssert(type = CONTAINER_ROOT) ;


  window->interrupt_expose = TRUE ;

  /* This could probably call the col order stuff as pre recurse function... */
  zmapWindowContainerExecuteFull(FOO_CANVAS_GROUP(super_root),
                                 ZMAPCONTAINER_LEVEL_FEATURESET,
                                 NULL,
                                 NULL,
                                 resetWindowWidthCB,
                                 window, TRUE) ;

  window->interrupt_expose = FALSE ;

  zmapWindowReFocusHighlights(window);

  zMapPrintTimer(NULL, "Finished resposition") ;

  return ;
}


/* Reset scrolled region width so that user can scroll across whole of canvas. */
void zmapWindowResetWidth(ZMapWindow window)
{
  FooCanvasGroup *root ;
  double x1, x2, y1, y2 ;
  double root_x1, root_x2, root_y1, root_y2 ;
  double scr_reg_width, root_width ;


  foo_canvas_get_scroll_region(window->canvas, &x1, &y1, &x2, &y2);

  scr_reg_width = x2 - x1 + 1.0 ;

  root = foo_canvas_root(window->canvas) ;

  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(root), &root_x1, &root_y1, &root_x2, &root_y2) ;

  root_width = root_x2 - root_x1 + 1 ;

  if (root_width != scr_reg_width)
    {
      double excess ;

      excess = root_width - scr_reg_width ;

      x2 = x2 + excess;

      foo_canvas_set_scroll_region(window->canvas, x1, y1, x2, y2) ;
    }


  return ;
}



/* Makes sure all the things that need to be redrawn for zooming get redrawn. */
void zmapWindowDrawZoom(ZMapWindow window)
{
  FooCanvasGroup *super_root ;
  ContainerType type = CONTAINER_INVALID ;
  ZoomDataStruct zoom_data = {NULL} ;

  super_root = FOO_CANVAS_GROUP(zmapWindowFToIFindItemFull(window->context_to_item,
							   0,0,0,
							   ZMAPSTRAND_NONE, ZMAPFRAME_NONE,
							   0)) ;
  zMapAssert(super_root) ;


  type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(super_root), CONTAINER_TYPE_KEY)) ;
  zMapAssert(type = CONTAINER_ROOT) ;

  zoom_data.window = window ;
  zoom_data.zoom = zMapWindowGetZoomFactor(window) ;

  zmapWindowContainerExecuteFull(FOO_CANVAS_GROUP(super_root),
                                 ZMAPCONTAINER_LEVEL_FEATURESET,
                                 preZoomCB,
                                 &zoom_data,
                                 resetWindowWidthCB,
                                 window, TRUE) ;

  return ;
}


/* Some features have different widths according to their score, this helps annotators
 * guage whether something like an alignment is worth taking notice of.
 * 
 * Currently we only implement the algorithm given by acedb's Score_by_width but it maybe
 * that we will need to do others to support their display.
 *
 * I've done this because for James Havana DBs all methods that mention score have:
 *
 * Score_by_width	
 * Score_bounds	 70.000000 130.000000
 *
 * (the bounds are different for different data sources...)
 * 
 * The interface is slightly tricky here in that we use the style->width to calculate
 * width, not (x2 - x1), hence x2 is only used to return result.
 * 
 */
void zmapWindowGetPosFromScore(ZMapFeatureTypeStyle style, 
			       double score,
			       double *curr_x1_inout, double *curr_x2_out)
{
  double dx = 0.0 ;
  double numerator, denominator ;
  double max_score, min_score ;

  zMapAssert(style && curr_x1_inout && curr_x2_out) ;

  min_score = zMapStyleGetMinScore(style) ;
  max_score = zMapStyleGetMaxScore(style) ;

  /* We only do stuff if there are both min and max scores to work with.... */
  if (max_score && min_score)
    {
      double start = *curr_x1_inout ;
      double width, half_width, mid_way ;

      width = zMapStyleGetWidth(style) ;

      half_width = width / 2 ;
      mid_way = start + half_width ;

      numerator = score - min_score ;
      denominator = max_score - min_score ;

      if (denominator == 0)				    /* catch div by zero */
	{
	  if (numerator < 0)
	    dx = 0.25 ;
	  else if (numerator > 0)
	    dx = 1 ;
	}
      else
	{
	  dx = 0.25 + (0.75 * (numerator / denominator)) ;
	}

      if (dx < 0.25)
	dx = 0.25 ;
      else if (dx > 1)
	dx = 1 ;
      
      *curr_x1_inout = mid_way - (half_width * dx) ;
      *curr_x2_out = mid_way + (half_width * dx) ;
    }

  return ;
}



/* Sort the column list according to the feature names order. */
void zmapWindowSortCols(GList *col_names, FooCanvasGroup *col_container, gboolean reverse)

{
#ifdef RDS_DONT_INCLUDE
  FooCanvasGroup *column_group ;
  GList *column_list ;
  GList *next ;
  int last_position ;

  zMapAssert(col_names && FOO_IS_CANVAS_GROUP(col_container)) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  g_list_foreach(col_names, printQuarks, NULL) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  column_group = zmapWindowContainerGetFeatures(col_container) ;
  column_list = column_group->item_list ;

  if (reverse)
    next = g_list_last(col_names) ;
  else
    next = g_list_first(col_names) ;

  last_position = 0 ;

  do
    {
      GList *column_element ;

      if ((column_element = g_list_find_custom(column_list, next->data, compareNameToColumn)))
	{
	  FooCanvasGroup *col_group ;

	  col_group = FOO_CANVAS_GROUP(column_element->data) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  printf("moving column %s to position %d\n",
		 g_quark_to_string(GPOINTER_TO_INT(next->data)), last_position) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	  column_list = zMap_g_list_move(column_list, column_element->data, last_position) ;

	  last_position++ ;
	}

      if (reverse)
	next = g_list_previous(next) ;
      else
	next = g_list_next(next) ;

    } while (next) ;

  /* Note how having rearranged the list we MUST reset the cached last item pointer !! */
  column_group->item_list = column_list ;
  column_group->item_list_end = g_list_last(column_group->item_list) ;

#endif
  return ;
}

void zmapWindowDrawSeparatorFeatures(ZMapWindow window, 
				     ZMapFeatureBlock block,
				     ZMapFeatureSet feature_set)
{
  ZMapFeatureContext context_cp, context, diff;
  ZMapFeatureAlignment align_cp, align;
  ZMapFeatureBlock     block_cp;

  /* We need the block to know which one to draw into... */

  if(zMapStyleDisplayInSeparator(feature_set->style))
    {
      SeparatorCanvasDataStruct canvas_data = {NULL};
      /* this is a good start. */
      /* need to copy the block, */
      block_cp = (ZMapFeatureBlock)zMapFeatureAnyCopy((ZMapFeatureAny)block);
      /* ... the alignment ... */
      align    = (ZMapFeatureAlignment)zMapFeatureGetParentGroup((ZMapFeatureAny)block, 
								 ZMAPFEATURE_STRUCT_ALIGN) ;
      align_cp = (ZMapFeatureAlignment)zMapFeatureAnyCopy((ZMapFeatureAny)align);
      /* ... and the context ... */
      context  = (ZMapFeatureContext)zMapFeatureGetParentGroup((ZMapFeatureAny)align, 
							       ZMAPFEATURE_STRUCT_CONTEXT) ;
      context_cp = (ZMapFeatureContext)zMapFeatureAnyCopy((ZMapFeatureAny)context);
      /* This is pretty important! */
      context_cp->styles = context->styles;

      /* Now make a tree */
      zMapFeatureContextAddAlignment(context_cp, align_cp, context->master_align == align);
      zMapFeatureAlignmentAddBlock(align_cp, block_cp);
      zMapFeatureBlockAddFeatureSet(block_cp, feature_set);

      /* Now we have a context to merge. */
      zMapFeatureContextMerge(&(window->strand_separator_context),
			      &context_cp, &diff);

      canvas_data.window = window;
      canvas_data.full_context = window->strand_separator_context;

      drawSeparatorFeatures(&canvas_data, diff);

      zmapWindowFullReposition(window);
    }
  else if(feature_set->style)
    zMapLogWarning("Trying to draw feature set with non-separator "
		   "style '%s' in strand separator.",
		   zMapStyleGetName(feature_set->style));
  else
    zMapLogWarning("Feature set '%s' has no style!", 
		   g_quark_to_string(feature_set->original_id));

  return;
}



/* 
 *                Internal routines.
 */




/* MAY NEED TO INGNORE BACKGROUND BOXES....WHICH WILL BE ITEMS.... */

/* A GCompareFunc() called from g_list_sort(), compares groups on their horizontal position. */
static gint horizPosCompare(gconstpointer a, gconstpointer b)
{
  gint result = 0 ;
  FooCanvasGroup *group_a = (FooCanvasGroup *)a, *group_b = (FooCanvasGroup *)b ;

  if (!FOO_IS_CANVAS_GROUP(group_a) || !FOO_IS_CANVAS_GROUP(group_b))
    {
      result = 0 ;
    }
  else
    {
      if (group_a->xpos < group_b->xpos)
	result = -1 ;
      else if (group_a->xpos > group_b->xpos)
	result = 1 ;
      else
	result = 0 ;
    }

  return result ;
}




#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static void printItem(FooCanvasItem *item, int indent, char *prefix)
{
  int i ;

  for (i = 0 ; i < indent ; i++)
    {
      printf("  ") ;
    }
  printf("%s", prefix) ;

  zmapWindowPrintItemCoords(item) ;

  return ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



/* We need the window in here.... */
/* GFunc to call on potentially all container groups.
 */
static void preZoomCB(FooCanvasGroup *data, FooCanvasPoints *points, 
                      ZMapContainerLevelType level, gpointer user_data)
{
  FooCanvasGroup *container = (FooCanvasGroup *)data ;
  ZoomData zoom_data = (ZoomData)user_data ;

  switch(level)
    {
    case ZMAPCONTAINER_LEVEL_FEATURESET:
      {
	FooCanvasGroup *container_underlay;
	container_underlay = zmapWindowContainerGetUnderlays(container);
	zmapWindowContainerPurge(container_underlay);
      }
      columnZoomChanged(container, zoom_data->zoom, zoom_data->window) ;
      break ;
    default:
      break ;
    }

  return ;
}



/* A version of zmapWindowResetWidth which uses the points from the recursion to set the width */
static void resetWindowWidthCB(FooCanvasGroup *data, FooCanvasPoints *points, 
                               ZMapContainerLevelType level, gpointer user_data)
{
  ZMapWindow window = NULL;
  double x1, x2, y1, y2 ;       /* scroll region positions */
  double scr_reg_width, root_width ;

  if(level == ZMAPCONTAINER_LEVEL_ROOT)
    {
      window = (ZMapWindow)user_data ;

      zmapWindowGetScrollRegion(window, &x1, &y1, &x2, &y2);

      scr_reg_width = x2 - x1 + 1.0 ;

      root_width = points->coords[2] - points->coords[0] + 1.0 ;

      if (root_width != scr_reg_width)
        {
          double excess ;
          
          excess = root_width - scr_reg_width ;
          /* the spacing should be a border width from somewhere. */
          x2 = x2 + excess + window->config.strand_spacing;

	  /* Annoyingly the initial size of the canvas is an issue here on first draw */
	  if(y2 == 100.0)
	    y2 = window->max_coord;

          zmapWindowSetScrollRegion(window, &x1, &y1, &x2, &y2) ;
        }
    }
  
  return ;
}


static void columnZoomChanged(FooCanvasGroup *container, double new_zoom, ZMapWindow window)
{
  ZMapWindowItemFeatureSetData set_data ;
  ZMapFeatureTypeStyle style = NULL;

  set_data = zmapWindowContainerGetData(container, ITEM_FEATURE_SET_DATA) ;
  zMapAssert(set_data) ;
  style = set_data->style ;

  if (zMapStyleGetDisplay(style) == ZMAPSTYLE_COLDISPLAY_SHOW_HIDE)
    zmapWindowColumnSetMagState(window, container) ;

  return ;
}




/* 
 *             3 Frame Display functions.
 * 
 * 3 frame display is complex in that some columns should only be shown in "3 frame"
 * mode, whereas others are shown as a single column normally and then as the three
 * "frame" columns in "3 frame" mode. This involves deleting columns, moving them,
 * recreating them etc.
 * 
 */


/*
 *      Functions to remove frame sensitive columns from the canvas.
 */

static void remove3Frame(ZMapWindow window)
{
  FooCanvasGroup *super_root ;

  super_root = FOO_CANVAS_GROUP(zmapWindowFToIFindItemFull(window->context_to_item,
							   0,0,0,
							   ZMAPSTRAND_NONE, ZMAPFRAME_NONE,
							   0)) ;
  zMapAssert(super_root) ;


  zmapWindowContainerExecute(super_root,
                             ZMAPCONTAINER_LEVEL_FEATURESET,
                             remove3FrameCol, window);

  return ;
}

static void remove3FrameCol(FooCanvasGroup *container, FooCanvasPoints *points, 
                            ZMapContainerLevelType level, gpointer user_data)
{
  ZMapWindow window = (ZMapWindow)user_data ;

  if (level == ZMAPCONTAINER_LEVEL_FEATURESET)
    {
      ZMapFeatureTypeStyle style ;
      gboolean frame_specific = FALSE ;

      style = zmapWindowContainerGetStyle(container) ;
      zMapAssert(style) ;
      zMapStyleGetStrandAttrs(style, NULL, &frame_specific, NULL, NULL ) ;

      if (frame_specific)
	{
	  ZMapWindowItemFeatureSetData set_data ;
	  FooCanvasGroup *parent ;

	  set_data = g_object_get_data(G_OBJECT(container), ITEM_FEATURE_SET_DATA) ;
	  zMapAssert(set_data) ;

	  if (set_data->strand != ZMAPSTRAND_REVERSE
	      || (set_data->strand == ZMAPSTRAND_REVERSE && window->show_3_frame_reverse))
	    {
	      parent = zmapWindowContainerGetSuperGroup(container) ;
	      parent = zmapWindowContainerGetFeatures(parent) ;

	      zmapWindowContainerDestroy(container) ;
	    }
	}
    }

  return ;
}




/*
 *   Functions to draw as "normal" (i.e. not read frame) the frame sensitive columns
 *   from the canvas.
 */

static void redraw3FrameNormal(ZMapWindow window)
{
  FooCanvasGroup *super_root ;

  super_root = FOO_CANVAS_GROUP(zmapWindowFToIFindItemFull(window->context_to_item,
							   0,0,0,
							   ZMAPSTRAND_NONE, ZMAPFRAME_NONE,
							   0)) ;
  zMapAssert(super_root) ;

  zmapWindowContainerExecute(super_root,
                             ZMAPCONTAINER_LEVEL_BLOCK,
                             redraw3FrameCol, window);

  zmapWindowColOrderColumns(window);
  
  zmapWindowFullReposition(window) ;

  return ;
}


static void redraw3FrameCol(FooCanvasGroup *container, FooCanvasPoints *points, 
                            ZMapContainerLevelType level, gpointer user_data)
{
  ZMapWindow window = (ZMapWindow)user_data ;

  if (level == ZMAPCONTAINER_LEVEL_BLOCK)
    {
      RedrawDataStruct redraw_data = {NULL} ;
      FooCanvasGroup *block_children ;
      FooCanvasGroup *forward, *reverse ;

      redraw_data.block = g_object_get_data(G_OBJECT(container), ITEM_FEATURE_DATA) ;
      redraw_data.window = window ;

      block_children = zmapWindowContainerGetFeatures(container) ;

      forward = zmapWindowContainerGetStrandGroup(container, ZMAPSTRAND_FORWARD) ;
      redraw_data.forward_group = zmapWindowContainerGetFeatures(forward) ;

      if (window->show_3_frame_reverse)
	{
	  reverse = zmapWindowContainerGetStrandGroup(container, ZMAPSTRAND_REVERSE) ;
	  redraw_data.reverse_group = zmapWindowContainerGetFeatures(reverse) ;
	}

      /* Recreate all the frame sensitive columns as single columns and populate them with features. */
      g_list_foreach(window->feature_set_names, createSetColumn, &redraw_data) ;

      /* Now make sure the columns are in the right order. */
      zmapWindowSortCols(window->feature_set_names, forward, FALSE) ;
      if (window->show_3_frame_reverse)
	zmapWindowSortCols(window->feature_set_names, reverse, TRUE) ;

      /* Now remove any empty columns. */
      if (!(window->keep_empty_cols))
	{
	  zmapWindowRemoveEmptyColumns(window,
				       redraw_data.forward_group, redraw_data.reverse_group) ;
	}

    }

  return ;
}

static void createSetColumn(gpointer data, gpointer user_data)
{
  GQuark feature_set_id = GPOINTER_TO_UINT(data) ;
  RedrawData redraw_data = (RedrawData)user_data ;
  ZMapWindow window = redraw_data->window ;
  FooCanvasGroup *forward_col = NULL, *reverse_col = NULL ;
  ZMapFeatureTypeStyle style ;
  ZMapFeatureSet feature_set ;


  /* need to get style and check for 3 frame..... */
  if (!(style = zMapFindStyle(window->feature_context->styles, feature_set_id)))
    {
      char *name = (char *)g_quark_to_string(feature_set_id) ;

      zMapLogCritical("feature set \"%s\" not displayed because its style (\"%s\") could not be found.",
		      name, name) ;
    }
  else if (feature_set_id != zMapStyleCreateID(ZMAP_FIXED_STYLE_3FRAME)
	   && (zMapStyleIsDisplayable(style) && zMapStyleIsFrameSpecific(style))
	   && ((feature_set = zMapFeatureBlockGetSetByID(redraw_data->block, feature_set_id))))
    {
      /* Make the forward/reverse columns for this feature set. */
      zmapWindowCreateSetColumns(window,
                                 redraw_data->forward_group, 
                                 redraw_data->reverse_group,
				 redraw_data->block, 
                                 feature_set,
                                 ZMAPFRAME_NONE,
				 &forward_col, &reverse_col, NULL) ;

      redraw_data->curr_forward_col = forward_col ;
      redraw_data->curr_reverse_col = reverse_col ;

      /* Now put the features in the columns. */
      drawSetFeatures(feature_set_id, feature_set, redraw_data) ;
    }


  return ;
}


static void drawSetFeatures(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeatureSet feature_set = (ZMapFeatureSet)data ;
  RedrawData redraw_data = (RedrawData)user_data ;
  ZMapWindow window = redraw_data->window ;
  GQuark type_quark ;
  FooCanvasGroup *forward_col, *reverse_col ;
  ZMapFeatureTypeStyle style ;

  /* Each column is known by its type/style name. */
  type_quark = feature_set->unique_id ;
  zMapAssert(type_quark == key_id) ;			    /* sanity check. */

  forward_col = redraw_data->curr_forward_col ;
  reverse_col = redraw_data->curr_reverse_col ;

  /* If neither column can be found it means that there is a mismatch between the requested
   * feature_sets and the styles. This happens when dealing with gff sources read from
   * files for instance because the features must be retrieved using the style names but then
   * filtered by feature_set name, so if extra styles are provided this will result in the
   * mismatch.
   *  */
  if (!forward_col && !reverse_col)
    {
      /* There is a memory leak here in that we do not delete the feature set from the context. */
      char *name = (char *)g_quark_to_string(feature_set->original_id) ;

      /* Temporary...probably user will not want to see this in the end but for now its good for debugging. */
      zMapShowMsg(ZMAP_MSG_WARNING, 
		  "Feature set \"%s\" not displayed because although it was retrieved from the server,"
		  " it is not in the list of feature sets to be displayed."
		  " Please check the list of styles and feature sets in the ZMap configuration file.",
		  name) ;

      zMapLogCritical("Feature set \"%s\" not displayed because although it was retrieved from the server,"
		      " it is not in the list of feature sets to be displayed."
		      " Please check the list of styles and feature sets in the ZMap configuration file.",
		      name) ;

      return ;
    }


  /* Record the feature sets on the column groups. */
  if (forward_col)
    {
      zmapWindowContainerSetData(forward_col, ITEM_FEATURE_DATA, feature_set) ;
    }

  if (reverse_col)
    {
      zmapWindowContainerSetData(reverse_col, ITEM_FEATURE_DATA, feature_set) ;
    }


  /* Check from the style whether this column is a 3 frame column and whether it should
   * ever be shown, if so then draw the features, note that setting frame to ZMAPFRAME_NONE
   * means the frame will be ignored and the features will all be in one column on the.
   * forward or reverse strand. */
  style = feature_set->style ;

  if (zMapStyleIsDisplayable(style) && zMapStyleIsFrameSpecific(style))
    {
      zmapWindowDrawFeatureSet(window, feature_set,
                               forward_col, reverse_col, ZMAPFRAME_NONE) ;
    }

  return ;
}



/* 
 *       Functions to draw the 3 frame columns.
 * 
 * precursors: all existing frame sensitive cols have been removed
 *             all other cols are left intact in their original order 
 * 
 * processing is like this:
 * 
 * find the position where the 3 frame cols should be shown
 * 
 * for (all cols)
 *      for (all frames)
 *         for (all cols)
 *            if (col frame sensitive)
 *                create col
 *                insert col in list of cols at 3 frame position
 *                add features that are in correct frame to that col.
 * 
 * reposition all cols
 * 
 */


static void redrawAs3Frames(ZMapWindow window)
{
  FooCanvasGroup *super_root ;

  super_root = FOO_CANVAS_GROUP(zmapWindowFToIFindItemFull(window->context_to_item,
							   0,0,0,
							   ZMAPSTRAND_NONE, ZMAPFRAME_NONE,
							   0)) ;
  zMapAssert(super_root) ;

  /* possibly change nxt 3 calls to. 
   * zmapWindowContainerExecuteFull(super_root, 
   *                                ZMAPCONTAINER_LEVEL_STRAND,
   *                                redrawAs3FrameCols, window,
   *                                orderColumnCB, order_data, NULL);
   * means moving column code around though. Basically need to decide 
   * on how much NewReposition does...
   */
  zmapWindowContainerExecute(super_root,
                             ZMAPCONTAINER_LEVEL_BLOCK,
                             redrawAs3FrameCols, window);

  zmapWindowColOrderColumns(window);
  
  zmapWindowFullReposition(window) ;

  return ;
}


static void redrawAs3FrameCols(FooCanvasGroup *container, FooCanvasPoints *points, 
                               ZMapContainerLevelType level, gpointer user_data)
{
  ZMapWindow window = (ZMapWindow)user_data ;

  if (level == ZMAPCONTAINER_LEVEL_BLOCK)
    {
      RedrawDataStruct redraw_data = {NULL} ;
      FooCanvasGroup *block_children ;
      FooCanvasGroup *forward, *reverse ;

      redraw_data.block = g_object_get_data(G_OBJECT(container), ITEM_FEATURE_DATA) ;
      redraw_data.window = window ;

      block_children = zmapWindowContainerGetFeatures(container) ;


      forward = zmapWindowContainerGetStrandGroup(container, ZMAPSTRAND_FORWARD) ;
      redraw_data.forward_group = zmapWindowContainerGetFeatures(forward) ;

      if (window->show_3_frame_reverse)
	{
	  reverse = zmapWindowContainerGetStrandGroup(container, ZMAPSTRAND_REVERSE) ;
	  redraw_data.reverse_group = zmapWindowContainerGetFeatures(reverse) ;
	}


      /* We need to find the 3 frame position and use this to insert stuff.... */
      if ((redraw_data.frame3_pos = g_list_index(window->feature_set_names,
						 GINT_TO_POINTER(zMapStyleCreateID(ZMAP_FIXED_STYLE_3FRAME))))
	  == -1)
	{
	  zMapShowMsg(ZMAP_MSG_WARNING,
		      "Could not find 3 frame (\"%s\") in requested list of feature sets"
		      " so 3 Frame columns cannot be displayed.", ZMAP_FIXED_STYLE_3FRAME) ;

	  zMapLogCritical("Could not find 3 frame (\"%s\") in requested list of feature sets"
			  " so 3 Frame columns cannot be displayed.", ZMAP_FIXED_STYLE_3FRAME) ;
	}
      else
	{
	  for (redraw_data.frame = ZMAPFRAME_0 ; redraw_data.frame <= ZMAPFRAME_2 ; redraw_data.frame++)
	    {
	      g_list_foreach(window->feature_set_names, create3FrameCols, &redraw_data) ;
	    }
          
#ifdef RDS_DONT_INCLUDE
          /* We need to draw the translation column in here too */
          if((translation = zmapWindowItemGetTranslationColumnFromBlock(window, redraw_data.block)))
            {
              zmapWindowColumnShow(translation);
            }
#endif
	  /* Remove empty cols.... */
	  zmapWindowRemoveEmptyColumns(window, redraw_data.forward_group, redraw_data.reverse_group) ;
	}

    }

  return ;
}



static void create3FrameCols(gpointer data, gpointer user_data)
{
  GQuark feature_set_id = GPOINTER_TO_UINT(data) ;
  RedrawData redraw_data = (RedrawData)user_data ;
  ZMapWindow window = redraw_data->window ;
  FooCanvasGroup *forward_col = NULL, *reverse_col = NULL ;
  ZMapFeatureTypeStyle style ;
  ZMapFeatureSet feature_set ;

  /* need to get style and check for 3 frame..... */
  if (!(style = zMapFindStyle(window->feature_context->styles, feature_set_id)))
    {
      char *name = (char *)g_quark_to_string(feature_set_id) ;

      zMapLogCritical("feature set \"%s\" not displayed because its style (\"%s\") could not be found.",
		      name, name) ;
    }
  else if ((zMapStyleIsDisplayable(style) && zMapStyleIsFrameSpecific(style)) &&
	   ((feature_set = zMapFeatureBlockGetSetByID(redraw_data->block, feature_set_id))))
    {
      /* Create both forward and reverse columns. */
      zmapWindowCreateSetColumns(window,
                                 redraw_data->forward_group, 
                                 redraw_data->reverse_group,
				 redraw_data->block, 
                                 feature_set, 
                                 redraw_data->frame,
				 &forward_col, &reverse_col, NULL) ;

      /* There was some column ordering code here, but that's now in the
       * zmapWindowColOrderColumns code instead. */

      /* Set the current columns */
      redraw_data->curr_forward_col = forward_col ;

      if (window->show_3_frame_reverse)
        redraw_data->curr_reverse_col = reverse_col ;

      /* Now draw the features into the forward and reverse columns, empty columns are removed
       * at this stage. */
      draw3FrameSetFeatures(feature_set_id, feature_set, redraw_data) ;

      if (forward_col)
        zmapWindowColumnShow(forward_col) ;

      if(reverse_col)
        zmapWindowColumnShow(reverse_col) ;

      redraw_data->frame3_pos++ ;
    }


  return ;
}


static void draw3FrameSetFeatures(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeatureSet feature_set = (ZMapFeatureSet)data ;
  RedrawData redraw_data = (RedrawData)user_data ;
  ZMapWindow window = redraw_data->window ;
  GQuark type_quark ;
  FooCanvasGroup *forward_col, *reverse_col ;
  ZMapFeatureTypeStyle style ;


  /* Each column is known by its type/style name. */
  type_quark = feature_set->unique_id ;
  zMapAssert(type_quark == key_id) ;			    /* sanity check. */

  forward_col = redraw_data->curr_forward_col ;
  reverse_col = redraw_data->curr_reverse_col ;

  /* If neither column can be found it means that there is a mismatch between the requested
   * feature_sets and the styles. This happens when dealing with gff sources read from
   * files for instance because the features must be retrieved using the style names but then
   * filtered by feature_set name, so if extra styles are provided this will result in the
   * mismatch.
   *  */
  if (!forward_col && !reverse_col)
    {
      /* There is a memory leak here in that we do not delete the feature set from the context. */
      char *name = (char *)g_quark_to_string(feature_set->original_id) ;

      /* Temporary...probably user will not want to see this in the end but for now its good for debugging. */
      zMapShowMsg(ZMAP_MSG_WARNING, 
		  "Feature set \"%s\" not displayed because although it was retrieved from the server,"
		  " it is not in the list of feature sets to be displayed."
		  " Please check the list of styles and feature sets in the ZMap configuration file.",
		  name) ;

      zMapLogCritical("Feature set \"%s\" not displayed because although it was retrieved from the server,"
		      " it is not in the list of feature sets to be displayed."
		      " Please check the list of styles and feature sets in the ZMap configuration file.",
		      name) ;

      return ;
    }


  /* Set the feature set data on the canvas group so we can get it from event handlers. */
  if (forward_col)
    {
      zmapWindowContainerSetData(forward_col, ITEM_FEATURE_DATA, feature_set) ;
    }

  if (reverse_col)
    {
      zmapWindowContainerSetData(reverse_col, ITEM_FEATURE_DATA, feature_set) ;
    }


  /* need to get style and check for 3 frame and if column should be displayed and then
   * draw features. */
  style = feature_set->style ;

  if (zMapStyleIsDisplayable(style) && zMapStyleIsFrameSpecific(style))
    {
      zmapWindowDrawFeatureSet(window, feature_set,
                               forward_col, reverse_col, 
                               redraw_data->frame) ;
    }

  return ;
}


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE

/* 
 *             debug functions, please leave here for future use.
 */


static void printChild(gpointer data, gpointer user_data)
{
  FooCanvasGroup *container = (FooCanvasGroup *)data ;
  ZMapFeatureAny any_feature ;

  any_feature = g_object_get_data(G_OBJECT(container), ITEM_FEATURE_DATA) ;

  printf("%s ", g_quark_to_string(any_feature->unique_id)) ;

  return ;
}



static void printPtr(gpointer data, gpointer user_data)
{
  FooCanvasGroup *container = (FooCanvasGroup *)data ;

  printf("%p ", container) ;

  return ;
}


static void printQuarks(gpointer data, gpointer user_data)
{


  printf("quark str: %s\n", g_quark_to_string(GPOINTER_TO_INT(data))) ;


  return ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



/* GFunc to call on potentially all container groups.
 */
static void hideColsCB(FooCanvasGroup *data, FooCanvasPoints *points, 
		       ZMapContainerLevelType level, gpointer user_data)
{
  FooCanvasGroup *container = (FooCanvasGroup *)data ;
  VisCoords coord_data = (VisCoords)user_data ;

  switch(level)
    {
    case ZMAPCONTAINER_LEVEL_FEATURESET:
      {
	FooCanvasGroup *features ;

	coord_data->in_view = FALSE ;
	coord_data->left = 999999999 ;			    /* Must be bigger than any feature position. */
	coord_data->right = 0 ;

	features = zmapWindowContainerGetFeatures(container) ;

	g_list_foreach(features->item_list, featureInViewCB, coord_data) ;

	if (zmapWindowItemIsShown(FOO_CANVAS_ITEM(container)))
	  {
	    ZMapFeatureSet feature_set ;
	    ZMapWindowItemFeatureSetData set_data ;
	    ZMapFeatureTypeStyle style ;

	    feature_set = g_object_get_data(G_OBJECT(container), ITEM_FEATURE_DATA);
	    zMapAssert(feature_set) ;

	    set_data = g_object_get_data(G_OBJECT(container), ITEM_FEATURE_SET_DATA) ;
	    zMapAssert(set_data) ;

	    style = set_data->style ;



	    if (!(coord_data->in_view)
		&& (coord_data->compress_mode == ZMAPWWINDOW_COMPRESS_VISIBLE
		    || zMapStyleGetDisplay(style) != ZMAPSTYLE_COLDISPLAY_SHOW))
	      {
		/* No items overlap with given area so hide the column completely. */

		zmapWindowColumnHide(container) ;

		coord_data->block_data->compressed_cols = g_list_append(coord_data->block_data->compressed_cols,
									container) ;
	      }
	    else
	      {
		/* There are some items showing but column may need to be rebumped if there only a few. */
		double x1, y1, x2, y2, width_col, width_features ;
		gboolean bump_col = FALSE ;

		foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(container), &x1, &y1, &x2, &y2) ;

		/* Compare width of column and total width of visible features, if difference is
		 * greater than 10% then rebump. This stops us rebumping columns that are just
		 * slightly different in size or have already been bumped to a mark for instance. */
		width_col = (x2 - x1) ;
		width_features = (coord_data->right - coord_data->left) ;

		if (((fabs(width_col - width_features)) / width_features) > 0.1)
		  bump_col = TRUE ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
		printf("col %s %s: %f, %f (%f)\titems: %f, %f (%f)\n",
		       g_quark_to_string(feature_set->original_id),
		       (bump_col ? "Bumped" : "Not Bumped"),
		       x1, x2, width_col,
		       coord_data->left, coord_data->right, width_features) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

		if (bump_col)
		  {
		    zmapWindowColumnBumpRange(FOO_CANVAS_ITEM(container),
					      ZMAPOVERLAP_INVALID, coord_data->compress_mode) ;

		    coord_data->block_data->bumped_cols = g_list_append(coord_data->block_data->bumped_cols,
									container) ;
		  }
	      }
	  }

	coord_data->in_view = FALSE ;

	break ;
      }
    default:
      break ;
    }

  return ;
}


/* check whether a feature is between start and end coords at all. */
static void featureInViewCB(void *data, void *user_data)
{
  FooCanvasItem *feature_item = (FooCanvasItem *)data ;
  VisCoords coord_data = (VisCoords)user_data ;
  ZMapFeature feature ;

  /* bumped cols have items that are _not_ features. */
  if ((feature = g_object_get_data(G_OBJECT(feature_item), ITEM_FEATURE_DATA)))
    {

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      if (g_ascii_strcasecmp("Locus", zMapStyleGetName(feature->style)) == 0)
	printf("found locus\n") ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

      if (!(feature->x1 > coord_data->end || feature->x2 < coord_data->start))
	{
	  double x1, y1, x2, y2 ;

	  coord_data->in_view = TRUE ;

	  foo_canvas_item_get_bounds(feature_item, &x1, &y1, &x2, &y2) ;

	  if (x1 < coord_data->left)
	    coord_data->left = x1 ;
	  if (x2 > coord_data->right)
	    coord_data->right = x2 ;
	}
    }

  return ;
}


/* Reshow hidden cols. */
static void showColsCB(void *data, void *user_data_unused)
{
  FooCanvasGroup *col_group = (FooCanvasGroup *)data ;

  /* This is called from the Compress Columns code, which _is_ a user
   * action. */
  zmapWindowColumnShow(col_group) ; 

  return ;
}

/* Reshow bumped cols. */
static void rebumpColsCB(void *data, void *user_data)
{
  FooCanvasGroup *col_group = (FooCanvasGroup *)data ;

  /* This is called from the Compress Columns code, which _is_ a user
   * action. */

  zmapWindowColumnBumpRange(FOO_CANVAS_ITEM(col_group), ZMAPOVERLAP_INVALID, GPOINTER_TO_UINT(user_data)) ;

  return ;
}


static ZMapFeatureContextExecuteStatus draw_separator_features(GQuark key_id,
							       gpointer feature_data,
							       gpointer user_data,
							       char **error_out)
{
  ZMapFeatureAny feature_any             = (ZMapFeatureAny)feature_data;
  SeparatorCanvasData canvas_data        = (SeparatorCanvasData)user_data;
  ZMapWindow window                      = canvas_data->window;
  ZMapFeatureContextExecuteStatus status = ZMAP_CONTEXT_EXEC_STATUS_OK;

  switch(feature_any->struct_type)
    {
    case ZMAPFEATURE_STRUCT_CONTEXT:
      break;
    case ZMAPFEATURE_STRUCT_ALIGN:
      {
	FooCanvasGroup *align_parent;
	FooCanvasItem  *align_hash_item;

	canvas_data->curr_alignment = 
	  zMapFeatureContextGetAlignmentByID(canvas_data->full_context,
					     feature_any->unique_id);
	if((align_hash_item = zmapWindowFToIFindItemFull(window->context_to_item,
							 feature_any->unique_id,
							 0, 0, ZMAPSTRAND_NONE,
							 ZMAPFRAME_NONE, 0)))
	  {
	    zMapAssert(FOO_IS_CANVAS_GROUP(align_hash_item));
	    align_parent = FOO_CANVAS_GROUP(align_hash_item);
	    canvas_data->curr_align_group = zmapWindowContainerGetFeatures(align_parent);
	  }
	else
	  zMapAssertNotReached();
      }
      break;
    case ZMAPFEATURE_STRUCT_BLOCK:
      {
	FooCanvasGroup *block_parent;
	FooCanvasItem  *block_hash_item;
	canvas_data->curr_block = 
	  zMapFeatureAlignmentGetBlockByID(canvas_data->curr_alignment,
					   feature_any->unique_id);

	if((block_hash_item = zmapWindowFToIFindItemFull(window->context_to_item,
							 canvas_data->curr_alignment->unique_id,
							 feature_any->unique_id, 0,
							 ZMAPSTRAND_NONE, ZMAPFRAME_NONE, 0)))
	  {
	    zMapAssert(FOO_IS_CANVAS_GROUP(block_hash_item));
	    block_parent = FOO_CANVAS_GROUP(block_hash_item);

	    canvas_data->curr_block_group = 
	      zmapWindowContainerGetFeatures(block_parent);
	      
	    canvas_data->curr_forward_group =
	      zmapWindowContainerGetStrandGroup(block_parent, ZMAPSTRAND_FORWARD);
	    canvas_data->curr_forward_group =
	      zmapWindowContainerGetFeatures(canvas_data->curr_forward_group);
	  }
	else
	  zMapAssertNotReached();
      }
      break;
    case ZMAPFEATURE_STRUCT_FEATURESET:
      {
	FooCanvasGroup *tmp_forward, *separator;
	
	canvas_data->curr_set = zMapFeatureBlockGetSetByID(canvas_data->curr_block,
							   feature_any->unique_id);
	if(zmapWindowCreateSetColumns(window,
				      canvas_data->curr_forward_group,
				      canvas_data->curr_reverse_group,
				      canvas_data->curr_block,
				      canvas_data->curr_set,
				      ZMAPFRAME_NONE,
				      &tmp_forward, NULL, &separator))
	  {
	    zmapWindowColumnSetState(window, separator, ZMAPSTYLE_COLDISPLAY_SHOW, FALSE);
	    zmapWindowDrawFeatureSet(window, (ZMapFeatureSet)feature_any,
				     NULL, separator, ZMAPFRAME_NONE);
	    if(tmp_forward)
	      zmapWindowRemoveIfEmptyCol(&tmp_forward);
	  }
      }
      break;
    case ZMAPFEATURE_STRUCT_FEATURE:
      break;
    default:
      zMapAssertNotReached();
      break;
    }

  return status;
}

static void drawSeparatorFeatures(SeparatorCanvasData canvas_data, ZMapFeatureContext context)
{
  zMapFeatureContextExecuteComplete((ZMapFeatureAny)context,
				    ZMAPFEATURE_STRUCT_FEATURE,
				    draw_separator_features,
				    NULL, canvas_data);
  return;
}


