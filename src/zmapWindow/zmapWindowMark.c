/*  File: zmapWindowMark.c
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2007: Genome Research Ltd.
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
 * originally written by:
 *
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *
 * Description: 
 *
 * Exported functions: See zmapWindow_P.h
 * HISTORY:
 * Last edited: Apr 15 14:33 2009 (rds)
 * Created: Tue Jan 16 09:51:19 2007 (rds)
 * CVS info:   $Id: zmapWindowMark.c,v 1.15 2009-04-15 14:38:52 rds Exp $
 *-------------------------------------------------------------------
 */

#include <ZMap/zmapUtils.h>
#include <zmapWindow_P.h>
#include <zmapWindowContainer.h>

/* User can set a range (perhaps by selecting an item) for operations like zooming and bump options. */
typedef struct _ZMapWindowMarkStruct
{
  ZMapMagic       magic;
  gboolean        mark_set ;
  ZMapWindow      window ;
  FooCanvasItem  *range_item ;
  FooCanvasGroup *block_group ;
  ZMapFeatureBlock block ;
  double world_x1, world_y1, world_x2, world_y2 ;
  int range_top, range_bottom ;
  GdkColor        colour ;
  GdkBitmap      *stipple ;
  FooCanvasGroup *range_group ;
  FooCanvasItem  *top_range_item ;
  FooCanvasItem  *bottom_range_item ;
} ZMapWindowMarkStruct ;

/* Used to hold highlight information for the hightlight callback function. */
typedef struct
{
  ZMapWindow window ;
  ZMapWindowMark mark ;
  gboolean highlight ;
} HighlightStruct, *Highlight ;


#define mark_bitmap_width 16
#define mark_bitmap_height 4
static char mark_bitmap_bits[] =
  {
    0x11, 0x11,
    0x22, 0x22,
    0x44, 0x44,
    0x88, 0x88
  } ;


static void markItem(ZMapWindowMark mark, FooCanvasItem *item, gboolean set_mark) ;
static void markFuncCB(gpointer data, gpointer user_data) ;
static void markRange(ZMapWindowMark mark, double y1, double y2) ;
static void setBoundingBoxColour(ZMapWindowMark mark, FooCanvasItem *item, gboolean highlight) ; 

/* 
 *           Set of functions for handling the marked feature or region.
 * 
 * Essentially there are two ways to mark, either by marking a feature or
 * by marking a region. The two are mutually exclusive.
 * 
 * If a feature is marked then it will be used in a number of window operations
 * such as zooming and column bumping.
 * 
 * 
 */

ZMAP_MAGIC_NEW(mark_magic_G, ZMapWindowMarkStruct) ;


ZMapWindowMark zmapWindowMarkCreate(ZMapWindow window)
{
  ZMapWindowMark mark ;

  zMapAssert(window) ;

  mark = g_new0(ZMapWindowMarkStruct, 1) ;

  mark->magic = mark_magic_G ;

  mark->mark_set = FALSE ;

  mark->window = window ;

  mark->range_item = NULL ;

  mark->range_top = mark->range_bottom = 0 ;

  zmapWindowMarkSetColour(mark, ZMAP_WINDOW_ITEM_MARK) ;

  mark->stipple = gdk_bitmap_create_from_data(NULL, &mark_bitmap_bits[0], mark_bitmap_width, mark_bitmap_height) ;

  return mark ;
}

gboolean zmapWindowMarkIsSet(ZMapWindowMark mark)
{
  zMapAssert(mark && ZMAP_MAGIC_IS_VALID(mark_magic_G, mark->magic)) ;

  return mark->mark_set ;
}

