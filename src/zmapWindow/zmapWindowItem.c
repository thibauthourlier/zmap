/*  File: zmapWindowItem.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006-2010: Genome Research Ltd.
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
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *     Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description: Functions to manipulate canvas items.
 *
 * Exported functions: See zmapWindow_P.h
 * HISTORY:
 * Last edited: Jul 29 08:30 2010 (edgrif)
 * Created: Thu Sep  8 10:37:24 2005 (edgrif)
 * CVS info:   $Id: zmapWindowItem.c,v 1.135 2010-07-29 10:04:41 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.h>

#include <math.h>
#include <ZMap/zmapUtils.h>
#include <ZMap/zmapGLibUtils.h>
#include <ZMap/zmapSequence.h>
#include <zmapWindow_P.h>
#include <zmapWindowContainerUtils.h>
#include <zmapWindowItemTextFillColumn.h>
#include <zmapWindowFeatures.h>
#include <zmapWindowContainerFeatureSet_I.h>

/* Used to hold highlight information for the hightlight callback function. */
typedef struct
{
  ZMapWindow window ;
  ZMapWindowMark mark ;
  gboolean highlight ;
} HighlightStruct, *Highlight ;

typedef struct
{
  int start, end;
  FooCanvasItem *item;
} StartEndTextHighlightStruct, *StartEndTextHighlight;

typedef struct
{
  double x1, y1, x2, y2;
} AreaStruct;

typedef struct
{
  ZMapWindow window;
  FooCanvasGroup *block;
  int seq_x, seq_y;
  double wx1, wx2, wy1, wy2;
  gboolean result;
} get_item_at_workaround_struct, *get_item_at_workaround;


typedef struct
{
  ZMapWindow window;
  gboolean multiple_select;
  gint highlighted;
  gint feature_count;
} HighlightContextStruct, *HighlightContext;



#if NOT_USED
static gboolean simple_highlight_region(FooCanvasPoints **points_out,
                                        FooCanvasItem    *subject,
                                        gpointer          user_data);
#endif

static ZMapFeatureContextExecuteStatus highlight_feature(GQuark key, gpointer data, gpointer user_data,
							 char **error_out) ;
static void highlightSequenceItems(ZMapWindow window, ZMapFeatureBlock block,
				   FooCanvasItem *focus_item,
				   ZMapSequenceType seq_type, ZMapFrame frame, int start, int end,
				   gboolean centre_on_region) ;

static gint sortByPositionCB(gconstpointer a, gconstpointer b) ;
static void extract_feature_from_item(gpointer list_data, gpointer user_data);

static void getVisibleCanvas(ZMapWindow window,
			     double *screenx1_out, double *screeny1_out,
			     double *screenx2_out, double *screeny2_out) ;

static FooCanvasItem *zmapWindowItemGetTranslationItemFromItemFrame(ZMapWindow window,
								    ZMapFeatureBlock block, ZMapFrame frame) ;
static FooCanvasItem *translation_item_from_block_frame(ZMapWindow window, char *column_name,
							gboolean require_visible,
							ZMapFeatureBlock block, ZMapFrame frame) ;

static FooCanvasItem *translation_from_block_frame(ZMapWindow window, char *column_name,
							gboolean require_visible,
							ZMapFeatureBlock block, ZMapFrame frame) ;
static void handleHighlightTranslation(gboolean highlight,  gboolean item_highlight,
				       ZMapWindow window, FooCanvasItem *item, ZMapFrame required_frame,
				       ZMapSequenceType coords_type, int region_start, int region_end) ;

static void handleHightlightDNA(gboolean on, gboolean item_highlight,
				ZMapWindow window, FooCanvasItem *item, ZMapFrame required_frame,
				ZMapSequenceType coords_type, int region_start, int region_end) ;


static void fill_workaround_struct(ZMapWindowContainerGroup container,
				   FooCanvasPoints         *points,
				   ZMapContainerLevelType   level,
				   gpointer                 user_data);

static gboolean areas_intersection(AreaStruct *area_1, AreaStruct *area_2, AreaStruct *intersect);
static gboolean areas_intersect_gt_threshold(AreaStruct *area_1, AreaStruct *area_2, double threshold);

#ifdef INTERSECTION_CODE
static gboolean foo_canvas_items_get_intersect(FooCanvasItem *i1, FooCanvasItem *i2, FooCanvasPoints **points_out);
static gboolean foo_canvas_items_intersect(FooCanvasItem *i1, FooCanvasItem *i2, double threshold);
#endif








/* This looks like something we will want to do often.... */
GList *zmapWindowItemSortByPostion(GList *feature_item_list)
{
  GList *sorted_list = NULL ;

  if (feature_item_list)
    sorted_list = g_list_sort(feature_item_list, sortByPositionCB) ;

  return sorted_list ;
}


gboolean zmapWindowItemGetStrandFrame(FooCanvasItem *item, ZMapStrand *set_strand, ZMapFrame *set_frame)
{
  ZMapWindowContainerFeatureSet container;
  ZMapFeature feature ;
  gboolean result = FALSE ;

  /* Retrieve the feature item info from the canvas item. */
  feature = zmapWindowItemGetFeature(item);
  zMapAssert(feature && feature->struct_type == ZMAPFEATURE_STRUCT_FEATURE) ;

  container = (ZMapWindowContainerFeatureSet)zmapWindowContainerCanvasItemGetContainer(item) ;

  *set_strand = container->strand ;
  *set_frame  = container->frame ;

  result = TRUE ;

  return result ;
}

GList *zmapWindowItemListToFeatureList(GList *item_list)
{
  GList *feature_list = NULL;

  g_list_foreach(item_list, extract_feature_from_item, &feature_list);

  return feature_list;
}




/*
 *                     Feature Item highlighting....
 */


/* Highlight a feature or list of related features (e.g. all hits for same query sequence). */
void zMapWindowHighlightObject(ZMapWindow window, FooCanvasItem *item,
			       gboolean replace_highlight_item, gboolean highlight_same_names)
{
  zmapWindowHighlightObject(window, item, replace_highlight_item, highlight_same_names) ;

  return ;
}


void zMapWindowHighlightObjects(ZMapWindow window, ZMapFeatureContext context, gboolean multiple_select)
{
  HighlightContextStruct highlight_data = {NULL};

  highlight_data.window = window;
  highlight_data.multiple_select = multiple_select;
  highlight_data.highlighted = highlight_data.feature_count = 0;

  zMapFeatureContextExecute((ZMapFeatureAny)context, ZMAPFEATURE_STRUCT_FEATURE,
                            highlight_feature, &highlight_data);

  return ;
}

void zmapWindowHighlightObject(ZMapWindow window, FooCanvasItem *item,
			       gboolean replace_highlight_item, gboolean highlight_same_names)
{
  ZMapWindowCanvasItem canvas_item ;
  ZMapFeature feature ;

  canvas_item = zMapWindowCanvasItemIntervalGetObject(item) ;
  zMapAssert(ZMAP_IS_CANVAS_ITEM(canvas_item)) ;


  /* Retrieve the feature item info from the canvas item. */
  feature = zmapWindowItemGetFeature(canvas_item);
  zMapAssert(feature) ;


  /* If any other feature(s) is currently in focus, revert it to its std colours */
  if (replace_highlight_item)
    zMapWindowUnHighlightFocusItems(window) ;


  /* For some types of feature we want to highlight all the ones with the same name in that column. */
  switch (feature->type)
    {
    case ZMAPSTYLE_MODE_ALIGNMENT:
      {
	if (highlight_same_names)
	  {
	    GList *set_items ;
	    char *set_strand, *set_frame ;

	    set_strand = set_frame = "*" ;

	    set_items = zmapWindowFToIFindSameNameItems(window->context_to_item, set_strand, set_frame, feature) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	    if (set_items)
	      zmapWindowFToIPrintList(set_items) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	    zmapWindowFocusAddItems(window->focus, set_items, item) ; // item is the hot one

	    g_list_free(set_items) ;
	  }
	else
	  {
	    zmapWindowFocusAddItem(window->focus, item) ;
	  }

	break ;
      }
    default:
      {
	/* Try highlighting both the item and its column. */
        zmapWindowFocusAddItem(window->focus, item);
      }
      break ;
    }

  zmapWindowFocusClearOverlayManagers(window->focus);

  zmapWindowItemHighlightDNARegion(window, TRUE, item, ZMAPFRAME_NONE, ZMAPSEQUENCE_NONE, feature->x1, feature->x2);

  {
    int frame_itr;

    for (frame_itr = ZMAPFRAME_0; frame_itr < ZMAPFRAME_2 + 1; frame_itr++)
      {
	zmapWindowItemHighlightTranslationRegion(window, TRUE, item,
						 frame_itr, ZMAPSEQUENCE_NONE, feature->x1, feature->x2) ;
      }

  }


  zMapWindowHighlightFocusItems(window);

  return ;
}




void zMapWindowHighlightFocusItems(ZMapWindow window)
{
  FooCanvasItem *hot_item ;
  FooCanvasGroup *hot_column = NULL;

  /* If any other feature(s) is currently in focus, revert it to its std colours */
  if ((hot_column = zmapWindowFocusGetHotColumn(window->focus)))
    zmapWindowFocusHighlightHotColumn(window->focus) ;

  if ((hot_item = zmapWindowFocusGetHotItem(window->focus)))
    zmapWindowFocusHighlightFocusItems(window->focus, window) ;

  return ;
}



void zMapWindowUnHighlightFocusItems(ZMapWindow window)
{
  FooCanvasItem *hot_item ;
  FooCanvasGroup *hot_column ;

  /* If any other feature(s) is currently in focus, revert it to its std colours */
//  zmapWindowFocusUnHighlightHotColumn(window) ;     // done by reset

  if ((hot_item = zmapWindowFocusGetHotItem(window->focus)))
    zmapWindowFocusUnhighlightFocusItems(window->focus, window) ;

  if (hot_column || hot_item)
    zmapWindowFocusReset(window->focus) ;

  return ;
}


/* 
 *                         Functions to get hold of items in various ways.
 */


