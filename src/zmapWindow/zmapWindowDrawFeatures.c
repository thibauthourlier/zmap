/*  File: zmapWindowDrawFeatures.c
 *  Author: Rob Clack (rnc@sanger.ac.uk)
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
 * and was written by
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk and
 *
 * Description: Draws genomic features in the data display window.
 *              
 * Exported functions: 
 * HISTORY:
 * Last edited: Jan  5 14:26 2006 (edgrif)
 * Created: Thu Jul 29 10:45:00 2004 (rnc)
 * CVS info:   $Id: zmapWindowDrawFeatures.c,v 1.109 2006-01-05 14:31:41 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <string.h>
#include <math.h>
#include <ZMap/zmapUtils.h>
#include <ZMap/zmapGLibUtils.h>
#include <ZMap/zmapUtilsGUI.h>
#include <ZMap/zmapUtilsDNA.h>
#include <ZMap/zmapConfig.h>
#include <ZMap/zmapGFF.h>
#include <zmapWindow_P.h>
#include <zmapWindowContainer.h>


/* Used to pass data to canvas item menu callbacks. */
typedef struct
{
  gboolean item_cb ;					    /* TRUE => item callback,
							       FALSE => column callback. */

  ZMapWindow window ;
  FooCanvasItem *item ;

  ZMapFeatureSet feature_set ;				    /* Only used in column callbacks... */
} ItemMenuCBDataStruct, *ItemMenuCBData ;



/* these will go when scale is in separate window. */
#define SCALEBAR_OFFSET   0.0
#define SCALEBAR_WIDTH   50.0



/* parameters passed between the various functions drawing the features on the canvas, it's
 * simplest to cache the current stuff as we go otherwise the code becomes convoluted by having
 * to look up stuff all the time. */
typedef struct _ZMapCanvasDataStruct
{
  ZMapWindow window ;
  FooCanvas *canvas ;

  /* Records which alignment, block, set, type we are processing. */
  ZMapFeatureContext full_context ;
  ZMapFeatureAlignment curr_alignment ;
  ZMapFeatureBlock curr_block ;
  ZMapFeatureSet curr_set ;

  GQuark curr_set_name ;

  GQuark curr_forward_col_id ;
  GQuark curr_reverse_col_id ;

  /* Records current positional information. */
  double curr_x_offset ;
  double curr_y_offset ;

  /* Records current canvas item groups, these are the direct parent groups of the display
   * types they contain, e.g. curr_root_group is the parent of the align */
  FooCanvasGroup *curr_root_group ;
  FooCanvasGroup *curr_align_group ;
  FooCanvasGroup *curr_block_group ;
  FooCanvasGroup *curr_forward_group ;
  FooCanvasGroup *curr_reverse_group ;


  FooCanvasGroup *curr_forward_col ;
  FooCanvasGroup *curr_reverse_col ;


  GdkColor colour_root ;
  GdkColor colour_alignment ;
  GdkColor colour_block ;
  GdkColor colour_mblock_for ;
  GdkColor colour_mblock_rev ;
  GdkColor colour_qblock_for ;
  GdkColor colour_qblock_rev ;
  GdkColor colour_mforward_col ;
  GdkColor colour_mreverse_col ;
  GdkColor colour_qforward_col ;
  GdkColor colour_qreverse_col ;

} ZMapCanvasDataStruct, *ZMapCanvasData ;



/* Used to pass data to removeEmptyColumn function. */
typedef struct
{
  ZMapCanvasData canvas_data ;
  ZMapStrand strand ;
} RemoveEmptyColumnStruct, *RemoveEmptyColumn ;



typedef struct
{
  double offset ;				    /* I think we do need these... */
} PositionColumnStruct, *PositionColumn ;



static gboolean canvasItemEventCB(FooCanvasItem *item, GdkEvent *event, gpointer data) ;
static gboolean canvasItemDestroyCB(FooCanvasItem *item, GdkEvent *event, gpointer data) ;


static void drawZMap(ZMapCanvasData canvas_data, ZMapFeatureContext diff_context) ;
static void drawAlignments(GQuark key_id, gpointer data, gpointer user_data) ;
static void drawBlocks(gpointer data, gpointer user_data) ;
static void createSetColumn(gpointer data, gpointer user_data) ;
static FooCanvasGroup *createColumn(FooCanvasGroup *parent_group,
				    ZMapWindow window,
				    ZMapFeatureTypeStyle style,
				    ZMapStrand strand,
				    double width,
				    double top, double bot, GdkColor *colour) ;
static void ProcessFeatureSet(GQuark key_id, gpointer data, gpointer user_data) ;
static void ProcessFeature(GQuark key_id, gpointer data, gpointer user_data) ;
static void removeEmptyColumns(ZMapCanvasData canvas_data) ;
static void removeEmptyColumnCB(gpointer data, gpointer user_data) ;

static void positionColumns(ZMapCanvasData canvas_data) ;
static void positionColumnCB(gpointer data, gpointer user_data) ;



static ZMapWindowMenuItem makeMenuFeatureOps(int *start_index_inout,
					     ZMapWindowMenuItemCallbackFunc callback_func,
					     gpointer callback_data) ;
static void itemMenuCB(int menu_item_id, gpointer callback_data) ;
static ZMapWindowMenuItem makeMenuBump(int *start_index_inout,
				       ZMapWindowMenuItemCallbackFunc callback_func,
				       gpointer callback_data) ;
static void bumpMenuCB(int menu_item_id, gpointer callback_data) ;
static ZMapWindowMenuItem makeMenuDumpOps(int *start_index_inout,
					  ZMapWindowMenuItemCallbackFunc callback_func,
					  gpointer callback_data) ;
static void dumpMenuCB(int menu_item_id, gpointer callback_data) ;
static ZMapWindowMenuItem makeMenuDNAHomol(int *start_index_inout,
					   ZMapWindowMenuItemCallbackFunc callback_func,
					   gpointer callback_data) ;
static ZMapWindowMenuItem makeMenuProteinHomol(int *start_index_inout,
					   ZMapWindowMenuItemCallbackFunc callback_func,
					       gpointer callback_data) ;
static void blixemMenuCB(int menu_item_id, gpointer callback_data) ;

static void populateMenu(ZMapWindowMenuItem menu,
			 int *start_index_inout,
			 ZMapWindowMenuItemCallbackFunc callback_func,
			 gpointer callback_data) ;

static void makeItemMenu(GdkEventButton *button_event, ZMapWindow window,
			 FooCanvasItem *item) ;
static void itemMenuCB(int menu_item_id, gpointer callback_data) ;
static gboolean dnaItemEventCB(FooCanvasItem *item, GdkEvent *event, gpointer data) ;


static gboolean columnBoundingBoxEventCB(FooCanvasItem *item, GdkEvent *event, gpointer data) ;
static void makeColumnMenu(GdkEventButton *button_event, ZMapWindow window,
			   FooCanvasItem *item, ZMapFeatureSet feature_set) ;

static ZMapWindowMenuItem makeMenuColumnOps(int *start_index_inout,
					    ZMapWindowMenuItemCallbackFunc callback_func,
					    gpointer callback_data) ;
static void columnMenuCB(int menu_item_id, gpointer callback_data) ;


static FooCanvasItem *drawSimpleFeature(FooCanvasGroup *parent, ZMapFeature feature,
					double feature_offset,
					double x1, double y1, double x2, double y2,
					GdkColor *outline,
                                        GdkColor *foreground,
                                        GdkColor *background,
					ZMapWindow window) ;
static FooCanvasItem *drawTranscriptFeature(FooCanvasGroup *parent, ZMapFeature feature,
					    double feature_offset,
					    double x1, double feature_top,
					    double x2, double feature_bottom,
					    GdkColor *outline, 
                                            GdkColor *foreground,
                                            GdkColor *background,
					    ZMapWindow window) ;
static FooCanvasItem *drawAlignmentFeature(FooCanvasGroup *parent, ZMapFeature feature,
                                           double feature_offset,
                                           double x1, double feature_top,
                                           double x2, double feature_bottom,
                                           GdkColor *outline, 
                                           GdkColor *foreground,
                                           GdkColor *background,
                                           ZMapWindow window) ;
static FooCanvasItem *drawSequenceFeature(FooCanvasGroup *parent, ZMapFeature feature,
                                          double feature_offset,
                                          double feature_zero,      double feature_top,
                                          double feature_thickness, double feature_bottom,
                                          GdkColor *outline, GdkColor *background,
                                          ZMapWindow window) ;
static gboolean featureClipItemToDraw(ZMapFeature feature, 
                                      double *start_inout, 
                                      double *end_inout);
static void attachDataToItem(FooCanvasItem *feature_item, 
                             ZMapWindow window,
                             ZMapFeature feature,
                             ZMapWindowItemFeatureType type,
                             gpointer subFeatureData);


static FooCanvasGroup *getItemsColGroup(FooCanvasItem *item) ;

static void setColours(ZMapCanvasData canvas_data) ;

static void dumpFASTA(ZMapWindow window) ;
static void dumpFeatures(ZMapWindow window, ZMapFeatureAny feature) ;
static void dumpContext(ZMapWindow window) ;
static void pfetchEntry(ZMapWindow window, char *sequence_name) ;




static void dna_redraw_callback(FooCanvasGroup *text_grp,
                                double zoom,
                                gpointer user_data);
static void updateInfoGivenCoords(textGroupSelection select, 
                                  double worldCurrentX,
                                  double worldCurrentY);
static void pointerIsOverItem(gpointer data, gpointer user_data);
static textGroupSelection getTextGroupData(FooCanvasGroup *txtGroup,
                                           ZMapWindow window);
static ZMapDrawTextIterator getIterator(double win_min_coord, double win_max_coord,
                                        double offset_start,  double offset_end,
                                        double text_height, gboolean numbered);
static void destroyIterator(ZMapDrawTextIterator iterator);






/* Drawing coordinates: PLEASE READ THIS BEFORE YOU START MESSING ABOUT WITH ANYTHING...
 * 
 * It seems that item coordinates are _not_ specified in absolute world coordinates but
 * in fact in the coordinates of their parent group. This has a lot of knock on effects as
 * we get our feature coordinates in absolute world coords and wish to draw them as simply
 * as possible.
 * 
 * Currently we have these coordinate systems operating:
 * 
 * 
 * sequence
 *  start           0             0            >= 0    
 *    ^             ^             ^              ^
 *                                                       scr_start
 *                                                          ^
 * 
 *   root          col           col            all         scroll
 *   group        group(s)     bounding       features      region
 *                             box
 * 
 *                                                          v
 *                                                       scr_end
 *    v             v             v              v
 * sequence      max_coord     max_coord     <= max_coord
 *  end + 1
 * 
 * where  max_coord = ((seq_end - seq_start) + 1) + 1 (to cover the last base)
 * 
 */


/************************ external functions ************************************************/


/* REMEMBER WHEN YOU READ THIS CODE THAT THIS ROUTINE MAY BE CALLED TO UPDATE THE FEATURES
 * IN A CANVAS WITH NEW FEATURES FROM A SEPARATE SERVER. */

/* Draw features on the canvas, I think that this routine should _not_ get called unless 
 * there is actually data to draw.......
 * 
 * full_context contains all the features.
 * diff_context contains just the features from full_context that are newly added and therefore
 *              have not yet been drawn.
 * 
 * So NOTE that if no features have _yet_ been drawn then  full_context == diff_context
 * 
 *  */
void zmapWindowDrawFeatures(ZMapWindow window,
			    ZMapFeatureContext full_context, ZMapFeatureContext diff_context)
{
  GtkAdjustment *h_adj;
  ZMapCanvasDataStruct canvas_data = {NULL} ;		    /* Rest of struct gets set to zero. */
  double x1, y1, x2, y2 ;
  FooCanvasGroup *root_group ;
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  FooCanvasItem *background_item ;
  gboolean debug = TRUE ;
#endif
  double x, y ;
  double start, end ;


  zMapAssert(window && full_context && diff_context) ;


  /* Set up colours. */
  setColours(&canvas_data) ;


  /* Must be reset each time because context will change as features get merged in. */
  window->feature_context = full_context ;

  window->seq_start = full_context->sequence_to_parent.c1 ;
  window->seq_end   = full_context->sequence_to_parent.c2 ;
  window->seqLength = zmapWindowExt(window->seq_start, window->seq_end) ;


  zmapWindowZoomControlInitialise(window); /* Sets min/max/zf */

  /* If user specified a max window length in bases then set the canvas_maxwin_size from
   * this. */
  if (window->canvas_maxwin_bases)
    {
      double zoom_max ;
      
      zoom_max = zMapWindowGetZoomMax(window) ;
      
      window->canvas_maxwin_size = (int)((double)(window->canvas_maxwin_bases) * zoom_max) ;
    }

  window->min_coord = window->seq_start ;
  window->max_coord = window->seq_end ;
  zmapWindowSeq2CanExt(&(window->min_coord), &(window->max_coord)) ;
  zmapWindowScrollRegionTool(window, NULL, &(window->min_coord), NULL, &(window->max_coord));

  h_adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(window->scrolled_window)) ;

  foo_canvas_set_pixels_per_unit_xy(window->canvas, 1.0, zMapWindowGetZoomFactor(window)) ;

  canvas_data.window = window;
  canvas_data.canvas = window->canvas;


  /* Draw the scale bar. */
#ifdef RDS_DONT_INCLUDE
  //foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(foo_canvas_root(window->canvas)), &x1, &y1, &x2, &y2) ;
  //foo_canvas_get_scroll_region(window->canvas, NULL, &y1, NULL, &y2);