void zmapWindowMarkReset(ZMapWindowMark mark)
{
  zMapAssert(mark && ZMAP_MAGIC_IS_VALID(mark_magic_G, mark->magic)) ;

  if (mark->mark_set)
    {
      if (mark->range_item)
	{
	  /* undo highlighting */
	  markItem(mark, mark->range_item, FALSE) ;

	  mark->range_item = NULL ;
	}

      mark->range_top = mark->range_bottom = 0 ;

      if (mark->range_group)
	{
	  /* Do not destroy the range_group, its belongs to the block container ! */
	  zmapWindowLongItemRemove(mark->window->long_items, mark->top_range_item) ;
	  gtk_object_destroy(GTK_OBJECT(mark->top_range_item)) ;
	  mark->top_range_item = NULL ;

	  zmapWindowLongItemRemove(mark->window->long_items, mark->bottom_range_item) ;
	  gtk_object_destroy(GTK_OBJECT(mark->bottom_range_item)) ;
	  mark->bottom_range_item = NULL ;
	}

      mark->mark_set = FALSE ;
    }

  return ;
}

void zmapWindowMarkSetColour(ZMapWindowMark mark, char *colour)
{
  zMapAssert(mark && ZMAP_MAGIC_IS_VALID(mark_magic_G, mark->magic)) ;

  gdk_color_parse(colour, &(mark->colour)) ;

  return ;
}

GdkColor *zmapWindowMarkGetColour(ZMapWindowMark mark)
{
  zMapAssert(mark && ZMAP_MAGIC_IS_VALID(mark_magic_G, mark->magic)) ;

  return &(mark->colour) ;
}

/* Mark an item, the marking must be done explicitly, it is unaffected by highlighting. 
 * Note that the item must be a feature item.
 * 
 * The coords are all -1 or +1 to make sure we don't clip the given item with the
 * marking.
 *  */