/*
   (TYPE)zmapWindowItemGetFeatureAny(ITEM, STRUCT_TYPE)

#define zmapWindowItemGetFeatureSet(ITEM)   (ZMapFeatureContext)zmapWindowItemGetFeatureAnyType((ITEM), ZMAPFEATURE_STRUCT_CONTEXT)
#define zmapWindowItemGetFeatureAlign(ITEM) (ZMapFeatureAlign)zmapWindowItemGetFeatureAnyType((ITEM), ZMAPFEATURE_STRUCT_ALIGN)
#define zmapWindowItemGetFeatureBlock(ITEM) (ZMapFeatureBlock)zmapWindowItemGetFeatureAnyType((ITEM), ZMAPFEATURE_STRUCT_BLOCK)
#define zmapWindowItemGetFeatureSet(ITEM)   (ZMapFeatureSet)zmapWindowItemGetFeatureAnyType((ITEM), ZMAPFEATURE_STRUCT_FEATURESET)
#define zmapWindowItemGetFeature(ITEM)      (ZMapFeature)zmapWindowItemGetFeatureAnyType((ITEM), ZMAPFEATURE_STRUCT_FEATURE)
#define zmapWindowItemGetFeatureAny(ITEM)   zmapWindowItemGetFeatureAnyType((ITEM), -1)
 */
#ifdef RDS_IS_MACRO_TOO
ZMapFeatureAny zmapWindowItemGetFeatureAny(FooCanvasItem *item)
{
  ZMapFeatureAny feature_any;
  /* -1 means don't check */
  feature_any = zmapWindowItemGetFeatureAnyType(item, -1);

  return feature_any;
}
#endif

ZMapFeatureAny zmapWindowItemGetFeatureAnyType(FooCanvasItem *item, ZMapFeatureStructType expected_type)
{
  ZMapFeatureAny feature_any = NULL;
  ZMapWindowCanvasItem feature_item ;


  if (ZMAP_IS_CONTAINER_GROUP(item))
    {
      zmapWindowContainerGetFeatureAny((ZMapWindowContainerGroup)item, &feature_any);
    }
  else if ((feature_item = zMapWindowCanvasItemIntervalGetObject(item)))
    {
      feature_any = (ZMapFeatureAny)zMapWindowCanvasItemGetFeature(FOO_CANVAS_ITEM(feature_item)) ;
    }
  else
    {
      zMapLogMessage("Unexpected item [%s]", G_OBJECT_TYPE_NAME(item));
    }

  if (feature_any && expected_type != -1)
    {
      if (feature_any->struct_type != expected_type)
	{
	  zMapLogCritical("Unexpected feature type [%d] attached to item [%s]",
			  feature_any->struct_type, G_OBJECT_TYPE_NAME(item));
	  feature_any = NULL;
	}
    }

  return feature_any;
}


/* Get "parent" item of feature, for simple features, this is just the item itself but
 * for compound features we need the parent group.
 *  */
FooCanvasItem *zmapWindowItemGetTrueItem(FooCanvasItem *item)
{
  FooCanvasItem *true_item = NULL ;

  true_item = item;

  return true_item ;
}


/* Get nth child of a compound feature (e.g. transcript, alignment etc), index starts
 * at zero for the first child. */
FooCanvasItem *zmapWindowItemGetNthChild(FooCanvasGroup *compound_item, int child_index)
{
  FooCanvasItem *nth_item = NULL ;

  zMapAssertNotReached();

  return nth_item ;
}

/* Need to test whether this works for groups...it should do....
 *
 * For simple features or the parent of a compound feature the raise is done on the item
 * directly, for compound objects we want to raise the parent so that the whole item
 * is still raised.
 *  */
void zmapWindowRaiseItem(FooCanvasItem *item)
{

  foo_canvas_item_raise_to_top(item) ;
#ifdef RDS_DONT_INCLUDE
  /* this raises the container features group! Not good. */
  foo_canvas_item_raise_to_top(item->parent) ;
#endif /* RDS_DONT_INCLUDE */
  return ;
}





/* THESE FUNCTIONS ALL FEEL LIKE THEY SHOULD BE IN THE items directory somewhere..... */


/* Returns the container parent of the given canvas item which may not be feature, it might be
   some decorative box, e.g. as in the alignment colinear bars. */
FooCanvasGroup *zmapWindowItemGetParentContainer(FooCanvasItem *feature_item)
{
  FooCanvasGroup *parent_container = NULL ;

  parent_container = (FooCanvasGroup *)zmapWindowContainerCanvasItemGetContainer(feature_item);

  return parent_container ;
}


FooCanvasItem *zmapWindowItemGetDNAParentItem(ZMapWindow window, FooCanvasItem *item)
{
  ZMapFeature feature;
  ZMapFeatureBlock block = NULL;
  FooCanvasItem *dna_item = NULL;
  GQuark feature_set_unique = 0, dna_id = 0;
  char *feature_name = NULL;

  feature_set_unique = zMapStyleCreateID(ZMAP_FIXED_STYLE_DNA_NAME);

  if ((feature = zmapWindowItemGetFeature(item)))
    {
      if((block = (ZMapFeatureBlock)(zMapFeatureGetParentGroup((ZMapFeatureAny)feature, ZMAPFEATURE_STRUCT_BLOCK))) &&
         (feature_name = zMapFeatureDNAFeatureName(block)))
        {
          dna_id = zMapFeatureCreateID(ZMAPSTYLE_MODE_RAW_SEQUENCE,
                                       feature_name,
                                       ZMAPSTRAND_FORWARD, /* ALWAYS FORWARD */
                                       block->block_to_sequence.q1,
                                       block->block_to_sequence.q2,
                                       0,0);
          g_free(feature_name);
        }

      if((dna_item = zmapWindowFToIFindItemFull(window->context_to_item,
						block->parent->unique_id,
						block->unique_id,
						feature_set_unique,
						ZMAPSTRAND_FORWARD, /* STILL ALWAYS FORWARD */
						ZMAPFRAME_NONE,/* NO STRAND */
						dna_id)))
	{
	  if(!(FOO_CANVAS_ITEM(dna_item)->object.flags & FOO_CANVAS_ITEM_VISIBLE))
	    dna_item = NULL;
	}
    }
  else
    {
      zMapAssertNotReached();
    }

  return dna_item;
}

FooCanvasItem *zmapWindowItemGetDNATextItem(ZMapWindow window, FooCanvasItem *item)
{
  FooCanvasItem *dna_item = NULL ;
  ZMapFeatureAny feature_any = NULL ;
  ZMapFeatureBlock block = NULL ;

  if ((feature_any = zmapWindowItemGetFeatureAny(item))
      && (block = (ZMapFeatureBlock)zMapFeatureGetParentGroup(feature_any, ZMAPFEATURE_STRUCT_BLOCK)))
    {
      GQuark dna_set_id ;
      GQuark dna_id ;

      dna_set_id = zMapFeatureSetCreateID(ZMAP_FIXED_STYLE_DNA_NAME);

      dna_id = zMapFeatureDNAFeatureID(block);


      if ((dna_item = zmapWindowFToIFindItemFull(window->context_to_item,
						 block->parent->unique_id,
						 block->unique_id,
						 dna_set_id,
						 ZMAPSTRAND_FORWARD, /* STILL ALWAYS FORWARD */
						 ZMAPFRAME_NONE, /* NO FRAME */
						 dna_id)))
	{

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  /* this ought to work but it doesn't, I think this may because the parent (column) gets
	   * unmapped but not the dna text item, so it's not visible any more but it is mapped ? */
	  if(!(FOO_CANVAS_ITEM(dna_item)->object.flags & FOO_CANVAS_ITEM_VISIBLE))
	    dna_item = NULL;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	  if (!(dna_item->object.flags & FOO_CANVAS_ITEM_MAPPED))
	    dna_item = NULL;
	}
    }

  return dna_item ;
}




/* 
 * Sequence highlighting functions, these feel like they should be in the
 * sequence item class objects.
 */


void zmapWindowHighlightSequenceItem(ZMapWindow window, FooCanvasItem *item)
{
  ZMapFeatureAny feature_any ;

  if ((feature_any = zmapWindowItemGetFeatureAny(FOO_CANVAS_ITEM(item))))
    {
      ZMapFeatureBlock block ;

      block = (ZMapFeatureBlock)(zMapFeatureGetParentGroup((ZMapFeatureAny)(feature_any), ZMAPFEATURE_STRUCT_BLOCK));
      zMapAssert(block);

      highlightSequenceItems(window, block, item, ZMAPSEQUENCE_NONE, ZMAPFRAME_NONE, 0, 0, FALSE) ;
    }

  return ;
}

/* What item should be passed in here ??? */
void zmapWindowHighlightSequenceRegion(ZMapWindow window, ZMapFeatureBlock block,
				       ZMapSequenceType seq_type, ZMapFrame frame, int start, int end,
				       gboolean centre_on_region)
{
  highlightSequenceItems(window, block, NULL, seq_type, frame, start, end, centre_on_region) ;

  return ;
}



/* OK...THIS IS THE NUB OF THE MATTER...CHECK OUT WHAT THE ITEM CALLS DO...AND SORT OUT THE COORDS
   JUNK..... */