#endif 
  zmapWindowDrawScaleBar(window, window->min_coord, window->max_coord);


  /* Make sure we start drawing alignments to the right of the scale bar. */
  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(foo_canvas_root(window->canvas)), &x1, &y1, &x2, &y2) ;
  window->alignment_start = x2 + COLUMN_SPACING ;


  /* Add a background to the root window, must be as long as entire sequence... */
  root_group = zmapWindowContainerCreate(foo_canvas_root(window->canvas),
					 ZMAPCONTAINER_LEVEL_ROOT,
					 ALIGN_SPACING,
					 &(canvas_data.colour_root), &(window->canvas_border)) ;
  canvas_data.curr_root_group = zmapWindowContainerGetFeatures(root_group) ;
  g_object_set_data(G_OBJECT(root_group), "item_feature_data", full_context) ;

  zmapWindowFToIAddRoot(window->context_to_item, root_group);

  start = window->seq_start ;
  end = window->seq_end ;
  zmapWindowSeq2CanExtZero(&start, &end) ;
  zmapWindowLongItemCheck(window, zmapWindowContainerGetBackground(root_group),
			  start, end) ;

  /* Set root group to start where sequence starts... */
  x = canvas_data.curr_x_offset ;
  y = full_context->sequence_to_parent.c1 ;
  foo_canvas_item_w2i(FOO_CANVAS_ITEM(foo_canvas_root(window->canvas)), &x, &y) ;

  foo_canvas_item_set(FOO_CANVAS_ITEM(root_group),
		      "y", y,
		      NULL) ;

  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(foo_canvas_root(window->canvas)), &x1, &y1, &x2, &y2) ;
  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(root_group), &x1, &y1, &x2, &y2) ;


  /* 
   *     Draw all the features, so much in so few lines...sigh...
   */
  canvas_data.curr_x_offset = window->alignment_start ;
  canvas_data.full_context = full_context ;
  drawZMap(&canvas_data, diff_context) ;


  /* Set the background object size now we have finished... */
  zmapWindowContainerSetBackgroundSize(root_group, 0.0) ;


  /* There may be a focus item if this routine is called as a result of splitting a window
   * or adding more features, make sure we scroll to the same point as we were
   * at in the previously. */
  if (window->focus_item)
    {
      zMapWindowScrollToItem(window, window->focus_item) ;
    }
  else
    {
      double scroll_x1, scroll_y1, scroll_x2, scroll_y2 ;
      /* WHAT IS THIS HERE FOR */
      foo_canvas_get_scroll_region(window->canvas, &scroll_x1, &scroll_y1, &scroll_x2, &scroll_y2) ;
      if (scroll_x1 == 0 && scroll_y1 == 0 && scroll_x2 == 0 && scroll_y2 == 0)
	foo_canvas_set_scroll_region(window->canvas,
				     0.0, window->min_coord,
				     h_adj->page_size, window->max_coord) ;
      /* Not sure what this set_scroll_region achieves, page_size is
         likely to be too small at some point (more and more
         columns) */
    }



  /* Expand the scroll region to include everything, note the hard-coded zero start, this is
   * because the actual visible window may not change when the scrolled region changes if its
   * still visible so we can end up with the visible window starting where the alignment box.
   * starts...should go away when scale moves into separate window. */  
  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(foo_canvas_root(window->canvas)), &x1, &y1, &x2, &y2) ;

  /* Expand the scroll region to include everything again as we need to include the scale bar. */  
  foo_canvas_get_scroll_region(window->canvas, NULL, &y1, NULL, &y2);
  /* There's an issue here we always seem to be calling get or set scroll region. */
  if(y1 && y2)
    //zmapWindow_set_scroll_region(window, y1, y2);
    zmapWindowScrollRegionTool(window, &x1, &y1, &x2, &y2);
  else
    //zmapWindow_set_scroll_region(window, window->min_coord, window->max_coord);
    zmapWindowScrollRegionTool(window, 
                               &x1, &(window->min_coord), 
                               &x2, &(window->max_coord));

  /* Now crop any items that are too long, actually we shouldn't need to do this here
   * because we should be showing the entire sequence... but maybe we do if we are adding to
   * an existing display.... */
  //  zmapWindowLongItemCrop(window) ;


  /* debugging.... */
#ifdef RDS_DONT_INCLUDE
  zmapWindowContainerPrint(root_group) ;
#endif

  return ;
}



/* the column_txt_grp need to exist before calling this! */
void zmapWindowColumnWriteDNA(ZMapWindow window, FooCanvasGroup *column_txt_grp)
{
  ZMapFeatureContext full_context = NULL;
  ZMapDrawTextIterator iterator   = NULL;
  PangoFontDescription *dna_font = NULL;
  double text_height = 0.0;
  double x1, x2, y1, y2;
  int i = 0;

  x1 = x2 = y1 = y2 = 0.0;
  full_context = window->feature_context;

  zmapWindowScrollRegionTool(window, &x1, &y1, &x2, &y2);

  /* should this be copied? */
  dna_font = zMapWindowGetFixedWidthFontDescription(window);
  /* zMapGUIGetFixedWidthFont(, &dna_font); */

  zmapWindowGetBorderSize(window, &text_height);

  iterator = getIterator(window->min_coord, window->max_coord,
                         y1, y2, text_height, TRUE);
  
  for(i = 0; i < iterator->rows; i++)
    {
      FooCanvasItem *item = NULL;
      iterator->iteration = i;

      if((item = zMapDrawRowOfText(column_txt_grp, 
                                   dna_font, 
                                   full_context->sequence->sequence, 
                                   iterator)))
        g_signal_connect(G_OBJECT(item), "event",
                         G_CALLBACK(dnaItemEventCB), (gpointer)window);      
    }

  destroyIterator(iterator);

  return ;
}


/************************ internal functions **************************************/


/* NOTE THAT WHEN WE COME TO MERGE DATA FROM SEVERAL SOURCES WE ARE GOING TO HAVE TO
 * CHECK AT EACH STAGE WHETHER A PARTICULAR ALIGN, BLOCK OR SET ALREADY EXISTS... */


static void drawZMap(ZMapCanvasData canvas_data, ZMapFeatureContext diff_context)
{

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if (!canvas_data->window->alignments)
    canvas_data->window->alignments = diff_context->alignments ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  g_datalist_foreach(&(diff_context->alignments), drawAlignments, canvas_data) ;


  /* Reorder list of alignments based on their position. */
  zmapWindowCanvasGroupChildSort(foo_canvas_root(canvas_data->window->canvas)) ;


  return ;
}


/* Draw all the alignments in a context, one of these is special in that it is the master
 * sequence that all the other alignments are aligned to. Commonly zmap will only have
 * the master alignment for many users as they will just view the features on a single
 * sequence. */
static void drawAlignments(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeatureAlignment alignment = (ZMapFeatureAlignment)data ;
  ZMapCanvasData canvas_data = (ZMapCanvasData)user_data ;
  ZMapWindow window = canvas_data->window ;
  double x1, y1, x2, y2 ;
  double x, y ;
  gboolean status ;
  FooCanvasGroup *align_parent ;


  canvas_data->curr_alignment = alignment ;

  /* THIS MUST GO.... */
  /* Always reset the aligns to start at y = 0. */
  canvas_data->curr_y_offset = 0.0 ;


  x = canvas_data->curr_x_offset ;
  y = canvas_data->full_context->sequence_to_parent.c1 ;
  foo_canvas_item_w2i(FOO_CANVAS_ITEM(canvas_data->curr_root_group), &x, &y) ;

  x = canvas_data->curr_x_offset ;
  y = canvas_data->full_context->sequence_to_parent.c1 ;
  my_foo_canvas_item_w2i(FOO_CANVAS_ITEM(canvas_data->curr_root_group), &x, &y) ;

  align_parent = zmapWindowContainerCreate(canvas_data->curr_root_group,
					   ZMAPCONTAINER_LEVEL_ALIGN,
					   0.0,		    /* There is no spacing between blocks. */
					   &(canvas_data->colour_alignment),
					   &(window->canvas_border)) ;
  canvas_data->curr_align_group = zmapWindowContainerGetFeatures(align_parent) ;

  foo_canvas_item_set(FOO_CANVAS_ITEM(align_parent),
		      "x", x,
		      "y", y,
		      NULL) ;

  g_object_set_data(G_OBJECT(align_parent), "item_feature_data", alignment) ;

  status = zmapWindowFToIAddAlign(window->context_to_item, key_id, align_parent) ;


  /* Do all the blocks within the alignment. */
  g_list_foreach(alignment->blocks, drawBlocks, canvas_data) ;


  /* We should have a standard routine to do this.... */

  /* We find out how wide the alignment ended up being after we drew all the blocks/columns
   * and then we draw the next alignment to the right of this one. */
  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(align_parent), &x1, &y1, &x2, &y2) ;

  canvas_data->curr_x_offset = x2 + ALIGN_SPACING ;	    /* Must come before we reset x2 below. */


  /* Set the background object size for the align now we have finished drawing... */
  zmapWindowContainerSetBackgroundSize(align_parent, 0.0) ;


  /* N.B. no need to sort blocks as they are vertically aligned. */

  return ;
}


/* Draw all the blocks within an alignment, these will vertically one below another and positioned
 * according to their alignment on the master sequence. If we are drawing the master sequence,
 * the code assumes this is represented by a single block that spans the entire master alignment. */
static void drawBlocks(gpointer data, gpointer user_data)
{
  ZMapFeatureBlock block = (ZMapFeatureBlock)data ;
  ZMapCanvasData canvas_data = (ZMapCanvasData)user_data ;
  gboolean status ;
  double x1, y1, x2, y2 ;
  GdkColor *for_bg_colour, *rev_bg_colour ;
  double top, bottom ;
  FooCanvasGroup *block_parent, *forward_group, *reverse_group ;
  double x, y ;

  /* not needed now...????? I think these can go now.... */
  top    = block->block_to_sequence.t1 ;
  bottom = block->block_to_sequence.t2 ;
  zmapWindowSeq2CanExtZero(&top, &bottom) ;


  canvas_data->curr_block = block ;

  /* Always set y offset to be top of current block. */
  canvas_data->curr_y_offset = block->block_to_sequence.t1 ;


  /* Add a group for the block and groups for the forward and reverse columns. */
  x = 0.0 ;
  y = block->block_to_sequence.t1 ;
  foo_canvas_item_w2i(FOO_CANVAS_ITEM(canvas_data->curr_align_group), &x, &y) ;


  x = 0.0 ;
  y = block->block_to_sequence.t1 ;
  my_foo_canvas_item_w2i(FOO_CANVAS_ITEM(canvas_data->curr_align_group), &x, &y) ;


  block_parent = zmapWindowContainerCreate(canvas_data->curr_align_group,
					   ZMAPCONTAINER_LEVEL_BLOCK,
					   STRAND_SPACING,
					   &(canvas_data->colour_block),
					   &(canvas_data->window->canvas_border)) ;
  canvas_data->curr_block_group = zmapWindowContainerGetFeatures(block_parent) ;
  zmapWindowLongItemCheck(canvas_data->window, zmapWindowContainerGetBackground(block_parent),
			  top, bottom) ;

  foo_canvas_item_set(FOO_CANVAS_ITEM(block_parent),
		      "y", y,
		      NULL) ;

  g_object_set_data(G_OBJECT(block_parent), "item_feature_data", block) ;


  /* Add this block to our hash for going from the feature context to its on screen item. */
  status = zmapWindowFToIAddBlock(canvas_data->window->context_to_item,
				  block->parent->unique_id, block->unique_id,
				  block_parent) ;



  /* Add a group each to hold forward and reverse strand columns. */
  if (canvas_data->curr_alignment == canvas_data->full_context->master_align)
    {
      for_bg_colour = &(canvas_data->colour_mblock_for) ;
      rev_bg_colour = &(canvas_data->colour_mblock_rev) ;
    }
  else
    {
      for_bg_colour = &(canvas_data->colour_qblock_for) ;
      rev_bg_colour = &(canvas_data->colour_qblock_rev) ;
    }

  forward_group = zmapWindowContainerCreate(canvas_data->curr_block_group,
					    ZMAPCONTAINER_LEVEL_STRAND,
					    COLUMN_SPACING,
					    for_bg_colour, &(canvas_data->window->canvas_border)) ;
  canvas_data->curr_forward_group = zmapWindowContainerGetFeatures(forward_group) ;
  zmapWindowLongItemCheck(canvas_data->window, zmapWindowContainerGetBackground(forward_group),
			  top, bottom) ;

  reverse_group = zmapWindowContainerCreate(canvas_data->curr_block_group,
					    ZMAPCONTAINER_LEVEL_STRAND,
					    COLUMN_SPACING,
					    rev_bg_colour, &(canvas_data->window->canvas_border)) ;
  canvas_data->curr_reverse_group = zmapWindowContainerGetFeatures(reverse_group) ;
  zmapWindowLongItemCheck(canvas_data->window, zmapWindowContainerGetBackground(reverse_group),
			  top, bottom) ;


  /* Add _all_ the types cols for the block, at this stage we don't know if they have features
   * and also the user may want them displayed even if empty.... */
  g_list_foreach(canvas_data->full_context->feature_set_names, createSetColumn, canvas_data) ;


  /* Now draw all features within each column, note that this operates on the feature context
   * so is called only for feature sets that contain features. */
  g_datalist_foreach(&(block->feature_sets), ProcessFeatureSet, canvas_data) ;
  
  /* Optionally remove all empty columns. */
  if (!(canvas_data->window->keep_empty_cols))
    {
      removeEmptyColumns(canvas_data) ;
    }

  /* THIS SHOULD BE WRITTEN AS A GENERAL FUNCTION TO POSITION STUFF, IN FACT WE WILL NEED
   *  A FUNCTION THAT REPOSITIONS EVERYTHING....... */
  /* Now we must position all the columns as some columns will have been bumped. */
  positionColumns(canvas_data) ;

  /* Now we've positioned all the columns we can set the backgrounds for the forward and
   * reverse strand groups and also set their positions within the block. */
  zmapWindowContainerSetBackgroundSize(forward_group, 0.0) ;
  zmapWindowContainerSetBackgroundSize(reverse_group, 0.0) ;

  /* Here we need to position the forward/reverse column groups by taking their size and
   * resetting their positions....should we have a reverse group if there are no reverse cols ? */

  x1 = 0.0 ;
  my_foo_canvas_item_goto(FOO_CANVAS_ITEM(reverse_group), &x1, NULL) ;

  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(reverse_group), &x1, &y1, &x2, &y2) ;

  x2 = x2 + STRAND_SPACING ;
  my_foo_canvas_item_goto(FOO_CANVAS_ITEM(forward_group), &x2, NULL) ;


  /* Set the size of background for the block now everything is positioned. */
  zmapWindowContainerSetBackgroundSize(block_parent, 0.0) ;

  /* Sort lists of children in block and column groups by their position.... */
  zmapWindowCanvasGroupChildSort(canvas_data->curr_block_group) ;

  zmapWindowCanvasGroupChildSort(canvas_data->curr_forward_group) ;

  zmapWindowCanvasGroupChildSort(canvas_data->curr_reverse_group) ;

  return ;
}