void zmapWindowMarkSetItem(ZMapWindowMark mark, FooCanvasItem *item)
{
  ZMapFeature feature ;
  double x1, y1, x2, y2 ;

  zMapAssert(mark && ZMAP_MAGIC_IS_VALID(mark_magic_G, mark->magic) && FOO_IS_CANVAS_ITEM(item)) ;

  zmapWindowMarkReset(mark) ;

  mark->range_item = item ;

  /* We need to get the world coords because the item passed in is likely to be a child of a child
   * of a.... */
  my_foo_canvas_item_get_world_bounds(mark->range_item, &x1, &y1, &x2, &y2) ;

  feature = g_object_get_data(G_OBJECT(mark->range_item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature) ;

  mark->range_top = feature->x1 - 1 ;
  mark->range_bottom = feature->x2 + 1 ;

  mark->block_group = zmapWindowContainerGetParentLevel(mark->range_item, ZMAPCONTAINER_LEVEL_BLOCK) ;
  mark->block = g_object_get_data(G_OBJECT(mark->block_group), ITEM_FEATURE_DATA) ;

  markItem(mark, mark->range_item, TRUE) ;

  markRange(mark, y1 - 1, y2 + 1) ;

  mark->mark_set = TRUE ;

  return ;
}

FooCanvasItem *zmapWindowMarkGetItem(ZMapWindowMark mark)
{
  zMapAssert(mark && ZMAP_MAGIC_IS_VALID(mark_magic_G, mark->magic)) ;

  return mark->range_item ;
}


gboolean zmapWindowMarkSetWorldRange(ZMapWindowMark mark,
				     double world_x1, double world_y1, double world_x2, double world_y2)
{
  gboolean result ;
  FooCanvasGroup *block_grp_out ;
  double scroll_x1, scroll_x2;
  int y1_out, y2_out ;

  zMapAssert(mark);
  zMapAssert(ZMAP_MAGIC_IS_VALID(mark_magic_G, mark->magic)) ;

  zmapWindowMarkReset(mark) ;
  
  y1_out = y2_out = 0 ;

  /* clamp x to scroll region. Fix RT # 55131 */
  zmapWindowGetScrollRegion(mark->window, &scroll_x1, NULL, &scroll_x2, NULL);

  if(world_x2 > scroll_x2)
    {
      world_x2 = scroll_x2;
      /* Together with swapping below, appears to fix RT # 68249, thanks to log from RT # 107182 */
      world_x1 = ((scroll_x1) + ((scroll_x2 - scroll_x1) / 2));
    }

  if(world_y1 >= world_y2)
    world_y1 = world_y2 - 1.0;

  if(world_x1 > world_x2)
    ZMAP_SWAP_TYPE(double, world_x1, world_x2);

  if ((result = zmapWindowWorld2SeqCoords(mark->window, world_x1, world_y1, world_x2, world_y2,
					  &block_grp_out, &y1_out, &y2_out)))
    {
      double y1, y2, dummy;

      mark->block_group = block_grp_out ;
      mark->block = g_object_get_data(G_OBJECT(mark->block_group), ITEM_FEATURE_DATA) ;
      mark->world_x1 = world_x1 ;
      mark->world_y1 = world_y1 ;
      mark->world_x2 = world_x2 ;
      mark->world_y2 = world_y2 ;

      mark->range_top = y1_out ;
      mark->range_bottom = y2_out ;

      y1 = mark->world_y1 ;
      y2 = mark->world_y2 ;
      foo_canvas_item_w2i(FOO_CANVAS_ITEM(mark->block_group), &dummy, &y1) ;
      foo_canvas_item_w2i(FOO_CANVAS_ITEM(mark->block_group), &dummy, &y2) ;

      markRange(mark, y1, y2) ;

      mark->mark_set = TRUE ;
    }
  else
    zMapLogWarning("%s", "zmapWindowWorld2SeqCoords failed, possibly due to foo_canvas_get_item_at bug.");

  return result ;
}

gboolean zmapWindowMarkGetWorldRange(ZMapWindowMark mark,
				     double *world_x1, double *world_y1, double *world_x2, double *world_y2)
{
  gboolean result = FALSE ;

  zMapAssert(mark && ZMAP_MAGIC_IS_VALID(mark_magic_G, mark->magic)) ;

  if (mark->mark_set)
    {
      *world_x1 = mark->world_x1 ;
      *world_y1 = mark->world_y1 ;
      *world_x2 = mark->world_x2 ;
      *world_y2 = mark->world_y2 ;

      if(mark->range_item)
	{
	  /* See the markRange() call in zmapWindowMarkSetItem() */
	  (*world_y2)--;
	  (*world_y1)++;
	}

      result = TRUE ;
    }

  return result ;
}

gboolean zmapWindowMarkGetSequenceRange(ZMapWindowMark mark, int *start, int *end)
{
  gboolean result = FALSE ;

  zMapAssert(mark && ZMAP_MAGIC_IS_VALID(mark_magic_G, mark->magic)) ;

  if (mark->mark_set)
    {
      *start = mark->range_top ;
      *end = mark->range_bottom ;

      result = TRUE ;
    }

  return result ;
}

void zmapWindowMarkDestroy(ZMapWindowMark mark)
{
  zMapAssert(mark && ZMAP_MAGIC_IS_VALID(mark_magic_G, mark->magic)) ;

  zmapWindowMarkReset(mark) ;

  g_object_unref(G_OBJECT(mark->stipple)) ;		    /* This seems to be the only way to
							       get rid of a drawable. */

  g_free(mark) ;

  return ;
}




/*
 *               Internal functions
 */


/* Mark/unmark an item with a highlight colour. */
static void markItem(ZMapWindowMark mark, FooCanvasItem *item, gboolean set_mark)
{
  FooCanvasItem *parent ;

  parent = item ;

  if (FOO_IS_CANVAS_GROUP(parent))
    {
      HighlightStruct highlight_data = {NULL} ;
      FooCanvasGroup *group = FOO_CANVAS_GROUP(parent) ;

      highlight_data.mark = mark ;
      highlight_data.highlight = set_mark ;
      
      g_list_foreach(group->item_list, markFuncCB, (void *)&highlight_data) ;
    }
  else
    {
      setBoundingBoxColour(mark, parent, set_mark) ;
    }

  return ;
}


/* This is a g_list callback function. */
static void markFuncCB(gpointer data, gpointer user_data)
{
  FooCanvasItem *item = (FooCanvasItem *)data ;
  Highlight highlight = (Highlight)user_data ;

  setBoundingBoxColour(highlight->mark, item, highlight->highlight) ;

  return ;
}


/* The range markers are implemented as overlays in the block container. This makes sense because
 * a mark is relevant only to its parent block. */
static void markRange(ZMapWindowMark mark, double y1, double y2)
{
  double block_x1, block_y1, block_x2, block_y2, tmp_y1, tmp_y2 ;

  foo_canvas_item_get_bounds(FOO_CANVAS_ITEM(mark->block_group), &block_x1, &block_y1, &block_x2, &block_y2) ;

  zmapWindowExt2Zero(&block_x1, &block_x2) ;
  zmapWindowExt2Zero(&block_y1, &block_y2) ;

  tmp_y1 = y1 ;
  tmp_y2 = y2 ;

  /* This seems wierd but it only really happens when we get marks very close to the end of blocks
   * and it's because items can be slightly bigger than blocks because of the thickness of their
   * lines and foocanvas does not pick this up. So by reversing coords we can calculate these
   * marks zones correctly at the very ends of the blocks. */
  if (tmp_y1 < block_y1)
    {
      double tmp ;

      tmp = tmp_y1 ;
      tmp_y1 = block_y1 ;
      block_y1 = tmp ;
    }
  if (block_y2 < tmp_y2)
    {
      double tmp ;

      tmp = tmp_y2 ;
      tmp_y2 = block_y2 ;
      block_y2 = tmp ;
    }


  /* Only maximise overlays width on resize, do not change height. */
  zmapWindowContainerSetOverlayResizing(mark->block_group, TRUE, FALSE) ;

  mark->range_group = zmapWindowContainerGetOverlays(mark->block_group) ;

  mark->top_range_item = zMapDrawBoxOverlay(FOO_CANVAS_GROUP(mark->range_group), 
					    block_x1, block_y1, block_x2, tmp_y1, 
					    &(mark->colour)) ;

  zmapWindowLongItemCheck(mark->window->long_items, mark->top_range_item, block_y1, tmp_y1) ;

  g_object_set_data(G_OBJECT(mark->top_range_item), "my_range_key", "top range item") ;


  mark->bottom_range_item = zMapDrawBoxOverlay(FOO_CANVAS_GROUP(mark->range_group), 
					       block_x1, tmp_y2, block_x2, block_y2, 
					       &(mark->colour)) ;

  zmapWindowLongItemCheck(mark->window->long_items, mark->bottom_range_item, tmp_y2, block_y2) ;

  g_object_set_data(G_OBJECT(mark->bottom_range_item), "my_range_key", "bottom range item") ;

  mark->world_y1 = y1;
  mark->world_y2 = y2;

  return ;
}



/* 
 * The bounding_box is a bit of a hack to support marking objects,
 * a better way would be to use masks as overlays but I don't have time just
 * now. ALSO....if you put highlighting over the top of the feature you won't
 * be able to click on it any more....agh....something to solve.....
 * 
 */
static void setBoundingBoxColour(ZMapWindowMark mark, FooCanvasItem *item, gboolean highlight)
{
  ZMapWindowItemFeatureType item_feature_type ;
  ZMapFeature item_feature = NULL;
  ZMapFeatureTypeStyle style = NULL;


  item_feature_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), ITEM_FEATURE_TYPE)) ;
  zMapAssert(item_feature_type != ITEM_FEATURE_INVALID) ;

  item_feature = g_object_get_data(G_OBJECT(item), ITEM_FEATURE_DATA) ;
  zMapAssert(item_feature) ;

  style = zmapWindowItemGetStyle(item) ;
  zMapAssert(style) ;

  if (item_feature_type == ITEM_FEATURE_BOUNDING_BOX)
    {
      GdkColor *highlight_colour = NULL ;
      GdkBitmap *stipple ;
      char *highlight_target, *highlight_stipple ;

      highlight_target = "fill_color_gdk" ;
      highlight_stipple = "fill_stipple" ;

      if (highlight)
	{
	  highlight_colour = zmapWindowMarkGetColour(mark) ;
	  stipple = mark->stipple ;
	}
      else
	{
	  highlight_colour = NULL ;
	  stipple = NULL ;
	}

      foo_canvas_item_set(FOO_CANVAS_ITEM(item),
			  highlight_target, highlight_colour,
			  highlight_stipple, stipple,
			  NULL) ;
    }

  return ;
}