static void highlightSequenceItems(ZMapWindow window, ZMapFeatureBlock block,
				   FooCanvasItem *focus_item, 
				   ZMapSequenceType seq_type, ZMapFrame frame, int start, int end,
				   gboolean centre_on_region)
{
  FooCanvasItem *item ;
  GQuark set_id ;
  ZMapFrame tmp_frame ;
  ZMapStrand tmp_strand ;
  gboolean done_centring = FALSE ;


  /* If there is a dna column then highlight match in that. */
  tmp_strand = ZMAPSTRAND_NONE ;
  tmp_frame = ZMAPFRAME_NONE ;
  set_id = zMapStyleCreateID(ZMAP_FIXED_STYLE_DNA_NAME) ;

  if ((item = zmapWindowFToIFindItemFull(window->context_to_item,
					 block->parent->unique_id, block->unique_id,
					 set_id, tmp_strand, tmp_frame, 0)))
    {
      int dna_start, dna_end ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      zmapWindowItemTrueTypePrint(item) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

      dna_start = start ;
      dna_end = end ;

      /* If it's a peptide hit, convert peptide start/end to dna sequence coords. */
      if (seq_type == ZMAPSEQUENCE_PEPTIDE)
	zMapSequencePep2DNA(&dna_start, &dna_end, frame) ;

      zmapWindowItemHighlightDNARegion(window, FALSE, item, frame, seq_type, dna_start, dna_end) ;

      if (centre_on_region)
	{
	  zmapWindowItemCentreOnItemSubPart(window, item, FALSE, 0.0, dna_start, dna_end) ;
	  done_centring = TRUE ;
	}
    }


  /* If there is a peptide column then highlight match in that. */
  tmp_strand = ZMAPSTRAND_NONE ;
  tmp_frame = ZMAPFRAME_NONE ;
  set_id = zMapStyleCreateID(ZMAP_FIXED_STYLE_3FT_NAME) ;

  if ((item = zmapWindowFToIFindItemFull(window->context_to_item,
					 block->parent->unique_id, block->unique_id,
					 set_id, tmp_strand, tmp_frame, 0)))
    {
      int frame_num, pep_start, pep_end ;

      for (frame_num = ZMAPFRAME_0 ; frame_num < ZMAPFRAME_2 + 1 ; frame_num++)
	{
	  pep_start = start ;
	  pep_end = end ;

	  /* If it's a dna hit, convert dna coords to peptide. */
	  if (seq_type == ZMAPSEQUENCE_DNA)
	    zMapSequenceDNA2Pep(&pep_start, &pep_end, frame_num) ;

	  /* slightly tricky...if it's a peptide match then we should only highlight in
	   * the right frame, the others are by definition not a match. */
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  if (seq_type == ZMAPSEQUENCE_DNA || frame_num == frame)
	    zmapWindowItemHighlightTranslationRegion(window, item, frame_num, pep_start, pep_end) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	  if (seq_type == ZMAPSEQUENCE_DNA)
	    {
	      zmapWindowItemHighlightTranslationRegion(window, FALSE, item, frame_num, seq_type, pep_start, pep_end) ;
	    }
	  else
	    {
	      if (frame_num == frame)
		zmapWindowItemHighlightTranslationRegion(window, FALSE, item,
							 frame_num, seq_type, pep_start, pep_end) ;
	      else
		zmapWindowItemUnHighlightTranslation(window, item, frame_num) ;
	    }
	}

      if (centre_on_region && !done_centring)
	zmapWindowItemCentreOnItemSubPart(window, item, FALSE, 0.0, start, end) ;		
    }


  return ;
}



/* highlights the dna given any foocanvasitem (with a feature) and a start and end
 * This _only_ highlights in the current window! */
void zmapWindowItemHighlightDNARegion(ZMapWindow window, gboolean item_highlight,
				      FooCanvasItem *item, ZMapFrame required_frame,
				      ZMapSequenceType coords_type, int region_start, int region_end)
{
  handleHightlightDNA(TRUE, item_highlight, window, item, required_frame,
		      coords_type, region_start, region_end) ;

  return ;
}


void zmapWindowItemUnHighlightDNA(ZMapWindow window, FooCanvasItem *item)
{
  handleHightlightDNA(FALSE, FALSE, window, item, ZMAPFRAME_NONE, ZMAPSEQUENCE_NONE, 0, 0) ;

  return ;
}



static void handleHightlightDNA(gboolean on, gboolean item_highlight,
				ZMapWindow window, FooCanvasItem *item, ZMapFrame required_frame,
				ZMapSequenceType coords_type, int region_start, int region_end)
{
  FooCanvasItem *dna_item ;

  if ((dna_item = zmapWindowItemGetDNATextItem(window, item))
      && ZMAP_IS_WINDOW_SEQUENCE_FEATURE(dna_item) && item != dna_item)
    {
      ZMapWindowSequenceFeature sequence_feature ;
      ZMapFeature feature ;

      sequence_feature = (ZMapWindowSequenceFeature)dna_item ;

      /* highlight specially for transcripts (i.e. exons). */
      if (on)
	{
	  if (item_highlight && (feature = zmapWindowItemGetFeature(item)))
	    {
	      zMapWindowSequenceFeatureSelectByFeature(sequence_feature, feature) ;
	    }
	  else
	    {
	      int tmp_start, tmp_end ;

	      tmp_start = region_start ;
	      tmp_end = region_end ;

	      /* this needs to pass into selectbyregion call.... */
	      if (coords_type == ZMAPSEQUENCE_PEPTIDE)
		zMapSequencePep2DNA(&tmp_start, &tmp_end, required_frame) ;

	      zMapWindowSequenceFeatureSelectByRegion(sequence_feature, coords_type, tmp_start, tmp_end) ;
	    }
	}
      else
	{
	  zMapWindowSequenceDeSelect(sequence_feature) ;
	}
    }

  return ;
}


void zmapWindowItemHighlightTranslationRegions(ZMapWindow window, gboolean item_highlight,
					       FooCanvasItem *item,
					       ZMapSequenceType coords_type,
					       int region_start, int region_end)
{
  ZMapFrame frame ;

  for (frame = ZMAPFRAME_0 ; frame < ZMAPFRAME_2 + 1 ; frame++)
    {
      zmapWindowItemHighlightTranslationRegion(window, item_highlight,
					       item, frame,
					       coords_type, region_start, region_end) ;
    }

  return ;
}



/* highlights the translation given any foocanvasitem (with a
 * feature), frame and a start and end (protein seq coords) */
/* This _only_ highlights in the current window! */
void zmapWindowItemHighlightTranslationRegion(ZMapWindow window, gboolean item_highlight,
					      FooCanvasItem *item,
					      ZMapFrame required_frame,
					      ZMapSequenceType coords_type, int region_start, int region_end)
{
  handleHighlightTranslation(TRUE, item_highlight, window, item,
			     required_frame, coords_type, region_start, region_end) ;

  return ;
}

void zmapWindowItemUnHighlightTranslation(ZMapWindow window, FooCanvasItem *item, ZMapFrame required_frame)
{
  handleHighlightTranslation(FALSE, FALSE, window, item, required_frame, ZMAPSEQUENCE_NONE, 0, 0) ;

  return ;
}