/* Makes a column for each style in the list, the columns are populated with features by the 
 * ProcessFeatureSet() routine, at this stage the columns do not have any features and some
 * columns may end up not having any features at all. */
static void createSetColumn(gpointer data, gpointer user_data)
{
  GQuark feature_set_id = GPOINTER_TO_UINT(data) ;
  ZMapCanvasData canvas_data  = (ZMapCanvasData)user_data ;
  ZMapWindow window = canvas_data->window ;
  /*  ZMapFeatureBlock block = canvas_data->curr_block ; */
  /* GQuark type_quark ; */
  /*  double x1, y1, x2, y2 ; */
  /* FooCanvasItem *group ;*/
  double top, bottom ;
  gboolean status ;
  FooCanvasGroup *forward_col, *reverse_col ;
  GdkColor *for_bg_colour, *rev_bg_colour ;
  ZMapFeatureContext context = window->feature_context ;
  ZMapFeatureTypeStyle style ;


  /* Look up the overall column style... */
  style = zMapFindStyle(context->styles, feature_set_id) ;
  zMapAssert(style) ;


  /* Add a background colouring for the column. */
  if (canvas_data->curr_alignment == canvas_data->full_context->master_align)
    {
      for_bg_colour = &(canvas_data->colour_mforward_col) ;
      rev_bg_colour = &(canvas_data->colour_mreverse_col) ;
    }
  else
    {
      for_bg_colour = &(canvas_data->colour_qforward_col) ;
      rev_bg_colour = &(canvas_data->colour_qreverse_col) ;
    }


  /* We need the background column object to span the entire bottom of the alignment block. */
  top = canvas_data->curr_block->block_to_sequence.t1 ;
  bottom = canvas_data->curr_block->block_to_sequence.t2 ;
  zmapWindowSeq2CanExtZero(&top, &bottom) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  {
    double x = canvas_data->curr_block->block_to_sequence.t1,
      y = canvas_data->curr_block->block_to_sequence.t2 ;

    foo_canvas_item_w2i(FOO_CANVAS_ITEM(canvas_data->curr_root_group), &x, &y) ;
    my_foo_canvas_item_w2i(FOO_CANVAS_ITEM(canvas_data->curr_root_group), &x, &y) ;

  }
#endif

  forward_col = createColumn(FOO_CANVAS_GROUP(canvas_data->curr_forward_group),
			     window,
			     style,
			     ZMAPSTRAND_FORWARD,
			     style->width,
			     top, bottom,
			     for_bg_colour) ;


  canvas_data->curr_forward_col_id = zmapWindowFToIMakeSetID(feature_set_id, ZMAPSTRAND_FORWARD) ;

  status = zmapWindowFToIAddSet(window->context_to_item,
				canvas_data->curr_alignment->unique_id,
				canvas_data->curr_block->unique_id,
				feature_set_id,
				ZMAPSTRAND_FORWARD,
				forward_col) ;
  zMapAssert(status) ;


  canvas_data->curr_forward_col = zmapWindowContainerGetFeatures(forward_col) ;



  /* Our default action is to show features on the "forward" strand and _not_ on the
   * reverse strand, the style must have show_rev_strand == TRUE for the col. to be
   * shown on the reverse strand, SO WE HAVE A PROBLEM HERE IN THAT WE DON'T YET KNOW WHAT.
   * THE STYLE SAYS... */

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if (style->strand_specific && style->show_rev_strand == FALSE)
    {
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

      reverse_col = createColumn(FOO_CANVAS_GROUP(canvas_data->curr_reverse_group),
				 window,
				 style,
				 ZMAPSTRAND_REVERSE,
				 style->width,
				 top, bottom,
				 rev_bg_colour) ;

      /* We can't set the "item_feature_data" as we don't have the feature set at this point.
       * This probably points to some muckiness in the code, problem is caused by us deciding
       * to display all columns whether they have features or not and so some columns may not
       * have feature sets. */

      canvas_data->curr_reverse_col_id = zmapWindowFToIMakeSetID(feature_set_id, ZMAPSTRAND_REVERSE) ;
      status = zmapWindowFToIAddSet(window->context_to_item,
				    canvas_data->curr_alignment->unique_id,
				    canvas_data->curr_block->unique_id,
				    feature_set_id,
				    ZMAPSTRAND_REVERSE,
				    reverse_col) ;
      zMapAssert(status) ;

      canvas_data->curr_reverse_col = zmapWindowContainerGetFeatures(reverse_col) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  if (forward_col)
    zmapWindowColumnSetMagState(window, forward_col, style) ;

  if (reverse_col)
    zmapWindowColumnSetMagState(window, reverse_col, style) ;

  return ;
}


/* Create an individual column group, this will have the feature items added to it. */
static FooCanvasGroup *createColumn(FooCanvasGroup *parent_group,
				    ZMapWindow window,
				    ZMapFeatureTypeStyle style,
				    ZMapStrand strand,
				    double width,
				    double top, double bot,
				    GdkColor *colour)
{
  FooCanvasGroup *group ;
  FooCanvasItem *bounding_box ;
  double x1, x2, y1, y2 ;


  group = zmapWindowContainerCreate(parent_group, ZMAPCONTAINER_LEVEL_FEATURESET,
				    0.0,		    /* no spacing between features. */
				    colour, &(window->canvas_border)) ;

  /* By default we do not redraw our children which are the individual features, the canvas
   * should do this for us. */
  zmapWindowContainerSetChildRedrawRequired(group, FALSE) ;

  /* Make sure group covers whole span in y direction. */
  zmapWindowContainerSetBackgroundSize(group, bot) ;

  bounding_box = zmapWindowContainerGetBackground(group) ;
  zmapWindowLongItemCheck(window, bounding_box, top, bot) ;


  /* Ugh...what is this code.....??? should be done by a container call.... */
  foo_canvas_item_get_bounds(bounding_box, &x1, &y1, &x2, &y2) ;
  if (x2 == 0)
    foo_canvas_item_set(bounding_box,
			"x2", width,
			NULL) ;


  /* We can't set the "item_feature_data" as we don't have the feature set at this point.
   * This probably points to some muckiness in the code, problem is caused by us deciding
   * to display all columns whether they have features or not and so some columns may not
   * have feature sets. */
  g_object_set_data(G_OBJECT(group), "item_feature_strand",
		    GINT_TO_POINTER(strand)) ;
  g_object_set_data(G_OBJECT(group), "item_feature_style",
		    GINT_TO_POINTER(style)) ;

  g_signal_connect(G_OBJECT(group), "event",
		   G_CALLBACK(columnBoundingBoxEventCB), (gpointer)window) ;

  return group ;
}




/* Called for each feature set, it then calls a routine to draw each of its features.  */
static void ProcessFeatureSet(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeatureSet feature_set = (ZMapFeatureSet)data ;
  ZMapCanvasData canvas_data  = (ZMapCanvasData)user_data ;
  ZMapWindow window = canvas_data->window ;
  GQuark type_quark ;
  double x1, y1, x2, y2 ;
  FooCanvasGroup *forward_col, *reverse_col ;


  canvas_data->curr_set = feature_set ;

  /* Each column is known by its type/style name. */
  type_quark = feature_set->unique_id ;
  zMapAssert(type_quark == key_id) ;			    /* sanity check. */


  /* UM, I THINK I SHOULD REVISIT THIS...SEEMS BAD TO HARD CODE THAT WE SHOULD _ALWAYS_ HAVE A
   * FORWARD COLUMN........ */
  /* Get hold of the current column and set up canvas_data to do the right thing,
   * remember that there must always be a forward column but that there might
   * not be a reverse column ! */
  forward_col = FOO_CANVAS_GROUP(zmapWindowFToIFindSetItem(window->context_to_item,
							   feature_set, ZMAPSTRAND_FORWARD)) ;
  zMapAssert(forward_col) ;



  /* Now we have the feature set, make sure it is set for the column. */
  g_object_set_data(G_OBJECT(forward_col), "item_feature_data", feature_set) ;


  canvas_data->curr_forward_col = zmapWindowContainerGetFeatures(forward_col) ;

  canvas_data->curr_forward_col_id = zmapWindowFToIMakeSetID(feature_set->unique_id,
							     ZMAPSTRAND_FORWARD) ;


  if ((reverse_col = FOO_CANVAS_GROUP(zmapWindowFToIFindSetItem(window->context_to_item,
								feature_set, ZMAPSTRAND_REVERSE))))
    {
      /* Now we have the feature set, make sure it is set for the column. */
      g_object_set_data(G_OBJECT(reverse_col), "item_feature_data", feature_set) ;

      canvas_data->curr_reverse_col = zmapWindowContainerGetFeatures(reverse_col) ;

      canvas_data->curr_reverse_col_id = zmapWindowFToIMakeSetID(feature_set->unique_id,
								 ZMAPSTRAND_REVERSE) ;
    }


  /* Now draw all the features in the column. */
  g_datalist_foreach(&(feature_set->features), ProcessFeature, canvas_data) ;


  /* TRY RESIZING BACKGROUND NOW..... */
  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(forward_col), &x1, &y1, &x2, &y2) ;
  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(zmapWindowContainerGetFeatures(forward_col)),
			     &x1, &y1, &x2, &y2) ;
  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(zmapWindowContainerGetBackground(forward_col)),
			     &x1, &y1, &x2, &y2) ;

  zmapWindowContainerMaximiseBackground(forward_col) ;

  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(forward_col), &x1, &y1, &x2, &y2) ;
  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(zmapWindowContainerGetFeatures(forward_col)),
			     &x1, &y1, &x2, &y2) ;
  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(zmapWindowContainerGetBackground(forward_col)),
			     &x1, &y1, &x2, &y2) ;
  
  if (reverse_col)
    zmapWindowContainerMaximiseBackground(reverse_col) ;

  return ;
}


/* Called to draw each individual feature. */
static void ProcessFeature(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeature feature = (ZMapFeature)data ; 
  ZMapFeatureTypeStyle style = feature->style ;
  ZMapCanvasData canvas_data  = (ZMapCanvasDataStruct*)user_data ;
  ZMapWindow window = canvas_data->window ;
  FooCanvasGroup *column_parent, *column_group ;
  GQuark column_id ;
  FooCanvasItem *top_feature_item = NULL ;
  double feature_offset;
  GdkColor *background ;
  double start_x, end_x ;


  /* Users will often not want to see what is on the reverse strand, style specifies what should
   * be shown. */
  if (style->strand_specific
      && (feature->strand == ZMAPSTRAND_REVERSE && style->show_rev_strand == FALSE))
    {
      return ;
    }

  /* Set colours... */
  if (canvas_data->curr_alignment == canvas_data->full_context->master_align)
    {
      if (feature->strand == ZMAPSTRAND_REVERSE)
	background = &(canvas_data->colour_mblock_rev) ;
      else
	background = &(canvas_data->colour_mblock_for) ;
    }
  else
    {
      if (feature->strand == ZMAPSTRAND_REVERSE)
	background = &(canvas_data->colour_qblock_rev) ;
      else
	background = &(canvas_data->colour_qblock_for) ;
    }


  /* Retrieve the parent col. group/id. */
  if (feature->strand == ZMAPSTRAND_FORWARD || feature->strand == ZMAPSTRAND_NONE)
    {
      column_group = canvas_data->curr_forward_col ;
      column_id = canvas_data->curr_forward_col_id ;
    }
  else
    {
      column_group = canvas_data->curr_reverse_col ;
      column_id = canvas_data->curr_reverse_col_id ;
    }


  /* Start/end of feature within alignment block.
   * Feature position on screen is determined the relative offset of the features coordinates within
   * its align block added to the alignment block _query_ coords. You can't just use the
   * features own coordinates as these will be its coordinates in its original sequence. */

  feature_offset = canvas_data->curr_block->block_to_sequence.q1 ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* I FEEL LIKE WE SHOULD BE USING THIS HERE BUT IT IS NOT STRAIGHT FORWARD...BECAUSE OF THE
   * QUERY BLOCK ALIGNMENT ETC ETC...SO MORE WORK TO DO HERE... */

  foo_canvas_item_w2i(column_group, &unused, &feature_offset) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



  start_x = 0.0 ;
  end_x   = style->width ;

  switch (feature->type)
    {
    case ZMAPFEATURE_BASIC:
      {
	top_feature_item = drawSimpleFeature(column_group, feature,
					     feature_offset,
					     start_x, feature->x1, end_x, feature->x2,
					     &style->outline,
                                             &style->foreground,
					     &style->background,
					     window) ;
	break ;
      }
    case ZMAPFEATURE_ALIGNMENT:
      {
        if(style->align_gaps)
          top_feature_item = drawAlignmentFeature(column_group, feature,
                                                  feature_offset,
                                                  start_x, feature->x1, end_x, feature->x2,
                                                  &style->outline,
                                                  &style->foreground,
                                                  &style->background,
                                                  window);
        else
          top_feature_item = drawSimpleFeature(column_group, feature,
                                               feature_offset,
                                               start_x, feature->x1, end_x, feature->x2,
                                               &style->outline,
                                               &style->foreground,
                                               &style->background,
                                               window) ;
        break;
      }
    case ZMAPFEATURE_RAW_SEQUENCE:
      {
	/* Surely this must be true.... */
	zMapAssert(feature->feature.sequence.sequence) ;

	/* Flag our parent container that it has children that need redrawing on zoom. */
	column_parent = zmapWindowContainerGetParent(FOO_CANVAS_ITEM(column_group)) ;
	zmapWindowContainerSetChildRedrawRequired(column_parent, TRUE) ;

	top_feature_item = drawSequenceFeature(column_group, feature,
                                               feature_offset,
                                               start_x, feature->x1, end_x, feature->x2,
                                               &style->outline,
                                               &style->background,
                                               window) ;

        break ;
      }
    case ZMAPFEATURE_TRANSCRIPT:
      {
	top_feature_item = drawTranscriptFeature(column_group, feature,
						 feature_offset,
						 start_x, 0.0, end_x, 0.0,
						 &style->outline,
                                                 &style->foreground,
						 &style->background,
						 window) ;
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	zmapWindowPrintGroup(FOO_CANVAS_GROUP(feature_group)) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	break ;
      }
    default:
      zMapLogFatal("Feature %s is of unknown type: %d\n", 
		   (char *)g_quark_to_string(feature->original_id), feature->type) ;
      break;
    }


  /* If we created an object then set up a hash that allows us to go unambiguously from
   * (feature, style) -> feature_item. */
  if (top_feature_item)
    {
      gboolean status ;
      
      status = zmapWindowFToIAddFeature(window->context_to_item,
					canvas_data->curr_alignment->unique_id,
					canvas_data->curr_block->unique_id, 
					column_id,
					feature->unique_id, top_feature_item) ;
      zMapAssert(status) ;
    }


  return ;
}



/* Removes a column which has no features (the default action). */
static void removeEmptyColumns(ZMapCanvasData canvas_data)
{
#ifdef RDS_DONT_INCLUDE
  ZMapWindow window = canvas_data->window ;
#endif
  RemoveEmptyColumnStruct remove_data ;

  remove_data.canvas_data = canvas_data ;

  remove_data.strand = ZMAPSTRAND_FORWARD ;
  g_list_foreach(canvas_data->curr_forward_group->item_list, removeEmptyColumnCB, &remove_data) ;

  remove_data.strand = ZMAPSTRAND_REVERSE ;
  g_list_foreach(canvas_data->curr_reverse_group->item_list, removeEmptyColumnCB, &remove_data) ;

  return ;
}


/* Note how we must look at the items drawn, _not_ whether there are any features in the feature
 * set because the features may not be drawn (e.g. because they are on the opposite strand. */
static void removeEmptyColumnCB(gpointer data, gpointer user_data)
{
  FooCanvasGroup *container = (FooCanvasGroup *)data ;


  if (!zmapWindowContainerHasFeatures(container))
    {
      RemoveEmptyColumn remove_data = (RemoveEmptyColumn)user_data ;
      ZMapCanvasData canvas_data = remove_data->canvas_data ;
      ZMapFeatureTypeStyle style ;
      gboolean status ;


      /* Remove this item from the hash of features -> items */
      style = g_object_get_data(G_OBJECT(container), "item_feature_style") ;

      status = zmapWindowFToIRemoveSet(canvas_data->window->context_to_item,
				       canvas_data->curr_alignment->unique_id,
				       canvas_data->curr_block->unique_id,
				       style->unique_id,
				       remove_data->strand) ;
      zMapAssert(status) ;

      /* Now remove from Long Items (if it was there in the first place... */
      zmapWindowLongItemRemove(&(canvas_data->window->long_items),
			       zmapWindowContainerGetBackground(container)) ;


      /* Now destroy the items themselves. */
      zmapWindowContainerDestroy(container) ;
    }

  return ;
}

FooCanvasItem *zmapWindowDrawFeatureParent(FooCanvasGroup *parent,
                                           ZMapFeature feature,
                                           double y_origin)
{
  FooCanvasItem *grp = NULL;
  /* this group is the parent of the entire transcript. */
  grp = foo_canvas_item_new(FOO_CANVAS_GROUP(parent),
                            foo_canvas_group_get_type(),
                            "y", y_origin,
                            NULL) ;
  grp = FOO_CANVAS_ITEM(grp) ;

  /* we probably need to set this I think, it means we will go to the group when we
   * navigate or whatever.... */
  g_object_set_data(G_OBJECT(grp), "item_feature_type",
                    GINT_TO_POINTER(ITEM_FEATURE_PARENT)) ;
  g_object_set_data(G_OBJECT(grp), "item_feature_data", feature) ;

  return grp;
}

/* LONG ITEMS SHOULD BE INCORPORATED IN THIS LOT.... */
/* some drawing functions, may want these to be externally visible later... */

static FooCanvasItem *drawSimpleFeature(FooCanvasGroup *parent, ZMapFeature feature,
					double feature_offset,
					double x1, double y1, double x2, double y2,
					GdkColor *outline, 
                                        GdkColor *foreground,
                                        GdkColor *background,
					ZMapWindow window)
{
  FooCanvasItem *feature_item ;

  if(featureClipItemToDraw(feature, &y1, &y2))
    return NULL;

  zmapWindowSeq2CanOffset(&y1, &y2, feature_offset) ;
  feature_item = zMapDrawBox(FOO_CANVAS_ITEM(parent),
                             x1, y1, x2, y2,
                             outline, background) ;
  attachDataToItem(feature_item, window, feature, ITEM_FEATURE_SIMPLE, NULL);
  zmapWindowLongItemCheck(window, feature_item, y1, y2) ;

  return feature_item ;
}


static FooCanvasItem *drawSequenceFeature(FooCanvasGroup *parent, ZMapFeature feature,
                                          double feature_offset,
                                          double feature_zero,      double feature_top,
                                          double feature_thickness, double feature_bottom,
                                          GdkColor *outline, GdkColor *background,
                                          ZMapWindow window)
{
  FooCanvasItem *text_feature ;

  /* I WOULD HAVE THOUGHT THAT WE NEEDED TO BE MAKING MORE USE OF COLOURS ETC HERE.... */


  /* Note that the sequence item is different in that there is a single group child for the
   * entire dna rows of text.... */

  text_feature = foo_canvas_item_new(parent,
				     foo_canvas_group_get_type(),
				     NULL) ;
  g_object_set_data(G_OBJECT(text_feature), CONTAINER_REDRAW_CALLBACK,
		    dna_redraw_callback) ;
  g_object_set_data(G_OBJECT(text_feature), CONTAINER_REDRAW_DATA,
		    (gpointer)window) ;

  zmapWindowColumnWriteDNA(window, FOO_CANVAS_GROUP(text_feature)) ;


  return text_feature ;
}


static FooCanvasItem *drawAlignmentFeature(FooCanvasGroup *parent, ZMapFeature feature,
                                           double feature_offset,
                                           double x1, double feature_top,
                                           double x2, double feature_bottom,
                                           GdkColor *outline, 
                                           GdkColor *foreground,
                                           GdkColor *background,
                                           ZMapWindow window) 
{
  FooCanvasItem *feature_item ;
  double offset ;
  int i ;

  if (!feature->feature.homol.align)
    feature_item = drawSimpleFeature(parent, feature, feature_offset,
                                     x1, feature_top, x2, feature_bottom,
                                     outline, foreground, background, window) ;
  else if(feature->feature.homol.align)
    {
      float line_width  = 1.5 ;
      double feature_start, feature_end;
      FooCanvasItem *lastBoxWeDrew   = NULL;
      FooCanvasGroup *feature_group  = NULL;
      ZMapAlignBlock prev_align_span = NULL;

      feature_start = feature->x1;
      feature_end   = feature->x2; /* I want this function to find these values */
      zmapWindowSeq2CanOffset(&feature_start, &feature_end, feature_offset) ;
      feature_item  = zmapWindowDrawFeatureParent(parent, feature, feature_start);
      feature_group = FOO_CANVAS_GROUP(feature_item);

      /* Calculate total offset for for subparts of the feature. */
      offset = feature_start + feature_offset ;

      for (i = 0 ; i < feature->feature.homol.align->len ; i++)  
        {
          ZMapAlignBlock align_span;
          FooCanvasItem *align_box, *gap_line, *gap_line_box;
          double top, bottom, dummy = 0.0;
          ZMapWindowItemFeature box_data, gap_data, align_data ;
          
          align_span = &g_array_index(feature->feature.homol.align, ZMapAlignBlockStruct, i) ;   
          top    = align_span->t1;
          bottom = align_span->t2;

          if(featureClipItemToDraw(feature, &top, &bottom))
            continue;

          align_data        = g_new0(ZMapWindowItemFeatureStruct, 1) ;
          align_data->start = align_span->t1 ;
          align_data->end   = align_span->t2 ;

          zmapWindowSeq2CanOffset(&top, &bottom, offset) ;

          if(prev_align_span)
            {
              int q_indel, t_indel;
              t_indel = align_span->t1 - prev_align_span->t2;
              /* For query the current and previous can be reversed
               * wrt target which is _always_ consecutive. */
              if((align_span->t_strand == ZMAPSTRAND_REVERSE && 
                  align_span->q_strand == ZMAPSTRAND_FORWARD) || 
                 (align_span->t_strand == ZMAPSTRAND_FORWARD &&
                  align_span->q_strand == ZMAPSTRAND_REVERSE))
                q_indel = prev_align_span->q1 - align_span->q2;
              else
                q_indel = align_span->q1 - prev_align_span->q2;
              
#ifdef RDS_THESE_NEED_A_LITTLE_BIT_OF_CHECKING
              /* I'm not 100% sure on some of this. */
                {
                  if(align_span->q_strand == ZMAPSTRAND_REVERSE)
                    printf("QR TF Find %s\n", g_quark_to_string(feature->unique_id));
                  else
                    printf("QF TR Find %s\n", g_quark_to_string(feature->unique_id));
                }
                {               /* Especially REVERSE REVERSE. */
                  if(align_span->t_strand == ZMAPSTRAND_REVERSE &&
                     align_span->q_strand == ZMAPSTRAND_REVERSE)
                    printf("QR TR Find %s\n", g_quark_to_string(feature->unique_id));
                }
#endif /* RDS_THESE_NEED_A_LITTLE_BIT_OF_CHECKING */

              if(q_indel >= t_indel) /* insertion in query, expand align and annotate the insertion */
                {
                  zMapDrawAnnotatePolygon(lastBoxWeDrew,
                                          ZMAP_ANNOTATE_EXTEND_ALIGN, 
                                          NULL,
                                          foreground,
                                          bottom,
                                          feature->strand);
                  /* we need to check here that the item hasn't been
                   * expanded to be too long! This will involve
                   * updating the rather than inserting as the check
                   * code currently does */
                  /* Really I want to display the correct width in the target here.
                   * Not sure on it's effect re the cap/join style.... 
                   * t_indel may always be 1, but possible that this is not the case, 
                   * code allows it not to be.
                   */
                }
              else
                {
                  lastBoxWeDrew = 
                    align_box = zMapDrawSSPolygon(feature_item, ZMAP_POLYGON_SQUARE,
                                                  x1, x2,
                                                  top, bottom,
                                                  outline, background,
                                                  feature->strand);
                  zmapWindowLongItemCheck(window, align_box, top, bottom);              
                  attachDataToItem(align_box, window, feature, ITEM_FEATURE_CHILD, align_data);

                  box_data        = g_new0(ZMapWindowItemFeatureStruct, 1) ;
                  box_data->start = align_span->t1 ;
                  box_data->end   = prev_align_span->t2 ;
                  gap_data        = g_new0(ZMapWindowItemFeatureStruct, 1) ;
                  gap_data->start = align_span->t1 ;
                  gap_data->end   = prev_align_span->t2 ;

                  bottom = prev_align_span->t2;
                  zmapWindowSeq2CanOffset(&dummy, &bottom, offset);
                  gap_line_box = zMapDrawSSPolygon(feature_item, ZMAP_POLYGON_SQUARE,
                                                   x1, x2,
                                                   bottom, top,
                                                   NULL,NULL,
                                                   feature->strand);
                  attachDataToItem(gap_line_box, window, feature, ITEM_FEATURE_BOUNDING_BOX, box_data);
                  zmapWindowLongItemCheck(window, gap_line_box, bottom, top);              
                  gap_line = zMapDrawAnnotatePolygon(gap_line_box,
                                                     ZMAP_ANNOTATE_GAP, 
                                                     NULL,
                                                     background,
                                                     line_width,
                                                     feature->strand);
                  attachDataToItem(gap_line, window, feature, ITEM_FEATURE_CHILD, gap_data);
                  zmapWindowLongItemCheck(window, gap_line, bottom, top);
                  foo_canvas_item_lower(gap_line, 2);
                  gap_data->twin_item = gap_line_box;
                  box_data->twin_item = gap_line;
                }
            }
          else
            {
              /* Draw the align match box */
              lastBoxWeDrew =
                align_box = zMapDrawSSPolygon(feature_item, ZMAP_POLYGON_SQUARE,
                                              x1, x2,
                                              top, bottom,
                                              outline, background,
                                              feature->strand);
              zmapWindowLongItemCheck(window, align_box, top, bottom) ;
              attachDataToItem(align_box, window, feature, ITEM_FEATURE_CHILD, align_data);
              /* Finished with the align match */

            }
          prev_align_span = align_span;
        }

    }
    
  return feature_item ;
}


	/* Note that for transcripts the boxes and lines are contained in a canvas group
	 * and that therefore their coords are relative to the start of the group which
	 * is the start of the transcript, i.e. feature->x1. */