static void handleHighlightTranslation(gboolean highlight, gboolean item_highlight,
				       ZMapWindow window, FooCanvasItem *item, ZMapFrame required_frame,
				       ZMapSequenceType coords_type, int region_start, int region_end)
{
  FooCanvasItem *translation_item = NULL;
  ZMapFeatureAny feature_any ;
  ZMapFeatureBlock block ;

  if ((feature_any = zmapWindowItemGetFeatureAny(item)))
    {
      block = (ZMapFeatureBlock)(zMapFeatureGetParentGroup((ZMapFeatureAny)(feature_any), ZMAPFEATURE_STRUCT_BLOCK)) ;

      if ((translation_item = zmapWindowItemGetTranslationItemFromItemFrame(window, block, required_frame)))
	{
	  ZMapWindowSequenceFeature sequence_feature ;
	  ZMapFeature feature ;

	  zmapWindowItemTrueTypePrint(translation_item) ;

	  sequence_feature = (ZMapWindowSequenceFeature)translation_item ;

	  if (highlight)
	    {
	      if (item_highlight && (feature = zmapWindowItemGetFeature(item)))
		{
		  zMapWindowSequenceFeatureSelectByFeature(sequence_feature, feature) ;
		}
	      else
		{
		  int tmp_start, tmp_end ;

		  tmp_start = region_start ;
		  tmp_end = region_end ;

		  /* this needs to pass into selectbyregion call.... */
		  if (coords_type == ZMAPSEQUENCE_DNA)
		    {
		      zMapSequenceDNA2Pep(&tmp_start, &tmp_end, required_frame) ;
		    }

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
		  else
		    {
		      zMapSequencePep2DNA(&tmp_start, &tmp_end, required_frame) ;

		      zMapSequenceDNA2Pep(&tmp_start, &tmp_end, required_frame) ;
		    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


		  zMapWindowSequenceFeatureSelectByRegion(sequence_feature, ZMAPSEQUENCE_DNA, tmp_start, tmp_end) ;
		}
	    }
	  else
	    {
	      zMapWindowSequenceDeSelect(sequence_feature) ;
	    }
	}
    }

  return ;
}



static FooCanvasItem *zmapWindowItemGetTranslationItemFromItemFrame(ZMapWindow window,
								    ZMapFeatureBlock block, ZMapFrame frame)
{
  FooCanvasItem *translation = NULL;

  /* Get the frame for the item... and its translation feature (ITEM_FEATURE_PARENT!) */
  translation = translation_from_block_frame(window, ZMAP_FIXED_STYLE_3FT_NAME, TRUE, block, frame);

  return translation;
}


ZMapFrame zmapWindowItemFeatureFrame(FooCanvasItem *item)
{
  ZMapFeatureSubPartSpan item_subfeature_data ;
  ZMapFeature feature;
  ZMapFrame frame = ZMAPFRAME_NONE;

  if((feature = zmapWindowItemGetFeature(item)))
    {
      frame = zmapWindowFeatureFrame(feature);

      if((item_subfeature_data = g_object_get_data(G_OBJECT(item), ITEM_SUBFEATURE_DATA)))
        {
          int diff = item_subfeature_data->start - feature->x1;
          int sub_frame = diff % 3;

          frame -= ZMAPFRAME_0;
          frame += sub_frame;
          frame  = (frame % 3) + ZMAPFRAME_0;
        }
    }

  return frame;
}

FooCanvasGroup *zmapWindowItemGetTranslationColumnFromBlock(ZMapWindow window, ZMapFeatureBlock block)
{
  ZMapFeatureSet feature_set;
  FooCanvasItem *translation;
  GQuark feature_set_id;

  feature_set_id = zMapStyleCreateID(ZMAP_FIXED_STYLE_3FT_NAME);
  /* and look up the translation feature set with ^^^ */
  feature_set  = zMapFeatureBlockGetSetByID(block, feature_set_id);

  translation  = zmapWindowFToIFindSetItem(window->context_to_item,
                                           feature_set,
                                           ZMAPSTRAND_FORWARD, /* STILL ALWAYS FORWARD */
                                           ZMAPFRAME_NONE);


  return FOO_CANVAS_GROUP(translation);
}


FooCanvasItem *zmapWindowItemGetShowTranslationColumn(ZMapWindow window, FooCanvasItem *item)
{
  FooCanvasItem *translation = NULL;
  ZMapFeature feature;
  ZMapFeatureBlock block;

  if ((feature = zmapWindowItemGetFeature(item)))
    {
      ZMapFeatureSet feature_set;
      ZMapFeatureTypeStyle style;

      /* First go up to block... */
      block = (ZMapFeatureBlock)(zMapFeatureGetParentGroup((ZMapFeatureAny)(feature), ZMAPFEATURE_STRUCT_BLOCK));
      zMapAssert(block);

      /* Get the frame for the item... and its translation feature (ITEM_FEATURE_PARENT!) */
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      if ((style = zMapFeatureContextFindStyle((ZMapFeatureContext)(block->parent->parent),
					       ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME))
	  && !(feature_set = zMapFeatureBlockGetSetByID(block,
							zMapStyleCreateID(ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME))))
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
	if ((style = zMapFindStyle(window->read_only_styles, zMapStyleCreateID(ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME)))
	    && !(feature_set = zMapFeatureBlockGetSetByID(block,
							  zMapStyleCreateID(ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME))))

	{
	  /* Feature set doesn't exist, so create. */
	  feature_set = zMapFeatureSetCreate(ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME, NULL);
	  //feature_set->style = style;
	  zMapFeatureBlockAddFeatureSet(block, feature_set);
	}

      if (feature_set)
	{
	  ZMapWindowContainerGroup parent_container;
	  ZMapWindowContainerStrand forward_container;
	  ZMapWindowContainerFeatures forward_features;
	  FooCanvasGroup *tmp_forward, *tmp_reverse;

#ifdef SIMPLIER
	  FooCanvasGroup *forward_group, *parent_group, *tmp_forward, *tmp_reverse ;
	  /* Get the FeatureSet Level Container */
	  parent_group = zmapWindowContainerCanvasItemGetContainer(item);
	  /* Get the Strand Level Container (Could be Forward OR Reverse) */
	  parent_group = zmapWindowContainerGetSuperGroup(parent_group);
	  /* Get the Block Level Container... */
	  parent_group = zmapWindowContainerGetSuperGroup(parent_group);
#endif /* SIMPLIER */

	  parent_container = zmapWindowContainerUtilsItemGetParentLevel(item, ZMAPCONTAINER_LEVEL_BLOCK);

	  /* Get the Forward Group Parent Container... */
	  forward_container = zmapWindowContainerBlockGetContainerStrand((ZMapWindowContainerBlock)parent_container, ZMAPSTRAND_FORWARD);
	  /* zmapWindowCreateSetColumns needs the Features not the Parent. */
	  forward_features  = zmapWindowContainerGetFeatures((ZMapWindowContainerGroup)forward_container);

	  /* make the column... */
	  if (zmapWindowCreateSetColumns(window,
					 forward_features,
					 NULL,
					 block,
					 feature_set,
					 window->read_only_styles,
					 ZMAPFRAME_NONE,
					 &tmp_forward, &tmp_reverse, NULL))
	    {
	      translation = FOO_CANVAS_ITEM(tmp_forward);
	    }
	}
      else
	zMapLogWarning("Failed to find Feature Set for '%s'", ZMAP_FIXED_STYLE_SHOWTRANSLATION_NAME);
    }
  else
    zMapAssertNotReached();

  return translation;
}

#ifdef RDS_NEVER
ZMapFeatureTypeStyle zmapWindowItemGetStyle(ZMapWindow window, FooCanvasItem *item)
{
  ZMapWindowCanvasItem canvas_item = NULL;
  ZMapFeatureTypeStyle style = NULL;
  ZMapFeature feature = NULL;

  canvas_item = zMapWindowCanvasItemIntervalGetTopLevelObject(item);

  if((canvas_item != NULL) &&
     (feature = zMapWindowCanvasItemGetFeature(canvas_item)))
    {
      style = zMapFindStyle(window->display_styles, feature->style_id);
    }

  return style;
}
#endif




/* Finds the feature item in a window corresponding to the supplied feature item..which is
 * usually one from a different window....
 *
 * This routine can return NULL if the user has two different sequences displayed and hence
 * there will be items in one window that are not present in another.
 *
 *  */
FooCanvasItem *zMapWindowFindFeatureItemByItem(ZMapWindow window, FooCanvasItem *item)
{
  FooCanvasItem *matching_item = NULL ;
  ZMapFeature feature ;
  ZMapWindowContainerFeatureSet container;
  ZMapFeatureSubPartSpan item_subfeature_data ;

  /* Retrieve the feature item info from the canvas item. */
  feature = zMapWindowCanvasItemGetFeature(item) ;
  zMapAssert(feature);


  container = (ZMapWindowContainerFeatureSet)zmapWindowContainerCanvasItemGetContainer(item) ;

  if ((item_subfeature_data = (ZMapFeatureSubPartSpan)g_object_get_data(G_OBJECT(item),
									ITEM_SUBFEATURE_DATA)))
    {
      matching_item = zmapWindowFToIFindItemChild(window->context_to_item,
						  container->strand, container->frame,
						  feature,
						  item_subfeature_data->start,
						  item_subfeature_data->end) ;
    }
  else
    {
      matching_item = zmapWindowFToIFindFeatureItem(window->context_to_item,
						    container->strand, container->frame,
						    feature) ;
    }

  return matching_item ;
}


/* Finds the feature item child in a window corresponding to the supplied feature item..which is
 * usually one from a different window....
 * A feature item child is something like the feature item representing an exon within a transcript. */
FooCanvasItem *zMapWindowFindFeatureItemChildByItem(ZMapWindow window, FooCanvasItem *item,
						    int child_start, int child_end)
{
  FooCanvasItem *matching_item = NULL ;
  ZMapFeature feature ;
  ZMapWindowContainerFeatureSet container;

  zMapAssert(window && item && child_start > 0 && child_end > 0 && child_start <= child_end) ;


  /* Retrieve the feature item info from the canvas item. */
  feature = zmapWindowItemGetFeature(item);
  zMapAssert(feature) ;

  container = (ZMapWindowContainerFeatureSet)zmapWindowContainerCanvasItemGetContainer(item) ;

  /* Find the item that matches */
  matching_item = zmapWindowFToIFindFeatureItem(window->context_to_item,
						container->strand, container->frame, feature) ;

  return matching_item ;
}


/* 
 *                  Testing items visibility and scrolling to those items.
 */

/* Checks to see if the item really is visible.  In order to do this
 * all the item's parent groups need to be examined.
 *
 *  mh17:not called from anywhere
 */
gboolean zmapWindowItemIsVisible(FooCanvasItem *item)
{
  gboolean visible = FALSE;

  visible = zmapWindowItemIsShown(item);

#ifdef RDS_DONT
  /* we need to check out our parents :( */
  /* we would like not to do this */
  if(visible)
    {
      FooCanvasGroup *parent = NULL;
      parent = FOO_CANVAS_GROUP(item->parent);
      while(visible && parent)
        {
          visible = zmapWindowItemIsShown(FOO_CANVAS_ITEM(parent));
          /* how many parents we got? */
          parent  = FOO_CANVAS_GROUP(FOO_CANVAS_ITEM(parent)->parent);
        }
    }
#endif

  return visible;
}


/* Checks to see if the item is shown.  An item may still not be
 * visible as any one of its parents might be hidden. If a
 * definitive answer is required use zmapWindowItemIsVisible instead. */
gboolean zmapWindowItemIsShown(FooCanvasItem *item)
{
  gboolean visible = FALSE;

  zMapAssert(FOO_IS_CANVAS_ITEM(item)) ;

  g_object_get(G_OBJECT(item),
               "visible", &visible,
               NULL);

  return visible;
}



/* Tests to see whether an item is visible on the screen. If "completely" is TRUE then the
 * item must be completely on the screen otherwise FALSE is returned. */
gboolean zmapWindowItemIsOnScreen(ZMapWindow window, FooCanvasItem *item, gboolean completely)
{
  gboolean is_on_screen = FALSE ;
  double screenx1_out, screeny1_out, screenx2_out, screeny2_out ;
  double itemx1_out, itemy1_out, itemx2_out, itemy2_out ;

  /* Work out which part of the canvas is visible currently. */
  getVisibleCanvas(window, &screenx1_out, &screeny1_out, &screenx2_out, &screeny2_out) ;

  /* Get the items world coords. */
  my_foo_canvas_item_get_world_bounds(item, &itemx1_out, &itemy1_out, &itemx2_out, &itemy2_out) ;

  /* Compare them. */
  if (completely)
    {
      if (itemx1_out >= screenx1_out && itemx2_out <= screenx2_out
	  && itemy1_out >= screeny1_out && itemy2_out <= screeny2_out)
	is_on_screen = TRUE ;
      else
	is_on_screen = FALSE ;
    }
  else
    {
      if (itemx2_out < screenx1_out || itemx1_out > screenx2_out
	  || itemy2_out < screeny1_out || itemy1_out > screeny2_out)
	is_on_screen = FALSE ;
      else
	is_on_screen = TRUE ;
    }

  return is_on_screen ;
}


/* if(!zmapWindowItemRegionIsVisible(window, item))
 *   zmapWindowItemCentreOnItem(window, item, changeRegionSize, boundarySize);
 */
gboolean zmapWindowItemRegionIsVisible(ZMapWindow window, FooCanvasItem *item)
{
  gboolean visible = FALSE;
  double wx1, wx2, wy1, wy2;
  double ix1, ix2, iy1, iy2;
  double dummy_x = 0.0;
  double feature_x1 = 0.0, feature_x2 = 0.0 ;
  ZMapFeature feature;

  foo_canvas_item_get_bounds(item, &ix1, &iy1, &ix2, &iy2);
  foo_canvas_item_i2w(item, &dummy_x, &iy1);
  foo_canvas_item_i2w(item, &dummy_x, &iy2);

  feature = (ZMapFeature) zmapWindowItemGetFeatureAnyType(item, -1) ;
  zMapAssert(feature) ;

  /* Get the features canvas coords (may be very different for align block features... */
  zMapFeature2MasterCoords(feature, &feature_x1, &feature_x2);

  /* Get scroll region (clamped to sequence coords) */
  zmapWindowGetScrollRegion(window, &wx1, &wy1, &wx2, &wy2);

  wx2 = feature_x2 + 1;
  if(feature_x1 >= wx1 && feature_x2 <= wx2  &&
     iy1 >= wy1 && iy2 <= wy2)
    {
      visible = TRUE;
    }

  return visible;
}



/* Scroll to the specified item.
 * If necessary, recalculate the scroll region, then scroll to the item
 * and highlight it.
 */
gboolean zMapWindowScrollToItem(ZMapWindow window, FooCanvasItem *item)
{
  gboolean result = FALSE ;

  zMapAssert(window && item) ;

  if ((result = zmapWindowItemIsShown(item)))
    {
      zmapWindowItemCentreOnItem(window, item, FALSE, 100.0) ;

      result = TRUE ;
    }

  return result ;
}


/* Moves to the given item and optionally changes the size of the scrolled region
 * in order to display the item. */
void zmapWindowItemCentreOnItem(ZMapWindow window, FooCanvasItem *item,
                                gboolean alterScrollRegionSize,
                                double boundaryAroundItem)
{
  zmapWindowItemCentreOnItemSubPart(window, item, alterScrollRegionSize, boundaryAroundItem, 0.0, 0.0) ;

  return ;
}


/* Moves to a subpart of an item, note the coords sub_start/sub_end need to be item coords,
 * NOT world coords. If sub_start == sub_end == 0.0 then they are ignored. */
void zmapWindowItemCentreOnItemSubPart(ZMapWindow window, FooCanvasItem *item,
				       gboolean alterScrollRegionSize,
				       double boundaryAroundItem,
				       double sub_start, double sub_end)
{
  double ix1, ix2, iy1, iy2;
  int cx1, cx2, cy1, cy2;
  int final_canvasx, final_canvasy, tmpx, tmpy, cheight, cwidth;
  gboolean debug = FALSE;

  zMapAssert(window && item) ;

  if (zmapWindowItemIsShown(item))
    {
      /* THIS CODE IS NOT GREAT AND POINTS TO SOME FUNDAMENTAL ROUTINES/UNDERSTANDING THAT IS
       * MISSING.....WE NEED TO SORT OUT WHAT SIZE WE NEED TO CALCULATE FROM AS OPPOSED TO 
       * WHAT THE CURRENTLY DISPLAYED OBJECT SIZE IS, THIS MAY HAVE BEEN TRUNCATED BY THE
       * THE LONG ITEM CLIP CODE OR JUST BY CHANGING THE SCROLLED REGION. */

      foo_canvas_item_get_bounds(item, &ix1, &iy1, &ix2, &iy2) ;


      /* If the item is a group then we need to use its background to check in long items as the
       * group itself is not a long item. */
      if (FOO_IS_CANVAS_GROUP(item) && zmapWindowContainerUtilsIsValid(FOO_CANVAS_GROUP(item)))
	{
	  FooCanvasItem *long_item ;
	  double height ;


	  /* this code tries to deal with long items but fails to deal with the zooming and the
	   * changing scrolled region.... */

	  /* Item may have been clipped by long items code so reinstate its true bounds. */
	  long_item = (FooCanvasItem *)zmapWindowContainerGetBackground(ZMAP_CONTAINER_GROUP(item)) ;

	  my_foo_canvas_item_get_long_bounds(window->long_items, long_item,
					     &ix1, &iy1, &ix2, &iy2) ;

	  /* If we are using the background then we should use it's height as originally set. */
	  height = zmapWindowContainerGroupGetBackgroundSize(ZMAP_CONTAINER_GROUP(item)) ;

	  if (iy1 > 0)
	    iy1 = 0 ;
	  if (iy2 < height)
	    iy2 = height ;
	}
      else
	{
	  foo_canvas_item_get_bounds(item, &ix1, &iy1, &ix2, &iy2) ;
	}

      if (sub_start != 0.0 || sub_end != 0.0)
	{
	  zMapAssert(sub_start <= sub_end
		     && (sub_start >= iy1 && sub_start <= iy2)
		     && (sub_end >= iy1 && sub_end <= iy2)) ;

	  iy2 = iy1 ;
	  iy1 = iy1 + sub_start ;
	  iy2 = iy2 + sub_end ;
	}


      /* Fix the numbers to make sense. */
      foo_canvas_item_i2w(item, &ix1, &iy1);
      foo_canvas_item_i2w(item, &ix2, &iy2);


      /* the item coords are now WORLD */


      if (boundaryAroundItem > 0.0)
	{
	  /* think about PPU for X & Y!! */
	  ix1 -= boundaryAroundItem;
	  iy1 -= boundaryAroundItem;
	  ix2 += boundaryAroundItem;
	  iy2 += boundaryAroundItem;
	}

      if(alterScrollRegionSize)
	{
	  /* this doeesn't work. don't know why. */
	  double sy1, sy2;
	  sy1 = iy1; sy2 = iy2;
	  zmapWindowClampSpan(window, &sy1, &sy2);
	  zMapWindowMove(window, sy1, sy2);
	}
      else
	{
	  if(!zmapWindowItemRegionIsVisible(window, item))
	    {
	      double sx1, sx2, sy1, sy2, tmps, tmpi, diff;
	      /* Get scroll region (clamped to sequence coords) */
	      zmapWindowGetScrollRegion(window, &sx1, &sy1, &sx2, &sy2);
	      /* we now have scroll region coords */
	      /* set tmp to centre of regions ... */
	      tmps = sy1 + ((sy2 - sy1) / 2);
	      tmpi = iy1 + ((iy2 - iy1) / 2);
	      /* find difference between centre points */
	      diff = tmps - tmpi;

	      /* alter scroll region to match that */
	      sy1 -= diff; sy2 -= diff;
	      /* clamp in case we've moved outside the sequence */
	      zmapWindowClampSpan(window, &sy1, &sy2);
	      /* Do the move ... */
	      zMapWindowMove(window, sy1, sy2);
	    }
	}



      /* THERE IS A PROBLEM WITH HORIZONTAL SCROLLING HERE, THE CODE DOES NOT SCROLL FAR
       * ENOUGH TO THE RIGHT...I'M NOT SURE WHY THIS IS AT THE MOMENT. */


      /* get canvas coords to work with */
      foo_canvas_w2c(item->canvas, ix1, iy1, &cx1, &cy1);
      foo_canvas_w2c(item->canvas, ix2, iy2, &cx2, &cy2);

      if(debug)
	{
	  printf("[zmapWindowItemCentreOnItem] ix1=%f, ix2=%f, iy1=%f, iy2=%f\n", ix1, ix2, iy1, iy2);
	  printf("[zmapWindowItemCentreOnItem] cx1=%d, cx2=%d, cy1=%d, cy2=%d\n", cx1, cx2, cy1, cy2);
	}

      /* This should possibly be a function */
      tmpx = cx2 - cx1; tmpy = cy2 - cy1;
      if(tmpx & 1)
	tmpx += 1;
      if(tmpy & 1)
	tmpy += 1;
      final_canvasx = cx1 + (tmpx / 2);
      final_canvasy = cy1 + (tmpy / 2);

      tmpx = GTK_WIDGET(window->canvas)->allocation.width;
      tmpy = GTK_WIDGET(window->canvas)->allocation.height;
      if(tmpx & 1)
	tmpx -= 1;
      if(tmpy & 1)
	tmpy -= 1;
      cwidth = tmpx / 2; cheight = tmpy / 2;
      final_canvasx -= cwidth;
      final_canvasy -= cheight;

      if(debug)
	{
	  printf("[zmapWindowItemCentreOnItem] cwidth=%d  cheight=%d\n", cwidth, cheight);
	  printf("[zmapWindowItemCentreOnItem] scrolling to"
		 " canvas x=%d y=%d\n",
		 final_canvasx, final_canvasy);
	}

      /* Scroll to the item... */
      foo_canvas_scroll_to(window->canvas, final_canvasx, final_canvasy);
    }

  return ;
}


/* Scrolls to an item if that item is not visible on the scren.
 *
 * NOTE: scrolling is only done in the axis in which the item is completely
 * invisible, the other axis is left unscrolled so that the visible portion
 * of the feature remains unaltered.
 *  */
void zmapWindowScrollToItem(ZMapWindow window, FooCanvasItem *item)
{
  double screenx1_out, screeny1_out, screenx2_out, screeny2_out ;
  double itemx1_out, itemy1_out, itemx2_out, itemy2_out ;
  gboolean do_x = FALSE, do_y = FALSE ;
  double x_offset = 0.0, y_offset = 0.0;
  int curr_x, curr_y ;

  /* Work out which part of the canvas is visible currently. */
  getVisibleCanvas(window, &screenx1_out, &screeny1_out, &screenx2_out, &screeny2_out) ;

  /* Get the items world coords. */
  my_foo_canvas_item_get_world_bounds(item, &itemx1_out, &itemy1_out, &itemx2_out, &itemy2_out) ;

  /* Get the current scroll offsets in world coords. */
  foo_canvas_get_scroll_offsets(window->canvas, &curr_x, &curr_y) ;
  foo_canvas_c2w(window->canvas, curr_x, curr_y, &x_offset, &y_offset) ;

  /* Work out whether any scrolling is required. */
  if (itemx1_out > screenx2_out || itemx2_out < screenx1_out)
    {
      do_x = TRUE ;

      if (itemx1_out > screenx2_out)
	{
	  x_offset = itemx1_out ;
	}
      else if (itemx2_out < screenx1_out)
	{
	  x_offset = itemx1_out ;
	}
    }

  if (itemy1_out > screeny2_out || itemy2_out < screeny1_out)
    {
      do_y = TRUE ;

      if (itemy1_out > screeny2_out)
	{
	  y_offset = itemy1_out ;
	}
      else if (itemy2_out < screeny1_out)
	{
	  y_offset = itemy1_out ;
	}
    }

  /* If we need to scroll then do it. */
  if (do_x || do_y)
    {
      foo_canvas_w2c(window->canvas, x_offset, y_offset, &curr_x, &curr_y) ;

      foo_canvas_scroll_to(window->canvas, curr_x, curr_y) ;
    }

  return ;
}





/* 
 *              some coord printing funcs.
 */



/* Prints out an items coords in local coords, good for debugging.... */
void zmapWindowPrintLocalCoords(char *msg_prefix, FooCanvasItem *item)
{
  double x1, y1, x2, y2 ;

  /* Gets bounding box in parents coord system. */
  foo_canvas_item_get_bounds(item, &x1, &y1, &x2, &y2) ;

  printf("%s:\t%f,%f -> %f,%f\n",
	 (msg_prefix ? msg_prefix : ""),
	 x1, y1, x2, y2) ;


  return ;
}



/* Prints out an items coords in world coords, good for debugging.... */
void zmapWindowPrintItemCoords(FooCanvasItem *item)
{
  double x1, y1, x2, y2 ;

  /* Gets bounding box in parents coord system. */
  foo_canvas_item_get_bounds(item, &x1, &y1, &x2, &y2) ;

  printf("P %f, %f, %f, %f -> ", x1, y1, x2, y2) ;

  foo_canvas_item_i2w(item, &x1, &y1) ;
  foo_canvas_item_i2w(item, &x2, &y2) ;

  printf("W %f, %f, %f, %f\n", x1, y1, x2, y2) ;

  return ;
}


/* Converts given world coords to an items coord system and prints them. */
void zmapWindowPrintW2I(FooCanvasItem *item, char *text, double x1_in, double y1_in)
{
  double x1 = x1_in, y1 = y1_in;

  my_foo_canvas_item_w2i(item, &x1, &y1) ;

  if (!text)
    text = "Item" ;

  printf("%s -  world(%f, %f)  ->  item(%f, %f)\n", text, x1_in, y1_in, x1, y1) ;

  return ;
}

/* Converts coords in an items coord system into world coords and prints them. */
/* Prints out item coords position in world and its parents coords.... */
void zmapWindowPrintI2W(FooCanvasItem *item, char *text, double x1_in, double y1_in)
{
  double x1 = x1_in, y1 = y1_in;

  my_foo_canvas_item_i2w(item, &x1, &y1) ;

  if (!text)
    text = "Item" ;

  printf("%s -  item(%f, %f)  ->  world(%f, %f)\n", text, x1_in, y1_in, x1, y1) ;


  return ;
}


/* moves a feature to new coordinates */
void zMapWindowMoveItem(ZMapWindow window, ZMapFeature origFeature,
			ZMapFeature modFeature, FooCanvasItem *item)
{
  double top, bottom, offset;

  if (FOO_IS_CANVAS_ITEM (item))
    {
      offset = ((ZMapFeatureBlock)(modFeature->parent->parent))->block_to_sequence.q1;
      top = modFeature->x1;
      bottom = modFeature->x2;
      zmapWindowSeq2CanOffset(&top, &bottom, offset);

      if (modFeature->type == ZMAPSTYLE_MODE_TRANSCRIPT)
	{
	  zMapAssert(origFeature);

	  foo_canvas_item_set(item->parent, "y", top, NULL);
	}
      else
	{
	  foo_canvas_item_set(item, "y1", top, "y2", bottom, NULL);
	}

      zmapWindowUpdateInfoPanel(window, modFeature, item, NULL, 0, 0, NULL, TRUE, TRUE) ;
    }
  return;
}


/* Returns the sequence coords that correspond to the given _world_ foocanvas coords.
 *
 * NOTE that although we only return y coords, we need the world x coords as input
 * in order to find the right foocanvas item from which to get the sequence coords. */
gboolean zmapWindowWorld2SeqCoords(ZMapWindow window,
				   double wx1, double wy1, double wx2, double wy2,
				   FooCanvasGroup **block_grp_out, int *y1_out, int *y2_out)
{
  gboolean result = FALSE ;
  FooCanvasItem *item ;

  /* Try to get the item at wx1, wy1... we have to start somewhere... */
  if ((item = foo_canvas_get_item_at(window->canvas, wx1, wy1)))
    {
      FooCanvasGroup *block_container ;
      ZMapFeatureBlock block ;

      /* Getting the block struct as well is a bit belt and braces...we could return it but
       * its redundant info. really. */
      if ((block_container = FOO_CANVAS_GROUP(zmapWindowContainerUtilsItemGetParentLevel(item, ZMAPCONTAINER_LEVEL_BLOCK)))
	  && (block = zmapWindowItemGetFeatureBlock(block_container)))
	{
	  double offset ;

	  offset = (double)(block->block_to_sequence.q1 - 1) ; /* - 1 for 1 based coord system. */

	  my_foo_canvas_world_bounds_to_item(FOO_CANVAS_ITEM(block_container), &wx1, &wy1, &wx2, &wy2) ;


	  if (block_grp_out)
	    *block_grp_out = block_container ;
	  if (y1_out)
	    *y1_out = floor(wy1 - offset + 0.5) ;
	  if (y2_out)
	    *y2_out = floor(wy2 - offset + 0.5) ;

	  result = TRUE ;
	}
      else
	zMapLogWarning("%s", "No Block Container");
    }
  else
    {
      get_item_at_workaround_struct workaround_struct = {NULL};
      double scroll_x2;

      workaround_struct.wx1 = wx1;
      workaround_struct.wx2 = wx2;
      workaround_struct.wy1 = wy1;
      workaround_struct.wy2 = wy2;

      /* For some reason foo_canvas_get_item_at() fails to find items
       * a lot of the time even when it shouldn't and so we need a solution. */

      /* Incidentally some of the time, for some users, so does this. (RT # 55131) */
      /* The cause: if the mark is > 10% out of the scroll region the threshold (90%)
       * for intersection will not be met.  So I'm going to clamp to the scroll
       * region and lower the threshold to 55%...
       */
      zmapWindowGetScrollRegion(window, NULL, NULL, &scroll_x2, NULL);

      if(workaround_struct.wx2 > scroll_x2)
	workaround_struct.wx2 = scroll_x2;

      if(workaround_struct.wx2 == 0)
	workaround_struct.wx2 = workaround_struct.wx1 + 1;

      /* Here's another fix for this workaround code.  It fixes the
       * problem seen in RT ticket #75034.  The long items code is
       * causing the issue, as the block background, used to get the
       * block size, is resized by the long items code.  The
       * workaround uses the item size of the background in the
       * calculation of intersection and as this only slightly
       * intersects with the mark when zoomed in the intersection test
       * fails. Passing the window in allows for fetching of the long
       * item's (block container background) original size. */
      workaround_struct.window = window;

      zmapWindowContainerUtilsExecute(window->feature_root_group, ZMAPCONTAINER_LEVEL_BLOCK,
				      fill_workaround_struct,     &workaround_struct);

      if((result = workaround_struct.result))
	{
	  if (block_grp_out)
	    *block_grp_out = workaround_struct.block;
	  if (y1_out)
	    *y1_out = workaround_struct.seq_x;
	  if (y2_out)
	    *y2_out = workaround_struct.seq_y;
	}
    }


  return result ;
}


gboolean zmapWindowItem2SeqCoords(FooCanvasItem *item, int *y1, int *y2)
{
  gboolean result = FALSE ;



  return result ;
}


#if NOT_USED
static gboolean simple_highlight_region(FooCanvasPoints **points_out,
                                        FooCanvasItem    *subject,
                                        gpointer          user_data)
{
  StartEndTextHighlight high_data = (StartEndTextHighlight)user_data;
  FooCanvasPoints *points;
  double first[ITEMTEXT_CHAR_BOUND_COUNT], last[ITEMTEXT_CHAR_BOUND_COUNT];
  int index1, index2;
  gboolean first_found, last_found;
  gboolean redraw = FALSE;

  points = foo_canvas_points_new(8);

  index1 = high_data->start;
  index2 = high_data->end;

  /* SWAP MACRO? */
  if(index1 > index2)
    ZMAP_SWAP_TYPE(int, index1, index2);

  /* From the text indices, get the bounds of that char */
  first_found = zmapWindowItemTextIndex2Item(high_data->item, index1, &first[0]);
  last_found  = zmapWindowItemTextIndex2Item(high_data->item, index2, &last[0]);

  if(first_found && last_found)
    {
      double minx, maxx;
      foo_canvas_item_get_bounds(high_data->item, &minx, NULL, &maxx, NULL);
      zmapWindowItemTextOverlayFromCellBounds(points,
					      &first[0],
					      &last[0],
					      minx, maxx);
      zmapWindowItemTextOverlayText2Overlay(high_data->item, points);

      /* set the points */
      if(points_out)
	{
	  *points_out = points;
	  /* record such */
	  redraw = TRUE;
	}
    }

  return redraw;
}
#endif



/**
 * my_foo_canvas_item_get_world_bounds:
 * @item: A canvas item.
 * Last edited: Dec  6 13:10 2006 (edgrif)
 * @rootx1_out: X left coord of item (output value).
 * @rooty1_out: Y top coord of item (output value).
 * @rootx2_out: X right coord of item (output value).
 * @rootx2_out: Y bottom coord of item (output value).
 *
 * Returns the _world_ bounds of an item, no function exists in FooCanvas to do this.
 **/
void my_foo_canvas_item_get_world_bounds(FooCanvasItem *item,
					 double *rootx1_out, double *rooty1_out,
					 double *rootx2_out, double *rooty2_out)
{
  double rootx1, rootx2, rooty1, rooty2 ;

  g_return_if_fail (FOO_IS_CANVAS_ITEM (item)) ;
  g_return_if_fail (rootx1_out != NULL || rooty1_out != NULL || rootx2_out != NULL || rooty2_out != NULL ) ;

  foo_canvas_item_get_bounds(item, &rootx1, &rooty1, &rootx2, &rooty2) ;
  foo_canvas_item_i2w(item, &rootx1, &rooty1) ;
  foo_canvas_item_i2w(item, &rootx2, &rooty2) ;

  if (rootx1_out)
    *rootx1_out = rootx1 ;
  if (rooty1_out)
    *rooty1_out = rooty1 ;
  if (rootx2_out)
    *rootx2_out = rootx2 ;
  if (rooty2_out)
    *rooty2_out = rooty2 ;

  return ;
}




/**
 * my_foo_canvas_world_bounds_to_item:
 * @item: A canvas item.
 * @rootx1_out: X left coord of item (input/output value).
 * @rooty1_out: Y top coord of item (input/output value).
 * @rootx2_out: X right coord of item (input/output value).
 * @rootx2_out: Y bottom coord of item (input/output value).
 *
 * Returns the given _world_ bounds in the given item coord system, no function exists in FooCanvas to do this.
 **/
void my_foo_canvas_world_bounds_to_item(FooCanvasItem *item,
					double *rootx1_inout, double *rooty1_inout,
					double *rootx2_inout, double *rooty2_inout)
{
  double rootx1, rootx2, rooty1, rooty2 ;

  g_return_if_fail (FOO_IS_CANVAS_ITEM (item)) ;
  g_return_if_fail (rootx1_inout != NULL || rooty1_inout != NULL || rootx2_inout != NULL || rooty2_inout != NULL ) ;

  rootx1 = rootx2 = rooty1 = rooty2 = 0.0 ;

  if (rootx1_inout)
    rootx1 = *rootx1_inout ;
  if (rooty1_inout)
    rooty1 = *rooty1_inout ;
  if (rootx2_inout)
    rootx2 = *rootx2_inout ;
  if (rooty2_inout)
    rooty2 = *rooty2_inout ;

  foo_canvas_item_w2i(item, &rootx1, &rooty1) ;
  foo_canvas_item_w2i(item, &rootx2, &rooty2) ;

  if (rootx1_inout)
    *rootx1_inout = rootx1 ;
  if (rooty1_inout)
    *rooty1_inout = rooty1 ;
  if (rootx2_inout)
    *rootx2_inout = rootx2 ;
  if (rooty2_inout)
    *rooty2_inout = rooty2 ;

  return ;
}


/* This function returns the original bounds of an item ignoring any long item clipping that may
 * have been done. */
void my_foo_canvas_item_get_long_bounds(ZMapWindowLongItems long_items, FooCanvasItem *item,
					double *x1_out, double *y1_out,
					double *x2_out, double *y2_out)
{
  zMapAssert(long_items && item) ;

  if (zmapWindowItemIsShown(item))
    {
      double x1, y1, x2, y2 ;
      double long_start, long_end ;

      foo_canvas_item_get_bounds(item, &x1, &y1, &x2, &y2) ;

      if (zmapWindowLongItemCoords(long_items, item, &long_start, &long_end))
	{
	  if (long_start < y1)
	    y1 = long_start ;

	  if (long_end > y2)
	    y2 = long_end ;
	}

      *x1_out = x1 ;
      *y1_out = y1 ;
      *x2_out = x2 ;
      *y2_out = y2 ;
    }

  return ;
}





/* I'M TRYING THESE TWO FUNCTIONS BECAUSE I DON'T LIKE THE BIT WHERE IT GOES TO THE ITEMS
 * PARENT IMMEDIATELY....WHAT IF THE ITEM IS ALREADY A GROUP ?????? THE ORIGINAL FUNCTION
 * CANNOT BE USED TO CONVERT A GROUPS POSITION CORRECTLY......S*/

/**
 * my_foo_canvas_item_w2i:
 * @item: A canvas item.
 * @x: X coordinate to convert (input/output value).
 * @y: Y coordinate to convert (input/output value).
 *
 * Converts a coordinate pair from world coordinates to item-relative
 * coordinates.
 **/
void my_foo_canvas_item_w2i (FooCanvasItem *item, double *x, double *y)
{
  g_return_if_fail (FOO_IS_CANVAS_ITEM (item));
  g_return_if_fail (x != NULL);
  g_return_if_fail (y != NULL);


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* orginal code... */
  item = item->parent;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
  if (!FOO_IS_CANVAS_GROUP (item))
    item = item->parent;

  while (item) {
    if (FOO_IS_CANVAS_GROUP (item)) {
      *x -= FOO_CANVAS_GROUP (item)->xpos;
      *y -= FOO_CANVAS_GROUP (item)->ypos;
    }

    item = item->parent;
  }

  return ;
}


/**
 * my_foo_canvas_item_i2w:
 * @item: A canvas item.
 * @x: X coordinate to convert (input/output value).
 * @y: Y coordinate to convert (input/output value).
 *
 * Converts a coordinate pair from item-relative coordinates to world
 * coordinates.
 **/
void my_foo_canvas_item_i2w (FooCanvasItem *item, double *x, double *y)
{
  g_return_if_fail (FOO_IS_CANVAS_ITEM (item));
  g_return_if_fail (x != NULL);
  g_return_if_fail (y != NULL);


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* Original code... */
  item = item->parent;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
  if (!FOO_IS_CANVAS_GROUP (item))
    item = item->parent;

  while (item) {
    if (FOO_IS_CANVAS_GROUP (item)) {
      *x += FOO_CANVAS_GROUP (item)->xpos;
      *y += FOO_CANVAS_GROUP (item)->ypos;
    }

    item = item->parent;
  }

  return ;
}

/* A major problem with foo_canvas_item_move() is that it moves an item by an
 * amount rather than moving it somewhere in the group which is painful for operations
 * like unbumping a column. */
void my_foo_canvas_item_goto(FooCanvasItem *item, double *x, double *y)
{
  double x1, y1, x2, y2 ;
  double dx, dy ;

  if (x || y)
    {
      x1 = y1 = x2 = y2 = 0.0 ;
      dx = dy = 0.0 ;

      foo_canvas_item_get_bounds(item, &x1, &y1, &x2, &y2) ;

      if (x)
	dx = *x - x1 ;
      if (y)
	dy = *y - y1 ;

      foo_canvas_item_move(item, dx, dy) ;
    }

  return ;
}


void zmapWindowItemGetVisibleCanvas(ZMapWindow window,
                                    double *wx1, double *wy1,
                                    double *wx2, double *wy2)
{

  getVisibleCanvas(window, wx1, wy1, wx2, wy2);

  return ;
}




/*
 *                  Internal routines.
 */


static FooCanvasItem *translation_from_block_frame(ZMapWindow window, char *column_name,
							gboolean require_visible,
							ZMapFeatureBlock block, ZMapFrame frame)
{
  FooCanvasItem *translation = NULL;
  GQuark feature_set_id, feature_id;
  ZMapFeatureSet feature_set;
  ZMapStrand strand = ZMAPSTRAND_FORWARD;

  feature_set_id = zMapStyleCreateID(column_name);
  /* and look up the translation feature set with ^^^ */

  if((feature_set = zMapFeatureBlockGetSetByID(block, feature_set_id)))
    {
      char *feature_name;

      /* Get the name of the framed feature... */
      feature_name = zMapFeature3FrameTranslationFeatureName(feature_set, frame);
      /* ... and its quark id */
      feature_id   = g_quark_from_string(feature_name);

      if((translation  = zmapWindowFToIFindItemFull(window->context_to_item,
						    block->parent->unique_id,
						    block->unique_id,
						    feature_set_id,
						    strand, /* STILL ALWAYS FORWARD */
						    frame,
						    feature_id)))
	{
	  if(require_visible && !(FOO_CANVAS_ITEM(translation)->object.flags & FOO_CANVAS_ITEM_VISIBLE))
	    translation = NULL;

	}

      g_free(feature_name);
    }

  return translation;
}





static FooCanvasItem *translation_item_from_block_frame(ZMapWindow window, char *column_name,
							gboolean require_visible,
							ZMapFeatureBlock block, ZMapFrame frame)
{
  FooCanvasItem *translation = NULL;
  GQuark feature_set_id, feature_id;
  ZMapFeatureSet feature_set;
  ZMapStrand strand = ZMAPSTRAND_FORWARD;

  feature_set_id = zMapStyleCreateID(column_name);
  /* and look up the translation feature set with ^^^ */

  if((feature_set = zMapFeatureBlockGetSetByID(block, feature_set_id)))
    {
      char *feature_name;

      /* Get the name of the framed feature... */
      feature_name = zMapFeature3FrameTranslationFeatureName(feature_set, frame);
      /* ... and its quark id */
      feature_id   = g_quark_from_string(feature_name);

      if((translation  = zmapWindowFToIFindItemFull(window->context_to_item,
						    block->parent->unique_id,
						    block->unique_id,
						    feature_set_id,
						    strand, /* STILL ALWAYS FORWARD */
						    frame,
						    feature_id)))
	{
	  if(require_visible && !(FOO_CANVAS_ITEM(translation)->object.flags & FOO_CANVAS_ITEM_VISIBLE))
	    translation = NULL;
	  else
	    translation = FOO_CANVAS_GROUP(translation)->item_list->data;
	}

      g_free(feature_name);
    }

  return translation;
}

FooCanvasItem *zmapWindowItemGetTranslationItemFromItem(ZMapWindow window, FooCanvasItem *item)
{
  ZMapFeatureBlock block;
  ZMapFeature feature;
  FooCanvasItem *translation = NULL;

  if((feature = zmapWindowItemGetFeature(item)))
    {
      /* First go up to block... */
      block = (ZMapFeatureBlock)
        (zMapFeatureGetParentGroup((ZMapFeatureAny)(feature),
                                   ZMAPFEATURE_STRUCT_BLOCK));
      zMapAssert(block);

      /* Get the frame for the item... and its translation feature (ITEM_FEATURE_PARENT!) */
      translation = translation_item_from_block_frame(window, ZMAP_FIXED_STYLE_3FT_NAME, TRUE,
						      block, zmapWindowItemFeatureFrame(item));
    }
  else
    {
      zMapAssertNotReached();
    }

  return translation;
}


static ZMapFeatureContextExecuteStatus highlight_feature(GQuark key, gpointer data, gpointer user_data, char **error_out)
{
  ZMapFeatureContextExecuteStatus status = ZMAP_CONTEXT_EXEC_STATUS_OK;
  ZMapFeatureAny feature_any = (ZMapFeatureAny)data;
  HighlightContext highlight_data = (HighlightContext)user_data;
  ZMapFeature feature_in, feature_current;
  FooCanvasItem *feature_item;
  gboolean replace_highlight = TRUE;

  if(feature_any->struct_type == ZMAPFEATURE_STRUCT_FEATURE)
    {
      feature_in = (ZMapFeature)feature_any;
      if(highlight_data->multiple_select || highlight_data->highlighted == 0)
        {
          replace_highlight = !(highlight_data->multiple_select);

          if((feature_item = zmapWindowFToIFindFeatureItem(highlight_data->window->context_to_item,
                                                           feature_in->strand, ZMAPFRAME_NONE,
                                                           feature_in)))
            {
              if(highlight_data->multiple_select)
                replace_highlight = !(zmapWindowFocusIsItemInHotColumn(highlight_data->window->focus, feature_item));

              feature_current = zmapWindowItemGetFeature(feature_item);
              zMapAssert(feature_current);

              if(feature_in->type == ZMAPSTYLE_MODE_TRANSCRIPT &&
                 feature_in->feature.transcript.exons->len > 0 &&
                 feature_in->feature.transcript.exons->len != feature_current->feature.transcript.exons->len)
                {
                  ZMapSpan span;
                  int i;

                  /* need to do something to find sub feature items??? */
                  for(i = 0; i < feature_in->feature.transcript.exons->len; i++)
                    {
                      if(replace_highlight && i > 0)
                        replace_highlight = FALSE;

                      span = &g_array_index(feature_in->feature.transcript.exons, ZMapSpanStruct, i) ;

                      if((feature_item = zmapWindowFToIFindItemChild(highlight_data->window->context_to_item,
                                                                     feature_in->strand, ZMAPFRAME_NONE,
                                                                     feature_in, span->x1, span->x2)))
                        zmapWindowHighlightObject(highlight_data->window, feature_item,
						  replace_highlight, TRUE) ;
                    }
                  for(i = 0; i < feature_in->feature.transcript.introns->len; i++)
                    {
                      span = &g_array_index(feature_in->feature.transcript.introns, ZMapSpanStruct, i) ;

                      if((feature_item = zmapWindowFToIFindItemChild(highlight_data->window->context_to_item,
                                                                     feature_in->strand, ZMAPFRAME_NONE,
                                                                     feature_in, span->x1, span->x2)))
                        zmapWindowHighlightObject(highlight_data->window, feature_item,
						  replace_highlight, TRUE);
                    }

                  replace_highlight = !(highlight_data->multiple_select);
                }
              else
                /* we need to highlight the full feature */
                zmapWindowHighlightObject(highlight_data->window, feature_item, replace_highlight, TRUE);

              if(replace_highlight)
                highlight_data->highlighted = 0;
              else
                highlight_data->highlighted++;
            }
        }
      highlight_data->feature_count++;
    }

  return status;
}




/* GCompareFunc() to compare two features by their coords so they are sorted into ascending order. */
static gint sortByPositionCB(gconstpointer a, gconstpointer b)
{
  gint result ;
  FooCanvasItem *item_a = (FooCanvasItem *)a ;
  FooCanvasItem *item_b = (FooCanvasItem *)b ;
  ZMapFeature feat_a ;
  ZMapFeature feat_b ;

  feat_a = zmapWindowItemGetFeature(item_a);
  zMapAssert(feat_a) ;

  feat_b = zmapWindowItemGetFeature(item_b);
  zMapAssert(feat_b) ;

  if (feat_a->x1 < feat_b->x1)
    result = -1 ;
  else if (feat_a->x1 > feat_b->x1)
    result = 1 ;
  else
    result = 0 ;

  return result ;
}



static void extract_feature_from_item(gpointer list_data, gpointer user_data)
{
  GList **list = (GList **)user_data;
  FooCanvasItem *item = (FooCanvasItem *)list_data;
  ZMapFeature feature;

  if((feature = zmapWindowItemGetFeature(item)))
    {
      *list = g_list_append(*list, feature);
    }
  else
    zMapAssertNotReached();

  return ;
}


/* Get the visible portion of the canvas. */
static void getVisibleCanvas(ZMapWindow window,
			     double *screenx1_out, double *screeny1_out,
			     double *screenx2_out, double *screeny2_out)
{
  GtkAdjustment *v_adjust, *h_adjust ;
  double x1, x2, y1, y2 ;

  /* Work out which part of the canvas is visible currently. */
  v_adjust = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(window->scrolled_window)) ;
  h_adjust = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(window->scrolled_window)) ;

  x1 = h_adjust->value ;
  x2 = x1 + h_adjust->page_size ;

  y1 = v_adjust->value ;
  y2 = y1 + v_adjust->page_size ;

  foo_canvas_window_to_world(window->canvas, x1, y1, screenx1_out, screeny1_out) ;
  foo_canvas_window_to_world(window->canvas, x2, y2, screenx2_out, screeny2_out) ;

  return ;
}