static FooCanvasItem *drawTranscriptFeature(FooCanvasGroup *parent, ZMapFeature feature,
					    double feature_offset,
					    double x1, double feature_top,
					    double x2, double feature_bottom,
					    GdkColor *outline, 
                                            GdkColor *foreground,
                                            GdkColor *background,
					    ZMapWindow window)
{
  FooCanvasItem *feature_item ;
  int i ;
  double offset ;
  
  /* If there are no exons/introns then just draw a simple box. Can happen for putative
   * transcripts or perhaps where user has a single exon object but does not give an exon,
   * only an overall extent. */
  if (!feature->feature.transcript.introns && !feature->feature.transcript.exons)
    {
      feature_item = drawSimpleFeature(parent, feature, feature_offset,
				       x1, feature_top, x2, feature_bottom,
				       outline, foreground, background, window) ;
    }
  else
    {
      double feature_start, feature_end;
      double cds_start = 0.0, cds_end = 0.0; /* cds_start < cds_end */
      GdkColor *cds_bg = background;
      FooCanvasGroup *feature_group ;

      feature_start = feature->x1;
      feature_end   = feature->x2; /* I want this function to find these values */
      
      zmapWindowSeq2CanOffset(&feature_start, &feature_end, feature_offset) ;

      feature_item = zmapWindowDrawFeatureParent(parent, feature, feature_start);
      feature_group = FOO_CANVAS_GROUP(feature_item);
      /* Calculate total offset for for subparts of the feature. */
      /* We could look @ foo_canavs_group->ypos, but locks us 2 the vertical! */
      offset = feature_start + feature_offset ;

      if(feature->feature.transcript.flags.cds)
        {
          cds_start = feature->feature.transcript.cds_start;
          cds_end   = feature->feature.transcript.cds_end;
          zMapAssert(cds_start < cds_end);
          cds_start -= offset;
          cds_end   -= offset;
        }
      
      /* first we draw the introns, then the exons.  Introns will have an invisible
       * box around them to allow the user to click there and get a reaction.
       * It's a bit hacky but the bounding box has the same coords stored on it as
       * the intron...this needs tidying up because its error prone, better if they
       * had a surrounding group.....e.g. what happens when we free ?? */
      if (feature->feature.transcript.introns)
	{
          float line_width = 1.5 ;
	  for (i = 0 ; i < feature->feature.transcript.introns->len ; i++)  
	    {
	      FooCanvasItem *intron_box, *intron_line ;
	      double left, right, top, bottom ;
	      ZMapWindowItemFeature box_data, intron_data ;
	      ZMapSpan intron_span ;

	      intron_span = &g_array_index(feature->feature.transcript.introns, ZMapSpanStruct, i) ;

	      left   = x1;
	      right  = x2;
              top    = intron_span->x1;
              bottom = intron_span->x2;

              if(featureClipItemToDraw(feature, &top, &bottom))
                continue;

              box_data        = g_new0(ZMapWindowItemFeatureStruct, 1) ;
              box_data->start = intron_span->x1;
              box_data->end   = intron_span->x2;
              intron_data        = g_new0(ZMapWindowItemFeatureStruct, 1) ;
              intron_data->start = intron_span->x1;
              intron_data->end   = intron_span->x2;

              zmapWindowSeq2CanOffset(&top, &bottom, offset);
              intron_box = zMapDrawSSPolygon(feature_item, 
                                             ZMAP_POLYGON_SQUARE, 
                                             left, right,
                                             top, bottom,
                                             NULL, NULL,
                                             feature->strand);
              
              attachDataToItem(intron_box, window, feature, ITEM_FEATURE_BOUNDING_BOX, box_data);
              zmapWindowLongItemCheck(window, intron_box, top, bottom);

              if(!feature->feature.transcript.flags.cds || 
                 (feature->feature.transcript.flags.cds &&
                 (cds_start > bottom)))
                cds_bg = background;  /* probably a reverse & cds_end thing here too! */
              else
                cds_bg = foreground;
              
              intron_line = zMapDrawAnnotatePolygon(intron_box,
                                                    ZMAP_ANNOTATE_INTRON, 
                                                    NULL,
                                                    cds_bg,
                                                    line_width,
                                                    feature->strand);
              attachDataToItem(intron_line, window, feature, ITEM_FEATURE_CHILD, intron_data);
              zmapWindowLongItemCheck(window, intron_line, top, bottom);              
                  
              intron_data->twin_item = intron_box;
              box_data->twin_item = intron_line;
              
            }
	}


      if (feature->feature.transcript.exons)
	{
	  for (i = 0; i < feature->feature.transcript.exons->len; i++)
	    {
	      ZMapSpan exon_span ;
	      FooCanvasItem *exon_box ;
	      ZMapWindowItemFeature exon_data ;
	      double top, bottom ;

	      exon_span = &g_array_index(feature->feature.transcript.exons, ZMapSpanStruct, i);

              top    = exon_span->x1;
              bottom = exon_span->x2;

              if(featureClipItemToDraw(feature, &top, &bottom))
                continue;

              exon_data        = g_new0(ZMapWindowItemFeatureStruct, 1) ;
              exon_data->start = exon_span->x1;
              exon_data->end   = exon_span->x2;

              zmapWindowSeq2CanOffset(&top, &bottom, offset);
              if(!feature->feature.transcript.flags.cds || 
                 (feature->feature.transcript.flags.cds &&
                 (cds_start > bottom)))
                cds_bg = background;  /* probably a reverse & cds_end thing here too! */
              else
                cds_bg = foreground;

              exon_box = zMapDrawSSPolygon(feature_item, 
                                           ZMAP_POLYGON_POINTING, 
                                           x1, x2,
                                           top, bottom,
                                           outline, cds_bg,
                                           feature->strand);
                
              attachDataToItem(exon_box, window, feature, ITEM_FEATURE_CHILD, exon_data);
              zmapWindowLongItemCheck(window, exon_box, top, bottom);

              if(feature->feature.transcript.flags.cds && 
                 (cds_start > top) && (cds_start < bottom))
                {
                  zMapDrawAnnotatePolygon(exon_box,
                                          (feature->strand == ZMAPSTRAND_REVERSE ? 
                                           ZMAP_ANNOTATE_UTR_LAST : ZMAP_ANNOTATE_UTR_FIRST),
                                          outline,
                                          background,
                                          cds_start,
                                          feature->strand);
                }
              if(feature->feature.transcript.flags.cds && 
                 (cds_end > top) && (cds_end < bottom))
                {
                  zMapDrawAnnotatePolygon(exon_box,
                                          (feature->strand == ZMAPSTRAND_REVERSE ? 
                                           ZMAP_ANNOTATE_UTR_FIRST : ZMAP_ANNOTATE_UTR_LAST),
                                          outline,
                                          background,
                                          cds_end,
                                          feature->strand);
                }

            }

	}
    }

  return feature_item ;
}

static gboolean featureClipItemToDraw(ZMapFeature feature, 
                                      double *start_inout, 
                                      double *end_inout)
{
  gboolean out_of_block  = FALSE; /* most will be within the block bounds */
  double block_start, block_end; /* coords of the block */
  int start_end_crossing = 0;
  ZMapFeatureBlock block = NULL;
  ZMapFeatureAny any     = NULL;

  zMapAssert(feature && start_inout && end_inout);

  any   = feature->parent->parent;
  zMapAssert(any && any->struct_type == ZMAPFEATURE_STRUCT_BLOCK);
  block = (ZMapFeatureBlock)any;

  /* Get block start and end */
  block_start = block->block_to_sequence.q1;
  block_end   = block->block_to_sequence.q2;

  /* shift according to how we cross, like this for ease of debugging, not speed */
  start_end_crossing |= ((*start_inout < block_start) << 1);
  start_end_crossing |= ((*end_inout   > block_end)   << 2);
  start_end_crossing |= ((*start_inout > block_end)   << 3);
  start_end_crossing |= ((*end_inout   < block_start) << 4);

  /* Now check whether we cross! */
  if(start_end_crossing & 8 || 
     start_end_crossing & 16) /* everything is out of range don't display! */
    out_of_block = TRUE;        
  
  if(start_end_crossing & 2)
    *start_inout = block_start;
  if(start_end_crossing & 4)
    *end_inout = block_end;

  return out_of_block;
}

static void attachDataToItem(FooCanvasItem *feature_item, 
                             ZMapWindow window,
                             ZMapFeature feature,
                             ZMapWindowItemFeatureType type,
                             gpointer subFeatureData)
{
  zMapAssert(feature_item && window && feature);

  g_object_set_data(G_OBJECT(feature_item), "item_feature_type",
                    GINT_TO_POINTER(type)) ;
  g_object_set_data(G_OBJECT(feature_item), "item_feature_data", feature) ;
  if(subFeatureData != NULL)
    g_object_set_data(G_OBJECT(feature_item), "item_subfeature_data",
                      subFeatureData);

  g_signal_connect(GTK_OBJECT(feature_item), "event",
                   GTK_SIGNAL_FUNC(canvasItemEventCB), window) ;
  g_signal_connect(GTK_OBJECT(feature_item), "destroy",
                   GTK_SIGNAL_FUNC(canvasItemDestroyCB), window) ;

  return ;
}



/* Positions columns within the forward/reverse strand groups. We have to do this retrospectively
 * because the width of some columns is determined at the time they are drawn and hence we don't
 * know their size until they've been drawn.
 */
static void positionColumns(ZMapCanvasData canvas_data)
{
  PositionColumnStruct pos_data ;

  pos_data.offset = 0.0 ;
  g_list_foreach(canvas_data->curr_forward_group->item_list, positionColumnCB, &pos_data) ;

  /* We want the reverse strand columns as a mirror image of the forward strand so we position
   * them backwards. */
  pos_data.offset = 0.0 ;
  zMap_g_list_foreach_directional(canvas_data->curr_reverse_group->item_list_end, 
                                  positionColumnCB, &pos_data, ZMAP_GLIST_REVERSE) ;

  return ;
}

/* Note how we must look at the items drawn, _not_ whether there are any features in the feature
 * set because the features may not be drawn (e.g. because they are on the opposite strand. */
static void positionColumnCB(gpointer data, gpointer user_data)
{
  FooCanvasGroup *container = (FooCanvasGroup *)data ;
  PositionColumn pos_data  = (PositionColumn)user_data ;
  double x1, y1, x2, y2 ;
  ZMapFeatureTypeStyle style ;

  style = zmapWindowContainerGetStyle(container) ;
  zMapAssert(style) ;

  /* Bump columns that need to be bumped. */
  if (style->overlap_mode != ZMAPOVERLAP_COMPLETE)
    zmapWindowColumnBump(container, style->overlap_mode) ;

  /* Set its x position. */
  my_foo_canvas_item_goto(FOO_CANVAS_ITEM(container), &(pos_data->offset), NULL) ;

  /* Calculate the offset of the next column from this ones width. */
  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(container), &x1, &y1, &x2, &y2) ;
  pos_data->offset = pos_data->offset + zmapWindowExt(x1, x2) + COLUMN_SPACING ;

  return ;
}





/* 
 *                           Event handlers
 */


/* Callback for destroy of items... */
static gboolean canvasItemDestroyCB(FooCanvasItem *item, GdkEvent *event, gpointer data)
{
  gboolean event_handled = FALSE ;			    /* Make sure any other callbacks also
							       get run. */
  ZMapWindowItemFeatureType item_feature_type ;
#ifdef RDS_DONT_INCLUDE
  ZMapWindow  window = (ZMapWindowStruct*)data ;
#endif

  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "item_feature_type")) ;

  switch (item_feature_type)
    {
    case ITEM_FEATURE_SIMPLE:
    case ITEM_FEATURE_PARENT:
      {
	/* Nothing to do... */

	break ;
      }
    case ITEM_FEATURE_BOUNDING_BOX:
    case ITEM_FEATURE_CHILD:
      {
	ZMapWindowItemFeature item_data ;

	item_data = g_object_get_data(G_OBJECT(item), "item_subfeature_data") ;
	g_free(item_data) ;
	break ;
      }
    default:
      {
	zMapLogFatal("Coding error, unrecognised ZMapWindowItemFeatureType: %d", item_feature_type) ;
	break ;
      }
    }

  return event_handled ;
}