/* workaround for a failing foo_canvas_item_at(). Actually only looks for blocks! */
static void fill_workaround_struct(ZMapWindowContainerGroup container,
				   FooCanvasPoints       *points,
				   ZMapContainerLevelType level,
				   gpointer               user_data)
{
  get_item_at_workaround workaround = (get_item_at_workaround)user_data;
extern char *group_foo_info(ZMapWindowContainerGroup container);

if(!container) return;
//zMapLogWarning("level %d container: %s",level,group_foo_info(container));

  switch(level)
    {
    case ZMAPCONTAINER_LEVEL_BLOCK:
      {
	FooCanvasItem *cont_backgrd;
	ZMapFeatureBlock block;

	if((cont_backgrd = (FooCanvasItem *)zmapWindowContainerGetBackground(container)))
	  {
	    double offset;
	    AreaStruct area_src = {workaround->wx1, workaround->wy1, workaround->wx2, workaround->wy2},
	      area_block = {};
	    foo_canvas_item_get_bounds(cont_backgrd,
				       &(area_block.x1), &(area_block.y1),
				       &(area_block.x2), &(area_block.y2));

	    /* The original size of the block needs to be used, not the longitem resized size. */
	    if(workaround->window && workaround->window->long_items)
	      {
		double long_y1, long_y2;
		/* Get the original size of the block's background, see caller & RT #75034 */
		if(zmapWindowLongItemCoords(workaround->window->long_items, cont_backgrd,
					    &long_y1, &long_y2))
		  {
		    area_block.y1 = long_y1;
		    area_block.y2 = long_y2;
		  }
	      }

	    if((workaround->wx1 >= area_block.x1 && workaround->wx2 <= area_block.x2 &&
		workaround->wy1 >= area_block.y1 && workaround->wy2 <= area_block.y2) ||
	       areas_intersect_gt_threshold(&area_src, &area_block, 0.55))
	      {
		/* We're inside */
		workaround->block = (FooCanvasGroup *)container;
		block = zmapWindowItemGetFeatureBlock(container);

		offset = (double)(block->block_to_sequence.q1 - 1) ; /* - 1 for 1 based coord system. */

		my_foo_canvas_world_bounds_to_item(FOO_CANVAS_ITEM(cont_backgrd),
						   &(workaround->wx1), &(workaround->wy1),
						   &(workaround->wx2), &(workaround->wy2)) ;

		workaround->seq_x  = floor(workaround->wy1 - offset + 0.5) ;
		workaround->seq_y  = floor(workaround->wy2 - offset + 0.5) ;
		workaround->result = TRUE;
	      }
	    else
	      {
                 zMapLogWarning("fill_workaround_struct: Area block (%f, %f), (%f, %f) "
			     "workaround (%f, %f), (%f, %f) Roy needs to look at this.",
			     area_block.x1, area_block.y1,
			     area_block.x2, area_block.y2,
			     workaround->wx1, workaround->wy1,
			     workaround->wx2, workaround->wy2);
            }
	  }

      }
      break;
    default:
      break;
    }

  return ;
}