/* Callback for any events that happen on individual canvas items. */
static gboolean canvasItemEventCB(FooCanvasItem *item, GdkEvent *event, gpointer data)
{
  gboolean event_handled = FALSE ;
  ZMapWindow  window = (ZMapWindowStruct*)data ;
  ZMapFeature feature ;
#ifdef RDS_DONT_INCLUDE
  ZMapFeatureTypeStyle type ;
#endif
  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      {
	GdkEventButton *but_event = (GdkEventButton *)event ;
	ZMapWindowItemFeatureType item_feature_type ;
	FooCanvasItem *real_item ;

	item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item),
							      "item_feature_type")) ;
	zMapAssert(item_feature_type == ITEM_FEATURE_SIMPLE
		   || item_feature_type == ITEM_FEATURE_CHILD
		   || item_feature_type == ITEM_FEATURE_BOUNDING_BOX) ;

	/* If its a bounding box then we don't want the that to influence highlighting.. */
	if (item_feature_type == ITEM_FEATURE_BOUNDING_BOX)
	  real_item = zMapWindowFindFeatureItemByItem(window, item) ;
	else
	  real_item = item ;

	/* Retrieve the feature item info from the canvas item. */
	feature = (ZMapFeature)g_object_get_data(G_OBJECT(item), "item_feature_data");  
	zMapAssert(feature) ;

	/* Button 1 and 3 are handled, 2 is left for a general handler which could be
	 * the root handler. */
	if (but_event->button == 1 || but_event->button == 3)
	  {
#ifdef RDS_DONT_INCLUDE
	    /* Highlight the object the user clicked on and pass information about
	     * about it back to layer above. */
	    ZMapWindowSelectStruct select = {NULL} ;
	    double x = 0.0, y = 0.0 ;
#endif
	    /* Pass information about the object clicked on back to the application. */
	    zMapWindowUpdateInfoPanel(window, feature, item);      

	    if (but_event->button == 3)
	      {
		/* Pop up an item menu. */
		makeItemMenu(but_event, window, real_item) ;
	      }

	    event_handled = TRUE ;
	  }
	break ;
      }
    default:
      {
	/* By default we _don't_handle events. */
	event_handled = FALSE ;

	break ;
      }
    }

  return event_handled ;
}




static gboolean columnBoundingBoxEventCB(FooCanvasItem *item, GdkEvent *event, gpointer data)
{
  gboolean event_handled = FALSE ;
  ZMapWindow window = (ZMapWindow)data ;

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      {
	GdkEventButton *but_event = (GdkEventButton *)event ;
	ZMapFeatureSet feature_set = NULL ;
	ZMapFeatureTypeStyle style ;

	/* If a column is empty it will not have a feature set but it will have a style from which we
	 * can display the column id. */
	feature_set = (ZMapFeatureSet)g_object_get_data(G_OBJECT(item), "item_feature_data") ;
	style = g_object_get_data(G_OBJECT(item), "item_feature_style") ;
	zMapAssert(feature_set || style) ;

	/* Button 1 and 3 are handled, 2 is passed on to a general handler which could be
	 * the root handler. */
	switch (but_event->button)
	  {
	  case 1:
	    {
	      ZMapWindowSelectStruct select = {NULL} ;

	      if (feature_set)
		select.text = (char *)g_quark_to_string(feature_set->original_id) ;
	      else
		select.text = (char *)g_quark_to_string(style->original_id) ;

	      (*(window->caller_cbs->select))(window, window->app_data, (void *)&select) ;

	      event_handled = TRUE ;
	      break ;
	    }
	  /* There are > 3 button mouse,  e.g. scroll wheels, which we don't want to handle. */
	  default:					    
	  case 2:
	    {
	      event_handled = FALSE ;
	      break ;
	    }
	  case 3:
	    {
	      if (feature_set)
		{
		  makeColumnMenu(but_event, window, item, feature_set) ;

		  event_handled = TRUE ;
		}
	      break ;
	    }
	  }
	break ;
      } 
    default:
      {
	/* By default we _don't_ handle events. */
	event_handled = FALSE ;

	break ;
      }
    }

  return event_handled ;
}


static gboolean dnaItemEventCB(FooCanvasItem *item, GdkEvent *event, gpointer data)
{
  gboolean event_handled = FALSE;
  FooCanvasGroup *txtGroup = NULL;
  textGroupSelection txtSelect = NULL;

  txtGroup  = FOO_CANVAS_GROUP(item->parent) ;

  txtSelect = getTextGroupData(txtGroup, (ZMapWindow)data); /* This will initialise for us if it's not there. */
  zMapAssert(txtSelect);

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      {
        event_handled   = TRUE;
        if(event->type == GDK_BUTTON_RELEASE)
          {
#define DONTGRABPOINTER
#ifndef DONTGRABPOINTER
            foo_canvas_item_ungrab(item, event->button.time);
#endif 
            txtSelect->originItemListMember = NULL;
            foo_canvas_item_hide(FOO_CANVAS_ITEM(txtSelect->tooltip));
          }
        else
          {
            GdkCursor *xterm;
            GList *list;
            xterm = gdk_cursor_new (GDK_XTERM);

#ifndef DONTGRABPOINTER
            foo_canvas_item_grab(item, 
                                 GDK_POINTER_MOTION_MASK |  
                                 GDK_BUTTON_RELEASE_MASK | 
                                 GDK_BUTTON_MOTION_MASK,
                                 xterm,
                                 event->button.time);
#endif
            gdk_cursor_destroy(xterm);
            list = g_list_first(txtGroup->item_list);
            while(list)
              {
                /* pointers equal */
                if(item != list->data)
                  list = list->next;
                else
                  {
                    txtSelect->originItemListMember = list;
                    list = NULL;
                  }
              }
            txtSelect->seqFirstIdx = txtSelect->seqLastIdx = -1;
            updateInfoGivenCoords(txtSelect, event->button.x, event->button.y);
          }
      }
      break;
    case GDK_MOTION_NOTIFY:
      {
        if(txtSelect->originItemListMember)
          {
            ZMapDrawTextRowData trd = NULL;
            if((trd = zMapDrawGetTextItemData(item)))
              {
                updateInfoGivenCoords(txtSelect, event->motion.x, event->motion.y);
                zMapDrawHighlightTextRegion(txtSelect->highlight,
                                            txtSelect->seqFirstIdx, 
                                            txtSelect->seqLastIdx,
                                            item);
                event_handled = TRUE;
              }
          }
      }    
      break;
    case GDK_LEAVE_NOTIFY:
      {
        foo_canvas_item_hide(FOO_CANVAS_ITEM( txtSelect->tooltip ));
      }
    case GDK_ENTER_NOTIFY:
      {
        
      }
      break;
    default:
      {
	/* By default we _don't_handle events. */
	event_handled = FALSE ;
	break ;
      }
    }

  return event_handled ;
}




/* 
 *                       Menu functions.
 */



/* Build the menu for a feature item. */
static void makeItemMenu(GdkEventButton *button_event, ZMapWindow window, FooCanvasItem *item)
{
  char *menu_title = "Item menu" ;
  GList *menu_sets = NULL ;
  ItemMenuCBData menu_data ;
  ZMapFeature feature ;
#ifdef RDS_DONT_INCLUDE
  ZMapWindowMenuItem menu_item ;
#endif

  /* Some parts of the menu are feature type specific so retrieve the feature item info
   * from the canvas item. */
  feature = g_object_get_data(G_OBJECT(item), "item_feature_data") ;
  zMapAssert(feature) ;

  /* Call back stuff.... */
  menu_data = g_new0(ItemMenuCBDataStruct, 1) ;
  menu_data->item_cb = TRUE ;
  menu_data->window = window ;
  menu_data->item = item ;

  /* Make up the menu. */
  menu_sets = g_list_append(menu_sets, makeMenuFeatureOps(NULL, NULL, menu_data)) ;

  menu_sets = g_list_append(menu_sets, makeMenuBump(NULL, NULL, menu_data)) ;

  if (feature->type == ZMAPFEATURE_ALIGNMENT)
    {
      if (feature->feature.homol.type == ZMAPHOMOL_X_HOMOL)
	menu_sets = g_list_append(menu_sets, makeMenuProteinHomol(NULL, NULL, menu_data)) ;
      else
	menu_sets = g_list_append(menu_sets, makeMenuDNAHomol(NULL, NULL, menu_data)) ;
    }

  menu_sets = g_list_append(menu_sets, makeMenuDumpOps(NULL, NULL, menu_data)) ;

  zMapWindowMakeMenu(menu_title, menu_sets, button_event) ;

  return ;
}



/* Build the background menu for a column. */
static void makeColumnMenu(GdkEventButton *button_event, ZMapWindow window,
			   FooCanvasItem *item, ZMapFeatureSet feature_set)
{
  char *menu_title = "Column menu" ;
  GList *menu_sets = NULL ;
  ItemMenuCBData cbdata ;
#ifdef RDS_DONT_INCLUDE
  ZMapWindowMenuItem menu_item ;
#endif

  cbdata = g_new0(ItemMenuCBDataStruct, 1) ;
  cbdata->item_cb = FALSE ;
  cbdata->window = window ;
  cbdata->item = item ;
  cbdata->feature_set = feature_set ;

  /* Make up the menu. */
  menu_sets = g_list_append(menu_sets, makeMenuColumnOps(NULL, NULL, cbdata)) ;

  menu_sets = g_list_append(menu_sets, makeMenuBump(NULL, NULL, cbdata)) ;

  menu_sets = g_list_append(menu_sets, makeMenuDumpOps(NULL, NULL, cbdata)) ;

  zMapWindowMakeMenu(menu_title, menu_sets, button_event) ;

  return ;
}




/* Set of makeMenuXXXX functions to create common subsections of menus. If you add to this
 * you should make sure you understand how to specify menu paths in the item factory style.
 * If you get it wrong then the menus will be scr*wed up.....
 * 
 * The functions are defined in pairs: one to define the menu, one to handle the callback
 * actions, this is to emphasise that their indexes must be kept in step !
 * 
 * NOTE HOW THE MENUS ARE DECLARED STATIC IN THE VARIOUS ROUTINES TO MAKE SURE THEY STAY
 * AROUND...OTHERWISE WE WILL HAVE TO KEEP ALLOCATING/DEALLOCATING THEM.....
 */



/* this is in the general menu and needs to be handled separately perhaps as the index is a global
 * one shared amongst all general menu functions... */
static ZMapWindowMenuItem makeMenuFeatureOps(int *start_index_inout,
					     ZMapWindowMenuItemCallbackFunc callback_func,
					     gpointer callback_data)
{
  static ZMapWindowMenuItemStruct menu[] =
    {
      {"Show Feature List",      1, itemMenuCB, NULL},
      {"Edit Feature Details",   2, itemMenuCB, NULL},
      {"Feature Search Window",  3, itemMenuCB, NULL},
      {"Pfetch this feature",    4, itemMenuCB, NULL},
      {NULL,                     0, NULL,       NULL}
    } ;

  populateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


static void itemMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapFeature feature ;

  /* Retrieve the feature item info from the canvas item. */
  feature = g_object_get_data(G_OBJECT(menu_data->item), "item_feature_data") ;
  zMapAssert(feature) ;

  switch (menu_item_id)
    {
    case 1:
      {
        ZMapFeature feature = NULL;
        GList *list = NULL;
        feature = g_object_get_data(G_OBJECT(menu_data->item), "item_feature_data");
        list = zmapWindowFToIFindItemSetFull(menu_data->window->context_to_item, 
					     feature->parent->parent->parent->unique_id,
					     feature->parent->parent->unique_id,
					     feature->parent->unique_id,
					     zmapFeatureLookUpEnum(feature->strand, STRAND_ENUM),
					     g_quark_from_string("*")) ;

        zmapWindowListWindowCreate(menu_data->window, list, 
                                   (char *)g_quark_to_string(feature->parent->original_id), 
                                   menu_data->item) ;
	break ;
      }
    case 2:
      {
        ZMapFeature feature = NULL;
        GList *list = NULL;
        feature = g_object_get_data(G_OBJECT(menu_data->item), "item_feature_data");
        list    = zmapWindowFToIFindItemSetFull(menu_data->window->context_to_item, 
                                                feature->parent->parent->parent->unique_id,
                                                feature->parent->parent->unique_id,
                                                feature->parent->unique_id,
                                                zmapFeatureLookUpEnum(feature->strand, STRAND_ENUM),
                                                feature->unique_id);
        zmapWindowEditorCreate(menu_data->window, list->data) ;
        
	break ;
      }
    case 3:
      zmapWindowCreateSearchWindow(menu_data->window, (ZMapFeatureAny)feature) ;
      break ;
    case 4:
      pfetchEntry(menu_data->window, (char *)g_quark_to_string(feature->original_id)) ;
      break ;

    default:
      zMapAssert("Coding error, unrecognised menu item number.") ; /* exits... */
      break ;
    }

  g_free(menu_data) ;

  return ;
}




/* this is in the general menu and needs to be handled separately perhaps as the index is a global
 * one shared amongst all general menu functions... */
static ZMapWindowMenuItem makeMenuColumnOps(int *start_index_inout,
					    ZMapWindowMenuItemCallbackFunc callback_func,
					    gpointer callback_data)
{
  static ZMapWindowMenuItemStruct menu[] =
    {
      {"Show Feature List",      1, columnMenuCB, NULL},
      {"Feature Search Window",  2, columnMenuCB, NULL},
      {NULL,                     0, NULL,       NULL}
    } ;

  populateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}




static void columnMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;

  switch (menu_item_id)
    {
    case 1:
      {
        ZMapFeatureAny feature ;
	ZMapStrand strand ;
        GList *list ;
	
        feature = (ZMapFeatureAny)g_object_get_data(G_OBJECT(menu_data->item), "item_feature_data") ;
	strand = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menu_data->item), "item_feature_strand")) ;
				 
	list = zmapWindowFToIFindItemSetFull(menu_data->window->context_to_item, 
					     feature->parent->parent->unique_id,
					     feature->parent->unique_id,
					     feature->unique_id,
					     zmapFeatureLookUpEnum(strand, STRAND_ENUM),
					     g_quark_from_string("*")) ;
	
        zmapWindowListWindowCreate(menu_data->window, list, 
                                   (char *)g_quark_to_string(feature->original_id), 
                                   NULL) ;

	break ;
      }
    case 2:
      zmapWindowCreateSearchWindow(menu_data->window, (ZMapFeatureAny)(menu_data->feature_set)) ;
      break ;
    default:
      zMapAssert("Coding error, unrecognised menu item number.") ;
      break ;
    }

  g_free(menu_data) ;

  return ;
}





/* Probably it would be wise to pass in the callback function, the start index for the item
 * identifier and perhaps the callback data...... */
static ZMapWindowMenuItem makeMenuBump(int *start_index_inout,
				       ZMapWindowMenuItemCallbackFunc callback_func,
				       gpointer callback_data)
{
  static ZMapWindowMenuItemStruct menu[] =
    {
      {"_Bump", 0, NULL, NULL},
      {"Bump/Column UnBump",        ZMAPOVERLAP_COMPLETE, bumpMenuCB, NULL},
      {"Bump/Column Bump Position", ZMAPOVERLAP_POSITION, bumpMenuCB, NULL},
      {"Bump/Column Bump Overlap",  ZMAPOVERLAP_OVERLAP,  bumpMenuCB, NULL},
      {"Bump/Column Bump Name",     ZMAPOVERLAP_NAME,     bumpMenuCB, NULL},
      {"Bump/Column Bump Simple",   ZMAPOVERLAP_SIMPLE,   bumpMenuCB, NULL},
      {NULL, 0, NULL, NULL}
    } ;

  populateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}

/* Bump a column and reposition the other columns. */
static void bumpMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapStyleOverlapMode bump_type = (ZMapStyleOverlapMode)menu_item_id  ;
  FooCanvasGroup *column_group ;

  if (menu_data->item_cb)
    column_group = getItemsColGroup(menu_data->item) ;
  else
    column_group = FOO_CANVAS_GROUP(menu_data->item) ;

  zmapWindowColumnBump(column_group, bump_type) ;
  zmapWindowColumnReposition(column_group) ;

  g_free(menu_data) ;

  return ;
}


static ZMapWindowMenuItem makeMenuDumpOps(int *start_index_inout,
					  ZMapWindowMenuItemCallbackFunc callback_func,
					  gpointer callback_data)
{
  static ZMapWindowMenuItemStruct menu[] =
    {
      {"_Dump",                  0, NULL,       NULL},
      {"Dump/Dump DNA"          , 1, dumpMenuCB, NULL},
      {"Dump/Dump Features"     , 2, dumpMenuCB, NULL},
      {"Dump/Dump Context"      , 3, dumpMenuCB, NULL},
      {NULL               , 0, NULL, NULL}
    } ;

  populateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}

static void dumpMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  ZMapFeatureAny feature ;

  feature = (ZMapFeatureAny)g_object_get_data(G_OBJECT(menu_data->item), "item_feature_data") ;

  switch (menu_item_id)
    {
    case 1:
      {
	dumpFASTA(menu_data->window) ;

	break ;
      }
    case 2:
      {
	dumpFeatures(menu_data->window, feature) ;

	break ;
      }
    case 3:
      {
	dumpContext(menu_data->window) ;

	break ;
      }
    default:
      zMapAssert("Coding error, unrecognised menu item number.") ; /* exits... */
      break ;
    }

  g_free(menu_data) ;

  return ;
}


static ZMapWindowMenuItem makeMenuDNAHomol(int *start_index_inout,
					   ZMapWindowMenuItemCallbackFunc callback_func,
					   gpointer callback_data)
{
  static ZMapWindowMenuItemStruct menu[] =
    {
      {"_Blixem",      0, NULL, NULL},
      {"Blixem/Show multiple dna alignment",                                 1, blixemMenuCB, NULL},
      {"Blixem/Show multiple dna alignment for just this type of homology",  2, blixemMenuCB, NULL},
      {NULL,                                                          0, NULL,         NULL}
    } ;

  populateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


static ZMapWindowMenuItem makeMenuProteinHomol(int *start_index_inout,
					       ZMapWindowMenuItemCallbackFunc callback_func,
					       gpointer callback_data)
{
  static ZMapWindowMenuItemStruct menu[] =
    {
      {"_Blixem",      0, NULL, NULL},
      {"Blixem/Show multiple protein alignment",                                 1, blixemMenuCB, NULL},
      {"Blixem/Show multiple protein alignment for just this type of homology",  2, blixemMenuCB, NULL},
      {NULL,                                                              0, NULL,         NULL}
    } ;

  populateMenu(menu, start_index_inout, callback_func, callback_data) ;

  return menu ;
}


/* call blixem either for a single type of homology or for all homologies. */
static void blixemMenuCB(int menu_item_id, gpointer callback_data)
{
  ItemMenuCBData menu_data = (ItemMenuCBData)callback_data ;
  gboolean single_homol_type = FALSE ;
  gboolean status ;

  switch (menu_item_id)
    {
    case 1:
      single_homol_type = FALSE ;
      break ;
    case 2:
      single_homol_type = TRUE ;
      break ;
    default:
      zMapAssert("Coding error, unrecognised menu item number.") ; /* exits... */
      break ;
    }

  status = zmapWindowCallBlixem(menu_data->window, menu_data->item, single_homol_type) ;
  
  g_free(menu_data) ;

  return ;
}



/* Overrides settings in menu with supplied data. */
static void populateMenu(ZMapWindowMenuItem menu,
			 int *start_index_inout,
			 ZMapWindowMenuItemCallbackFunc callback_func,
			 gpointer callback_data)
{
  ZMapWindowMenuItem menu_item ;
  int index ;

  zMapAssert(menu) ;

  if (start_index_inout)
    index = *start_index_inout ;

  menu_item = menu ;
  while (menu_item->name != NULL)
    {
      if (start_index_inout)
	menu_item->id = index ;
      if (callback_func)
	menu_item->callback_func = callback_func ;
      if (callback_data)
	menu_item->callback_data = callback_data ;

      menu_item++ ;
    }

  if (start_index_inout)
    *start_index_inout = index ;

  return ;
}



/* this needs to be a general function... */
static FooCanvasGroup *getItemsColGroup(FooCanvasItem *item)
{
  FooCanvasGroup *group = NULL ;
  ZMapWindowItemFeatureType item_feature_type ;


  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item),
							"item_feature_type")) ;

  switch (item_feature_type)
    {
    case ITEM_FEATURE_SIMPLE:
    case ITEM_FEATURE_PARENT:
      group = zmapWindowContainerGetParent(item->parent) ;
      break ;
    case ITEM_FEATURE_CHILD:
    case ITEM_FEATURE_BOUNDING_BOX:
      group = zmapWindowContainerGetParent(item->parent->parent) ;
      break ;
    default:
      zMapAssert("Coding error, unrecognised menu item number.") ;
      break ;
    }

  return group ;
}



/* Read colour information from the configuration file, note that we read _only_ the first
 * window stanza found in the file, subsequent ones are not read. */
static void setColours(ZMapCanvasData canvas_data)
{
  ZMapConfig config ;
  ZMapConfigStanzaSet colour_list = NULL ;
  ZMapConfigStanza colour_stanza ;
  char *window_stanza_name = ZMAP_WINDOW_CONFIG ;
  ZMapConfigStanzaElementStruct colour_elements[] = {{"colour_root", ZMAPCONFIG_STRING, {ZMAP_WINDOW_BACKGROUND_COLOUR}},
						     {"colour_alignment", ZMAPCONFIG_STRING, {ZMAP_WINDOW_BACKGROUND_COLOUR}},
						     {"colour_block", ZMAPCONFIG_STRING, {ZMAP_WINDOW_BACKGROUND_COLOUR}},
						     {"colour_m_forward", ZMAPCONFIG_STRING, {ZMAP_WINDOW_MBLOCK_F_BG}},
						     {"colour_m_reverse", ZMAPCONFIG_STRING, {ZMAP_WINDOW_MBLOCK_R_BG}},
						     {"colour_q_forward", ZMAPCONFIG_STRING, {ZMAP_WINDOW_QBLOCK_F_BG}},
						     {"colour_q_reverse", ZMAPCONFIG_STRING, {ZMAP_WINDOW_QBLOCK_R_BG}},
						     {"colour_m_forwardcol", ZMAPCONFIG_STRING, {ZMAP_WINDOW_MBLOCK_F_BG}},
						     {"colour_m_reversecol", ZMAPCONFIG_STRING, {ZMAP_WINDOW_MBLOCK_R_BG}},
						     {"colour_q_forwardcol", ZMAPCONFIG_STRING, {ZMAP_WINDOW_QBLOCK_F_BG}},
						     {"colour_q_reversecol", ZMAPCONFIG_STRING, {ZMAP_WINDOW_QBLOCK_R_BG}},
						     {NULL, -1, {NULL}}} ;


  /* IN A HURRY...BADLY WRITTEN, COULD BE MORE COMPACT.... */

  if ((config = zMapConfigCreate()))
    {
      colour_stanza = zMapConfigMakeStanza(window_stanza_name, colour_elements) ;

      if (zMapConfigFindStanzas(config, colour_stanza, &colour_list))
	{
	  ZMapConfigStanza next_colour ;
	  
	  /* Get the first window stanza found, we will ignore any others. */
	  next_colour = zMapConfigGetNextStanza(colour_list, NULL) ;

	  gdk_color_parse(zMapConfigGetElementString(next_colour, "colour_root"),
			  &(canvas_data->colour_root)) ;
	  gdk_color_parse(zMapConfigGetElementString(next_colour, "colour_alignment"),
			  &canvas_data->colour_alignment) ;
	  gdk_color_parse(zMapConfigGetElementString(next_colour, "colour_block"),
			  &canvas_data->colour_block) ;
	  gdk_color_parse(zMapConfigGetElementString(next_colour, "colour_m_forward"),
			  &canvas_data->colour_mblock_for) ;
	  gdk_color_parse(zMapConfigGetElementString(next_colour, "colour_m_reverse"),
			  &canvas_data->colour_mblock_rev) ;
	  gdk_color_parse(zMapConfigGetElementString(next_colour, "colour_q_forward"),
			  &canvas_data->colour_qblock_for) ;
	  gdk_color_parse(zMapConfigGetElementString(next_colour, "colour_q_reverse"),
			  &canvas_data->colour_qblock_rev) ;
	  gdk_color_parse(zMapConfigGetElementString(next_colour, "colour_m_forwardcol"),
			  &(canvas_data->colour_mforward_col)) ;
	  gdk_color_parse(zMapConfigGetElementString(next_colour, "colour_m_reversecol"),
			  &(canvas_data->colour_mreverse_col)) ;
	  gdk_color_parse(zMapConfigGetElementString(next_colour, "colour_q_forwardcol"),
			  &(canvas_data->colour_qforward_col)) ;
	  gdk_color_parse(zMapConfigGetElementString(next_colour, "colour_q_reversecol"),
			  &(canvas_data->colour_qreverse_col)) ;
	  
	  zMapConfigDeleteStanzaSet(colour_list) ;		    /* Not needed anymore. */
	}
      else
	{
	  gdk_color_parse(ZMAP_WINDOW_BACKGROUND_COLOUR, &(canvas_data->colour_root)) ;
	  gdk_color_parse(ZMAP_WINDOW_BACKGROUND_COLOUR, &canvas_data->colour_alignment) ;
	  gdk_color_parse(ZMAP_WINDOW_STRAND_DIVIDE_COLOUR, &canvas_data->colour_block) ;
	  gdk_color_parse(ZMAP_WINDOW_MBLOCK_F_BG, &canvas_data->colour_mblock_for) ;
	  gdk_color_parse(ZMAP_WINDOW_MBLOCK_R_BG, &canvas_data->colour_mblock_rev) ;
	  gdk_color_parse(ZMAP_WINDOW_QBLOCK_F_BG, &canvas_data->colour_qblock_for) ;
	  gdk_color_parse(ZMAP_WINDOW_QBLOCK_R_BG, &canvas_data->colour_qblock_rev) ;
	  gdk_color_parse(ZMAP_WINDOW_MBLOCK_F_BG, &(canvas_data->colour_mforward_col)) ;
	  gdk_color_parse(ZMAP_WINDOW_MBLOCK_R_BG, &(canvas_data->colour_mreverse_col)) ;
	  gdk_color_parse(ZMAP_WINDOW_QBLOCK_F_BG, &(canvas_data->colour_qforward_col)) ;
	  gdk_color_parse(ZMAP_WINDOW_QBLOCK_R_BG, &(canvas_data->colour_qreverse_col)) ;
	}


      zMapConfigDestroyStanza(colour_stanza) ;
      
      zMapConfigDestroy(config) ;
    }

  return ;
}


static void dumpFASTA(ZMapWindow window)
{
  gboolean dna = TRUE ;
  char *filepath = NULL ;
  GIOChannel *file = NULL ;
  GError *error = NULL ;
  char *seq_name = NULL ;
  int seq_len = 0 ;
  char *sequence = NULL ;
  char *error_prefix = "FASTA DNA dump failed:" ;

  if (!(dna = zmapFeatureContextDNA(window->feature_context, &seq_name, &seq_len, &sequence))
      || !(filepath = zmapGUIFileChooser(window->toplevel, "FASTA filename ?", NULL, NULL))
      || !(file = g_io_channel_new_file(filepath, "w", &error))
      || !zmapDNADumpFASTA(file, seq_name, seq_len, sequence, &error))
    {
      char *err_msg = NULL ;

      /* N.B. if there is no filepath it means user cancelled so take no action... */
      if (!dna)
	err_msg = "there is no DNA to dump." ;
      else if (error)
	err_msg = error->message ;

      if (err_msg)
	zMapShowMsg(ZMAP_MSG_WARNING, "%s  %s", error_prefix, err_msg) ;

      if (error)
	g_error_free(error) ;
    }


  if (file)
    {
      GIOStatus status ;

      if ((status = g_io_channel_shutdown(file, TRUE, &error)) != G_IO_STATUS_NORMAL)
	{
	  zMapShowMsg(ZMAP_MSG_WARNING, "%s  %s", error_prefix, error->message) ;

	  g_error_free(error) ;
	}
    }


  return ;
}