static gboolean areas_intersection(AreaStruct *area_1, AreaStruct *area_2, AreaStruct *intersect)
{
  double x1, x2, y1, y2;
  gboolean overlap = FALSE;

  x1 = MAX(area_1->x1, area_2->x1);
  y1 = MAX(area_1->y1, area_2->y1);
  x2 = MIN(area_1->x2, area_2->x2);
  y2 = MIN(area_1->y2, area_2->y2);

  if(y2 - y1 > 0 && x2 - x1 > 0)
    {
      intersect->x1 = x1;
      intersect->y1 = y1;
      intersect->x2 = x2;
      intersect->y2 = y2;
      overlap = TRUE;
    }
  else
    intersect->x1 = intersect->y1 =
      intersect->x2 = intersect->y2 = 0.0;

  return overlap;
}

/* threshold = percentage / 100. i.e. has a range of 0.00000001 -> 1.00000000 */
/* For 100% overlap pass 1.0, For 50% overlap pass 0.5 */
static gboolean areas_intersect_gt_threshold(AreaStruct *area_1, AreaStruct *area_2, double threshold)
{
  AreaStruct inter = {};
  double a1, aI;
  gboolean above_threshold = FALSE;

  if(areas_intersection(area_1, area_2, &inter))
    {
      aI = (inter.x2 - inter.x1 + 1.0) * (inter.y2 - inter.y1 + 1.0);
      a1 = (area_1->x2 - area_1->x1 + 1.0) * (area_1->y2 - area_1->y1 + 1.0);

      if(threshold > 0.0 && threshold < 1.0)
	threshold = 1.0 - threshold;
      else
	threshold = 0.0;		/* 100% overlap only */

      if(inter.x1 >= area_1->x1 &&
	 inter.y1 >= area_1->y1 &&
	 inter.x2 <= area_1->x2 &&
	 inter.y2 <= area_1->y2)
	{
	  above_threshold = TRUE; /* completely contained */
	}
      else if((aI <= (a1 * (1.0 + threshold))) && (aI >= (a1 * (1.0 - threshold))))
	above_threshold = TRUE;
      else
	zMapLogWarning("%s", "intersection below threshold");
    }
  else
    zMapLogWarning("%s", "no intersection");

  return above_threshold;
}