static void dumpFeatures(ZMapWindow window, ZMapFeatureAny feature)
{
  char *filepath = NULL ;
  GIOChannel *file = NULL ;
  GError *error = NULL ;
  char *error_prefix = "Features dump failed:" ;
  ZMapFeatureBlock feature_block ;

  /* Should extend this any type.... */
  zMapAssert(feature->struct_type == ZMAPFEATURE_STRUCT_FEATURESET
	     || feature->struct_type == ZMAPFEATURE_STRUCT_FEATURE) ;

  /* Find the block from whatever pointer we are sent...  */
  if (feature->struct_type == ZMAPFEATURE_STRUCT_FEATURESET)
    feature_block = (ZMapFeatureBlock)(feature->parent) ;
  else
    feature_block = (ZMapFeatureBlock)(feature->parent->parent) ;


  if (!(filepath = zmapGUIFileChooser(window->toplevel, "Feature Dump filename ?", NULL, "gff"))
      || !(file = g_io_channel_new_file(filepath, "w", &error))
      || !zMapGFFDump(file, feature_block, &error))
    {
      /* N.B. if there is no filepath it means user cancelled so take no action...,
       * otherwise we output the error message. */
      if (error)
	{
	  zMapShowMsg(ZMAP_MSG_WARNING, "%s  %s", error_prefix, error->message) ;

	  g_error_free(error) ;
	}
    }


  if (file)
    {
      GIOStatus status ;

      if ((status = g_io_channel_shutdown(file, TRUE, &error)) != G_IO_STATUS_NORMAL)
	{
	  zMapShowMsg(ZMAP_MSG_WARNING, "%s  %s", error_prefix, error->message) ;

	  g_error_free(error) ;
	}
    }


  return ;
}




static void dumpContext(ZMapWindow window)
{
  char *filepath = NULL ;
  GIOChannel *file = NULL ;
  GError *error = NULL ;
#ifdef RDS_DONT_INCLUDE
  char *seq_name = NULL ;
  int seq_len = 0 ;
  char *sequence = NULL ;
#endif
  char *error_prefix = "Feature context dump failed:" ;

  if (!(filepath = zmapGUIFileChooser(window->toplevel, "Context Dump filename ?", NULL, "zmap"))
      || !(file = g_io_channel_new_file(filepath, "w", &error))
      || !zMapFeatureContextDump(file, window->feature_context, &error))
    {
      /* N.B. if there is no filepath it means user cancelled so take no action...,
       * otherwise we output the error message. */
      if (error)
	{
	  zMapShowMsg(ZMAP_MSG_WARNING, "%s  %s", error_prefix, error->message) ;

	  g_error_free(error) ;
	}
    }


  if (file)
    {
      GIOStatus status ;

      if ((status = g_io_channel_shutdown(file, TRUE, &error)) != G_IO_STATUS_NORMAL)
	{
	  zMapShowMsg(ZMAP_MSG_WARNING, "%s  %s", error_prefix, error->message) ;

	  g_error_free(error) ;
	}
    }


  return ;
}


static void pfetchEntry(ZMapWindow window, char *sequence_name)
{
  char *error_prefix = "PFetch failed:" ;
  gboolean result ;
  gchar *command_line ;
  gchar *standard_output = NULL, *standard_error = NULL ;
  gint exit_status = 0 ;
  GError *error = NULL ;

  command_line = g_strdup_printf("pfetch -F '%s'", sequence_name) ;

  if (!(result = g_spawn_command_line_sync(command_line,
					   &standard_output, &standard_error,
					   &exit_status, &error)))
    {
      zMapShowMsg(ZMAP_MSG_WARNING, "%s  %s", error_prefix, error->message) ;

      g_error_free(error) ;
    }
  else if (!*standard_output)
    {
      /* some versions of pfetch erroneously return nothing if they fail to find an entry. */

      zMapShowMsg(ZMAP_MSG_WARNING, "%s  %s", error_prefix, "no output returned !") ;
    }
  else
    {
      *(standard_output + ((int)(strlen(standard_output)) - 1)) = '\0' ;	/* Get rid of annoying newline */

      if (g_ascii_strcasecmp(standard_output, "no match") == 0)
	{
	  zMapShowMsg(ZMAP_MSG_INFORMATION, "%s  %s", error_prefix, standard_output) ;
	}
      else
	{
	  char *title ;
	  
	  title = g_strdup_printf("pfetch: \"%s\"", sequence_name) ;

	  zMapGUIShowText(title, standard_output, FALSE) ;

	  g_free(title) ;
	}
    }

  /* Clear up, note that we have to do this because currently they are returned as buffers
   * from g_strings (not documented in the interface) and so should always be freed. I include
   * the conditional freeing in case the implementation changes anytime. */
  if (standard_output)
    g_free(standard_output) ;
  if (standard_error)
    g_free(standard_error) ;


  g_free(command_line) ;

  return ;
}


static textGroupSelection getTextGroupData(FooCanvasGroup *txtGroup, ZMapWindow window)
{
  textGroupSelection txtSelect = NULL;

  if(!(txtSelect = (textGroupSelection)g_object_get_data(G_OBJECT(txtGroup), "HIGHLIGHT_INFO")))
    {
      txtSelect  = (textGroupSelection)g_new0(textGroupSelectionStruct, 1);
      txtSelect->tooltip = zMapDrawToolTipCreate(FOO_CANVAS_ITEM(txtGroup)->canvas);
      foo_canvas_item_hide(FOO_CANVAS_ITEM( txtSelect->tooltip ));

      txtSelect->highlight = FOO_CANVAS_GROUP(foo_canvas_item_new(txtGroup,
                                                                  foo_canvas_group_get_type(),
                                                                  NULL));
      txtSelect->window = window;
      g_object_set_data(G_OBJECT(txtGroup), "HIGHLIGHT_INFO", (gpointer)txtSelect);

    }

  return txtSelect ;
}

static void pointerIsOverItem(gpointer data, gpointer user_data)
{
  FooCanvasItem *item = (FooCanvasItem *)data;
  textGroupSelection select = (textGroupSelection)user_data;
  ZMapDrawTextRowData trd = NULL;
  double currentY = 0.0;
  currentY = select->buttonCurrentY - select->window->min_coord;

  if(select->need_update && (trd = zMapDrawGetTextItemData(item)))
    {
      if(currentY > trd->rowOffset &&
         currentY < trd->rowOffset + trd->fullStrLength)
        {
          /* Now what do we need to do? */
          /* update the values of seqFirstIdx && seqLastIdx */
          int currentIdx, iMinIdx, iMaxIdx;
          double x1, x2, y1, y2, char_width, clmpX;

          foo_canvas_item_get_bounds(item, &x1, &y1, &x2, &y2);
          foo_canvas_item_i2w(item, &x1, &y1);
          foo_canvas_item_i2w(item, &x2, &y2);

          /* clamp inside the item! (and effectively the sequence) */
          clmpX = (select->buttonCurrentX > x1 ? 
                   select->buttonCurrentX < x2 ? select->buttonCurrentX :
                   x2 : x1);

          char_width  = trd->columnWidth / trd->drawnStrLength;
          currentIdx  = floor((clmpX - x1) / char_width); /* we're zero based so we need to be able to select the first base. revisit if it's an issue */
          currentIdx += trd->rowOffset;

          if(select->seqLastIdx  == -1 && 
             select->seqFirstIdx == select->seqLastIdx)
            select->seqFirstIdx = 
              select->seqLastIdx = 
              select->originIdx = currentIdx;

          /* clamp to the original index */
          iMinIdx = (select->seqFirstIdx < select->originIdx ? select->originIdx : select->seqFirstIdx);
          iMaxIdx = (select->seqLastIdx  > select->originIdx ? select->originIdx : select->seqLastIdx);
          
          /* resize to current index */
          select->seqFirstIdx = MIN(currentIdx, iMinIdx);
          select->seqLastIdx  = MAX(currentIdx, iMaxIdx);

          zMapDrawToolTipSetPosition(select->tooltip, 
                                     x1 - 200,
                                     trd->rowOffset - (y2 - y1), 
                                     g_strdup_printf("%d - %d", select->seqFirstIdx + 1, select->seqLastIdx + 1));

          select->need_update = FALSE;
        }
    }

  return ;
}

static void updateInfoGivenCoords(textGroupSelection select, 
                                  double currentX,
                                  double currentY) /* These are WORLD coords */
{
  GList *listStart = NULL;
  FooCanvasItem *origin;
  ZMapGListDirection forward = ZMAP_GLIST_FORWARD;

  select->buttonCurrentX = currentX;
  select->buttonCurrentY = currentY;
  select->need_update    = TRUE;
  listStart              = select->originItemListMember;

  /* Now work out where we are with custom g_list_foreach */
  
  /* First we need to know which way to go */
  origin = (FooCanvasItem *)(listStart->data);
  if(origin)
    {
      double x1, x2, y1, y2, halfy;
      foo_canvas_item_get_bounds(origin, &x1, &y1, &x2, &y2);
      foo_canvas_item_i2w(origin, &x1, &y2);
      foo_canvas_item_i2w(origin, &x2, &y2);
      halfy = ((y2 - y1) / 2) + y1;
      if(select->buttonCurrentY < halfy)
        forward = ZMAP_GLIST_REVERSE;
      else if(select->buttonCurrentY > halfy)
        forward = ZMAP_GLIST_FORWARD;
      else
        forward = ZMAP_GLIST_FORWARD;
    }

  if(select->need_update)
    zMap_g_list_foreach_directional(listStart, pointerIsOverItem, 
                                    (gpointer)select, forward);

  return ;
}

static void dna_redraw_callback(FooCanvasGroup *text_grp,
                                double zoom,
                                gpointer user_data)
{
  ZMapWindow window       = (ZMapWindow)user_data;
  FooCanvasItem *grp_item = FOO_CANVAS_ITEM(text_grp);

  if(zmapWindowItemIsVisible(grp_item)) /* No need to redraw if we're not visible! */
    {
      textGroupSelection select = NULL;
#ifdef RDS_DONT_INCLUDE
      FooCanvasItem *txt_item = NULL;
      ZMapDrawTextRowData   trd = NULL;
#endif
      FooCanvasGroup *container ;

      container = zmapWindowContainerGetParent(grp_item->parent) ;

      /* We should hide these so they don't look odd */
      select = getTextGroupData(text_grp, window);
      foo_canvas_item_hide(FOO_CANVAS_ITEM(select->tooltip));
      foo_canvas_item_hide(FOO_CANVAS_ITEM(select->highlight));

      /* gets rid of text_grp !!!!... so we must create it again.... */
      zmapWindowContainerPurge(FOO_CANVAS_GROUP(grp_item->parent)) ;

      text_grp = FOO_CANVAS_GROUP(foo_canvas_item_new(zmapWindowContainerGetFeatures(container),
						      foo_canvas_group_get_type(),
						      NULL)) ;
      g_object_set_data(G_OBJECT(text_grp), CONTAINER_REDRAW_CALLBACK,
			dna_redraw_callback) ;
      g_object_set_data(G_OBJECT(text_grp), CONTAINER_REDRAW_DATA,
			(gpointer)window) ;

      zmapWindowColumnWriteDNA(window, text_grp);
      
#ifdef RDS_DONT_INCLUDE
      /* We could probably redraw the highlight here! 
       * Not sure this is good. Although it sort of works
       * it's not superb and I think I prefer not doing so */
      if(text_grp->item_list &&
         (txt_item = FOO_CANVAS_ITEM(text_grp->item_list->data)) && 
         (trd = zMapDrawGetTextItemData(txt_item)))
        zMapDrawHighlightTextRegion(select->highlight,
                                    select->seqFirstIdx, 
                                    select->seqLastIdx, 
                                    txt_item);
#endif
    }
  
  return ;
}

ZMapDrawTextIterator getIterator(double win_min_coord, double win_max_coord,
                                 double offset_start,  double offset_end,
                                 double text_height, gboolean numbered)
{
  int tmp;
  ZMapDrawTextIterator iterator   = g_new0(ZMapDrawTextIteratorStruct, 1);

  iterator->seq_start      = win_min_coord;
  iterator->seq_end        = win_max_coord - 1.0;
  iterator->offset_start   = floor(offset_start - win_min_coord);
  iterator->offset_end     = ceil( offset_end   - win_min_coord);
  iterator->shownSeqLength = ceil( offset_end - offset_start + 1.0);

  iterator->x            = ZMAP_WINDOW_TEXT_BORDER;
  iterator->y            = 0.0;

  iterator->n_bases      = ceil(text_height);

  iterator->length2draw  = iterator->shownSeqLength;
  /* checks for trailing bases */
  iterator->length2draw -= (tmp = iterator->length2draw % iterator->n_bases);

  iterator->rows         = iterator->length2draw / iterator->n_bases;
  iterator->cols         = iterator->n_bases;
  
  if((iterator->numbered = numbered))
    {
      double dtmp;
      int i = 0;
      dtmp = (double)iterator->length2draw;
      while(++i && ((dtmp = dtmp / 10) > 1.0)){ /* Get the int's string length for the format. */ }  
      iterator->format = g_strdup_printf("%%%dd: %%s...", i);
    }
  else
    iterator->format = "%s...";
  
  return iterator;
}

static void destroyIterator(ZMapDrawTextIterator iterator)
{
  zMapAssert(iterator);
  if(iterator->numbered && iterator->format)
    g_free(iterator->format);
  g_free(iterator);

  return ;
}




/****************** end of file ************************************/