#ifdef INTERSECTION_CODE

/* I DON'T KNOW WHAT THIS WAS FOR, WHY IT WAS WRITTEN OR ANYTHING...... */



/* Untested... */
static gboolean foo_canvas_items_get_intersect(FooCanvasItem *i1, FooCanvasItem *i2, FooCanvasPoints **points_out)
{
  gboolean intersect = FALSE;

  if(points_out)
    {
      AreaStruct a1 = {};
      AreaStruct a2 = {};
      AreaStruct i  = {};

      foo_canvas_item_get_bounds(i1,
				 &(a1.x1), (&a1.y1),
				 &(a1.x2), (&a1.y2));
      foo_canvas_item_get_bounds(i2,
				 &(a2.x1), (&a2.y1),
				 &(a2.x2), (&a2.y2));

      if((intersect = areas_intersection(&a1, &a2, &i)))
	{
	  FooCanvasPoints *points = foo_canvas_points_new(2);
	  points->coords[0] = i.x1;
	  points->coords[1] = i.y1;
	  points->coords[2] = i.x2;
	  points->coords[3] = i.y2;
	  *points_out = points;
	}
    }

  return intersect;
}

/* Untested... */
static gboolean foo_canvas_items_intersect(FooCanvasItem *i1, FooCanvasItem *i2, double threshold)
{
  AreaStruct a1 = {};
  AreaStruct a2 = {};
  gboolean intersect = FALSE;

  foo_canvas_item_get_bounds(i1,
			     &(a1.x1), (&a1.y1),
			     &(a1.x2), (&a1.y2));
  foo_canvas_item_get_bounds(i2,
			     &(a2.x1), (&a2.y1),
			     &(a2.x2), (&a2.y2));

  intersect = areas_intersect_gt_threshold(&a1, &a2, threshold);

  return intersect;
}
#endif /* INTERSECTION_CODE */

