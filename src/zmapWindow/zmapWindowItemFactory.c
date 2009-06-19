 /*  File: zmapWindowItemFactory.c
 *  Author: Roy Storey (rds@sanger.ac.uk)
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
 * originally written by:
 *
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *
 * Description: Factory-object based set of functions for drawing
 *              features.
 *
 * Exported functions: See zmapWindowItemFactory.h
 * HISTORY:
 * Last edited: Jun 19 12:13 2009 (rds)
 * Created: Mon Sep 25 09:09:52 2006 (rds)
 * CVS info:   $Id: zmapWindowItemFactory.c,v 1.62 2009-06-19 11:14:12 rds Exp $
 *-------------------------------------------------------------------
 */

#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <ZMap/zmapUtils.h>
#include <ZMap/zmapStyle.h>
#include <zmapWindow_P.h>
#include <zmapWindowContainerUtils.h>
#include <zmapWindowItemFactory.h>
#include <zmapWindowItemTextFillColumn.h>
#include <zmapWindowCanvasItem.h>

typedef struct _RunSetStruct *RunSet;

typedef FooCanvasItem * (*ZMapWindowFToIFactoryInternalMethod)(RunSet run_data, ZMapFeature feature,
                                                               double feature_offset,
                                                               double x1, double y1, double x2, double y2,
                                                               ZMapFeatureTypeStyle style);

typedef struct
{
  ZMapStyleMode type;

  ZMapWindowFToIFactoryInternalMethod method;
}ZMapWindowFToIFactoryMethodsStruct, *ZMapWindowFToIFactoryMethods;


typedef struct _ZMapWindowFToIFactoryStruct
{
  guint line_width;            /* window->config.feature_line_width */
  ZMapWindowFToIFactoryMethodsStruct *methods;
  ZMapWindowFToIFactoryMethodsStruct *basic_methods;
  ZMapWindowFToIFactoryMethodsStruct *text_methods;
  ZMapWindowFToIFactoryMethodsStruct *graph_methods;
  GHashTable                         *ftoi_hash;
  ZMapWindowLongItems                 long_items;
  ZMapWindowFToIFactoryProductionTeam user_funcs;
  gpointer                            user_data;
  double                              limits[4];
  double                              points[4];
  PangoFontDescription               *font_desc;
  double                              text_width;
  double                              text_height;
} ZMapWindowFToIFactoryStruct ;

typedef struct _RunSetStruct
{
  ZMapWindowFToIFactory factory;
  ZMapFeatureContext    context;
  ZMapFeatureAlignment  align;
  ZMapFeatureBlock      block;
  ZMapFeatureSet        set;
  ZMapFrame             frame;
  FooCanvasGroup       *container;
  FooCanvasItem        *canvas_item;
}RunSetStruct;


static void copyCheckMethodTable(const ZMapWindowFToIFactoryMethodsStruct  *table_in, 
                                 ZMapWindowFToIFactoryMethodsStruct       **table_out) ;

static void datalistRun(gpointer key, gpointer list_data, gpointer user_data);
inline 
static void callItemHandler(ZMapWindowFToIFactory                   factory,
                            FooCanvasItem            *new_item,
                            ZMapWindowItemFeatureType new_item_type,
                            ZMapFeature               full_feature,
                            ZMapWindowItemFeature     sub_feature,
                            double                    new_item_y1,
                            double                    new_item_y2);

/* ZMapWindowFToIFactoryInternalMethod prototypes */
static FooCanvasItem *invalidFeature(RunSet run_data, ZMapFeature feature,
                                     double feature_offset, 
                                     double x1, double y1, double x2, double y2,
                                     ZMapFeatureTypeStyle style);
static FooCanvasItem *drawSimpleFeature(RunSet run_data, ZMapFeature feature,
                                        double feature_offset, 
					double x1, double y1, double x2, double y2,
                                        ZMapFeatureTypeStyle style);
static FooCanvasItem *drawGlyphFeature(RunSet run_data, ZMapFeature feature,
				       double feature_offset, 
				       double x1, double y1, double x2, double y2,
				       ZMapFeatureTypeStyle style);
static FooCanvasItem *drawAlignFeature(RunSet run_data, ZMapFeature feature,
                                       double feature_offset, 
                                       double x1, double y1, double x2, double y2,
                                       ZMapFeatureTypeStyle style);
static FooCanvasItem *drawTranscriptFeature(RunSet run_data, ZMapFeature feature,
                                            double feature_offset, 
                                            double x1, double y1, double x2, double y2,
                                            ZMapFeatureTypeStyle style);
static FooCanvasItem *drawSeqFeature(RunSet run_data, ZMapFeature feature,
                                     double feature_offset, 
                                     double x1, double y1, double x2, double y2,
                                     ZMapFeatureTypeStyle style);
static FooCanvasItem *drawPepFeature(RunSet run_data, ZMapFeature feature,
                                     double feature_offset, 
                                     double x1, double y1, double x2, double y2,
                                     ZMapFeatureTypeStyle style);
static FooCanvasItem *drawSimpleAsTextFeature(RunSet run_data, ZMapFeature feature,
                                              double feature_offset, 
                                              double x1, double y1, double x2, double y2,
                                              ZMapFeatureTypeStyle style);
static FooCanvasItem *drawSimpleGraphFeature(RunSet run_data, ZMapFeature feature,
					     double feature_offset, 
					     double x1, double y1, double x2, double y2,
					     ZMapFeatureTypeStyle style);
static FooCanvasItem *drawFullColumnTextFeature(RunSet run_data,  ZMapFeature feature,
                                                double feature_offset,
                                                double x1, double y1, double x2, double y2,
                                                ZMapFeatureTypeStyle style);

static void GapAlignBlockFromAdjacentBlocks(ZMapAlignBlock block_a, ZMapAlignBlock block_b, 
					    ZMapAlignBlockStruct *gap_span_out,
					    gboolean *t_indel_gt_1,
					    gboolean *q_indel_gt_1);

static void GapAlignBlockFromAdjacentBlocks2(ZMapAlignBlock block_a, ZMapAlignBlock block_b, 
					     ZMapAlignBlockStruct *gap_span_out,
					     gboolean *t_indel_gt_1,
					     gboolean *q_indel_gt_1);

static void null_item_created(FooCanvasItem            *new_item,
                              ZMapWindowItemFeatureType new_item_type,
                              ZMapFeature               full_feature,
                              ZMapWindowItemFeature     sub_feature,
                              double                    new_item_y1,
                              double                    new_item_y2,
                              gpointer                  handler_data);
static gboolean null_top_item_created(FooCanvasItem *top_item,
                                      ZMapFeatureContext context,
                                      ZMapFeatureAlignment align,
                                      ZMapFeatureBlock block,
                                      ZMapFeatureSet set,
                                      ZMapFeature feature,
                                      gpointer handler_data);
static gboolean null_feature_size_request(ZMapFeature feature, 
                                          double *limits_array, 
                                          double *points_array_inout, 
                                          gpointer handler_data);

static gboolean clip_feature_coords(ZMapWindowFToIFactory factory,
				    ZMapFeature           feature,
				    double                offset,
				    double               *start_inout,
				    double               *end_inout);


/*
 * Static function tables for drawing features in various ways.
 */

const static ZMapWindowFToIFactoryMethodsStruct factory_methods_G[] = {
  {ZMAPSTYLE_MODE_INVALID,      invalidFeature},
  {ZMAPSTYLE_MODE_BASIC,        drawSimpleFeature},
  {ZMAPSTYLE_MODE_ALIGNMENT,    drawAlignFeature},
  {ZMAPSTYLE_MODE_TRANSCRIPT,   drawTranscriptFeature},
  {ZMAPSTYLE_MODE_RAW_SEQUENCE, drawSeqFeature},
  {ZMAPSTYLE_MODE_PEP_SEQUENCE, drawPepFeature},
  {ZMAPSTYLE_MODE_ASSEMBLY_PATH, drawSimpleFeature},
  {ZMAPSTYLE_MODE_TEXT,         drawSimpleAsTextFeature},
  {ZMAPSTYLE_MODE_GRAPH,        drawSimpleGraphFeature},
  {ZMAPSTYLE_MODE_GLYPH,        drawGlyphFeature},
  {-1,                          NULL}
};

const static ZMapWindowFToIFactoryMethodsStruct factory_text_methods_G[] = {
  {ZMAPSTYLE_MODE_INVALID,      invalidFeature},
  {ZMAPSTYLE_MODE_BASIC,        drawSimpleAsTextFeature},
  {ZMAPSTYLE_MODE_ALIGNMENT,    drawAlignFeature},
  {ZMAPSTYLE_MODE_TRANSCRIPT,   drawTranscriptFeature},
  {ZMAPSTYLE_MODE_RAW_SEQUENCE, drawSeqFeature},
  {ZMAPSTYLE_MODE_PEP_SEQUENCE, drawPepFeature},
  {ZMAPSTYLE_MODE_ASSEMBLY_PATH, drawSimpleFeature},
  {ZMAPSTYLE_MODE_TEXT,         drawSimpleAsTextFeature},
  {ZMAPSTYLE_MODE_GRAPH,        drawSimpleGraphFeature},
  {ZMAPSTYLE_MODE_GLYPH,        drawGlyphFeature},
  {-1,                          NULL}
};

const static ZMapWindowFToIFactoryMethodsStruct factory_graph_methods_G[] = {
  {ZMAPSTYLE_MODE_INVALID,      invalidFeature},
  {ZMAPSTYLE_MODE_BASIC,        drawSimpleGraphFeature},
  {ZMAPSTYLE_MODE_ALIGNMENT,    drawSimpleGraphFeature},
  {ZMAPSTYLE_MODE_TRANSCRIPT,   drawSimpleGraphFeature},
  {ZMAPSTYLE_MODE_RAW_SEQUENCE, drawSimpleGraphFeature},
  {ZMAPSTYLE_MODE_PEP_SEQUENCE, drawSimpleGraphFeature},
  {ZMAPSTYLE_MODE_ASSEMBLY_PATH, drawSimpleFeature},
  {ZMAPSTYLE_MODE_TEXT,         drawSimpleAsTextFeature},
  {ZMAPSTYLE_MODE_GRAPH,        drawSimpleGraphFeature},
  {ZMAPSTYLE_MODE_GLYPH,        drawGlyphFeature},
  {-1,                          NULL}
};

const static ZMapWindowFToIFactoryMethodsStruct factory_basic_methods_G[] = {
  {ZMAPSTYLE_MODE_INVALID,      invalidFeature},
  {ZMAPSTYLE_MODE_BASIC,        drawSimpleFeature},
  {ZMAPSTYLE_MODE_ALIGNMENT,    drawSimpleFeature},
  {ZMAPSTYLE_MODE_TRANSCRIPT,   drawSimpleFeature},
  {ZMAPSTYLE_MODE_RAW_SEQUENCE, drawSimpleFeature},
  {ZMAPSTYLE_MODE_PEP_SEQUENCE, drawSimpleFeature},
  {ZMAPSTYLE_MODE_ASSEMBLY_PATH, drawSimpleFeature},
  {ZMAPSTYLE_MODE_TEXT,         drawSimpleAsTextFeature},
  {ZMAPSTYLE_MODE_GRAPH,        drawSimpleGraphFeature},
  {ZMAPSTYLE_MODE_GLYPH,        drawGlyphFeature},
  {-1,                          NULL}
};


ZMapWindowFToIFactory zmapWindowFToIFactoryOpen(GHashTable *feature_to_item_hash, 
                                                ZMapWindowLongItems long_items)
{
  ZMapWindowFToIFactory factory = NULL;
  ZMapWindowFToIFactoryProductionTeam user_funcs = NULL;

  if ((factory = g_new0(ZMapWindowFToIFactoryStruct, 1)) && 
     (user_funcs = g_new0(ZMapWindowFToIFactoryProductionTeamStruct, 1)))
    {
      factory->ftoi_hash  = feature_to_item_hash;
      factory->long_items = long_items;

      copyCheckMethodTable(&(factory_methods_G[0]), 
                           &(factory->methods));
      copyCheckMethodTable(&(factory_basic_methods_G[0]), 
                           &(factory->basic_methods));
      copyCheckMethodTable(&(factory_text_methods_G[0]), 
                           &(factory->text_methods));
      copyCheckMethodTable(&(factory_graph_methods_G[0]), 
                           &(factory->graph_methods));

      user_funcs->item_created = null_item_created;
      user_funcs->top_item_created = null_top_item_created;
      user_funcs->feature_size_request = null_feature_size_request;
      factory->user_funcs = user_funcs;
    }

  return factory;
}

void zmapWindowFToIFactorySetup(ZMapWindowFToIFactory factory, 
                                guint line_width, /* a config struct ? */
                                ZMapWindowFToIFactoryProductionTeam signal_handlers, 
                                gpointer handler_data)
{
  zMapAssert(signal_handlers);

  if(signal_handlers->item_created)
    factory->user_funcs->item_created = signal_handlers->item_created;

  if(signal_handlers->top_item_created)
    factory->user_funcs->top_item_created = signal_handlers->top_item_created;

  if(signal_handlers->feature_size_request)
    factory->user_funcs->feature_size_request = signal_handlers->feature_size_request;

  factory->user_data  = handler_data;
  factory->line_width = line_width;

  return ;
}

void zmapWindowFToIFactoryRunSet(ZMapWindowFToIFactory factory, 
                                 ZMapFeatureSet set, 
                                 FooCanvasGroup *container,
                                 ZMapFrame frame)
{
  RunSetStruct run_data = {NULL};

  run_data.factory = factory;
  /*
    run_data.container = canZMapWindowFToIFactoryDoThis_ShouldItBeAParameter();
  */
  run_data.container   = container;

  run_data.set     = set;
  run_data.context = (ZMapFeatureContext)zMapFeatureGetParentGroup((ZMapFeatureAny)set, ZMAPFEATURE_STRUCT_CONTEXT);
  run_data.align   = (ZMapFeatureAlignment)zMapFeatureGetParentGroup((ZMapFeatureAny)set, ZMAPFEATURE_STRUCT_ALIGN);
  run_data.block   = (ZMapFeatureBlock)zMapFeatureGetParentGroup((ZMapFeatureAny)set, ZMAPFEATURE_STRUCT_BLOCK);

  run_data.frame   = frame;

  g_hash_table_foreach(set->features, datalistRun, &run_data);

  return ;
}



FooCanvasItem *zmapWindowFToIFactoryRunSingle(ZMapWindowFToIFactory factory, 
					      FooCanvasItem        *current_item,
                                              FooCanvasGroup       *parent_container,
                                              ZMapFeatureContext    context, 
                                              ZMapFeatureAlignment  align, 
                                              ZMapFeatureBlock      block,
                                              ZMapFeatureSet        set,
                                              ZMapFeature           feature)
{
  RunSetStruct run_data = {NULL};
  FooCanvasItem *item = NULL, *return_item = NULL;
  FooCanvasGroup *features_container = NULL;
  ZMapFeatureTypeStyle style = NULL;
  ZMapStyleMode style_mode = ZMAPSTYLE_MODE_INVALID ;
  gboolean no_points_in_block = TRUE;
  /* check here before they get called.  I'd prefer to only do this
   * once per factory rather than once per Run! */


  zMapAssert(factory->user_funcs && 
             factory->user_funcs->item_created &&
             factory->user_funcs->top_item_created);
#ifdef RDS_REMOVED
  ZMapWindowItemFeatureSetData set_data = NULL;
  ZMapWindowStats parent_stats ;

  parent_stats = g_object_get_data(G_OBJECT(parent_container), ITEM_FEATURE_STATS) ;

  set_data = g_object_get_data(G_OBJECT(parent_container), ITEM_FEATURE_SET_DATA) ;
  zMapAssert(set_data) ;
#endif
  
  /* Get the styles table from the column and look for the features style.... */
  if (!(style = zmapWindowContainerFeatureSetStyleFromID((ZMapWindowContainerFeatureSet)parent_container, feature->style_id)))
    {
      zMapAssertNotReached();
    }

  style_mode = zMapStyleGetMode(style) ;

  if (style_mode >= 0 && style_mode <= FACTORY_METHOD_COUNT)
    {
      double *limits = &(factory->limits[0]);
      double *points = &(factory->points[0]);
      ZMapWindowFToIFactoryMethods method = NULL;
      ZMapWindowFToIFactoryMethodsStruct *method_table = NULL;
      ZMapWindowStats stats ;
      
      stats = g_object_get_data(G_OBJECT(parent_container), ITEM_FEATURE_STATS) ;
#ifdef RDS_REMOVED
      zMapAssert(stats) ;
      
      zmapWindowStatsAddChild(stats, (ZMapFeatureAny)feature) ;
#endif

      features_container = (FooCanvasGroup *)zmapWindowContainerGetFeatures((ZMapWindowContainerGroup)parent_container) ;
  
      if (zMapStyleIsStrandSpecific(style)
          && (feature->strand == ZMAPSTRAND_REVERSE && !zMapStyleIsShowReverseStrand(style)))
        goto undrawn;

      /* Note: for object to _span_ "width" units, we start at zero and end at "width". */
      limits[0] = 0.0;
      limits[1] = (double)(block->block_to_sequence.q1);
      limits[2] = zMapStyleGetWidth(style);
      limits[3] = (double)(block->block_to_sequence.q2);

      /* Set these to this for normal main window use. */
      points[0] = 0.0;
      points[1] = feature->x1;
      points[2] = zMapStyleGetWidth(style);
      points[3] = feature->x2;

      /* inline static void normalSizeReq(ZMapFeature f, ... ){ return ; } */
      /* inlined this should be nothing most of the time, question is can it be inlined?? */
      if((no_points_in_block = (factory->user_funcs->feature_size_request)(feature, &limits[0], &points[0], factory->user_data)) == TRUE)
        goto undrawn;

      style_mode = zMapStyleGetMode(style) ;


      /* NOTE TO ROY: I have probably not got the best logic below...I am sure that we
       * have not handled/thought about all the combinations of style modes and feature
       * types that might occur so will need to revisit this.... */
      switch(style_mode)
        {
        case ZMAPSTYLE_MODE_BASIC:
        case ZMAPSTYLE_MODE_GLYPH:
	  {
#ifdef RDS_REMOVED
	    ZMapWindowStatsBasic stats ;
	    stats = zmapWindowStatsAddBasic(parent_stats, feature) ;
	    stats->features++ ;
	    stats->items++ ;
#endif
	    method_table = factory->basic_methods;

	    if(feature->flags.has_score)
	      zmapWindowGetPosFromScore(style, feature->score, &(points[0]), &(points[2])) ;

	    method = &(method_table[style_mode]);

	    break;
	  }

        case ZMAPSTYLE_MODE_ALIGNMENT:
	  {
#ifdef RDS_REMOVED
	    ZMapWindowStatsAlign stats ;
	    stats = zmapWindowStatsAddAlign(parent_stats, feature) ;
	    stats->total_matches++ ;
#endif
	    method_table = factory->methods;

	    if (feature->flags.has_score)
	      zmapWindowGetPosFromScore(style, feature->score, &(points[0]), &(points[2])) ;


	    if (!zMapStyleIsAlignGaps(style) || !(feature->feature.homol.align))
	      {
#ifdef RDS_REMOVED
		stats->ungapped_matches++;
		stats->ungapped_boxes++;
		stats->total_boxes++;
#endif
		method = &(method_table[ZMAPSTYLE_MODE_BASIC]);              
	      }
	    else
	      {
		if (feature->feature.homol.flags.perfect)
		  {
#ifdef RDS_REMOVED
		    stats->gapped_matches++;
		    stats->total_boxes  += feature->feature.homol.align->len ;
		    stats->gapped_boxes += feature->feature.homol.align->len ;
#endif
		  }
		else
		  {
#ifdef RDS_REMOVED
		    stats->not_perfect_gapped_matches++;
		    stats->total_boxes++;
		    stats->ungapped_boxes++;
		    stats->imperfect_boxes += feature->feature.homol.align->len;
#endif
		  }

		method = &(method_table[style_mode]);
	      }

	    method = &(method_table[style_mode]);
	    
	    break;
	  }

        case ZMAPSTYLE_MODE_TEXT:
	  {
#ifdef RDS_REMOVED
	    ZMapWindowStatsBasic stats ;
	    stats = zmapWindowStatsAddBasic(parent_stats, feature) ;
	    stats->features++ ;
	    stats->items++ ;
#endif
	    method_table = factory->text_methods;

	    method = &(method_table[style_mode]);

	    break;
	  }

        case ZMAPSTYLE_MODE_GRAPH:
	  {
#ifdef RDS_REMOVED
	    ZMapWindowStatsBasic stats ;
	    stats = zmapWindowStatsAddBasic(parent_stats, feature) ;
	    stats->features++ ;
	    stats->items++ ;
#endif
	    method_table = factory->graph_methods;

	    method = &(method_table[style_mode]);

	    break;
	  }

        case ZMAPSTYLE_MODE_TRANSCRIPT:
	  {
#ifdef RDS_REMOVED
	    ZMapWindowStatsTranscript stats ;
	    stats = zmapWindowStatsAddTranscript(parent_stats, feature) ;
	    stats->transcripts++ ;

	    if (feature->feature.transcript.exons)
	      {
		stats->exons += feature->feature.transcript.exons->len ;
		stats->exon_boxes += feature->feature.transcript.exons->len ;
	      }

	    if (feature->feature.transcript.introns)
	      {
		/* There are two canvas items for every intron. */
		stats->introns += feature->feature.transcript.introns->len ;
		stats->intron_boxes += (feature->feature.transcript.introns->len * 2) ;
	      }

	    if (feature->feature.transcript.flags.cds)
	      {
		stats->cds++ ;
		stats->cds_boxes++ ;
	      }
#endif
	    method_table = factory->methods;
	    method = &(method_table[style_mode]);

	    break ;
	  }

	case ZMAPSTYLE_MODE_RAW_SEQUENCE:
	case ZMAPSTYLE_MODE_PEP_SEQUENCE:
	  {
	    method_table = factory->methods;

	    method = &(method_table[style_mode]);

	    break;
	  }

	case ZMAPSTYLE_MODE_ASSEMBLY_PATH:
	  {
	    method_table = factory->methods;

	    method = &(method_table[style_mode]);

	    break;
	  }

	default:
	  zMapAssertNotReached() ;
	  break;
	}

      
      run_data.factory   = factory;
      run_data.container = features_container;
      run_data.context   = context;
      run_data.align     = align;
      run_data.block     = block;
      run_data.set       = set;

      run_data.canvas_item = current_item;

      item   = ((method)->method)(&run_data, feature, limits[1],
				  points[0], points[1], 
				  points[2], points[3],
				  style);
      /* if(!run_data.item_in && item) */

      if (item)
        {
          gboolean status = FALSE;
          ZMapFrame frame;
	  ZMapStrand strand;

          g_object_set_data(G_OBJECT(item), ITEM_FEATURE_ITEM_STYLE, style) ;

	  frame  = zmapWindowContainerFeatureSetGetFrame((ZMapWindowContainerFeatureSet)parent_container);
	  strand = zmapWindowContainerFeatureSetGetStrand((ZMapWindowContainerFeatureSet)parent_container);
         
          if(factory->ftoi_hash)
	    {
	      status = zmapWindowFToIAddFeature(factory->ftoi_hash,
						align->unique_id, 
						block->unique_id, 
						set->unique_id, 
						strand, frame,
						feature->unique_id, item) ;
	    }

          status = (factory->user_funcs->top_item_created)(item, context, align, block, set, feature, factory->user_data);
          
          zMapAssert(status);

          return_item = item;
        }
    }
  else
    {
      zMapLogFatal("Feature %s is of unknown type: %d\n", 
		   (char *)g_quark_to_string(feature->original_id), feature->type) ;
    }

 undrawn:

  return return_item;
}


void zmapWindowFToIFactoryClose(ZMapWindowFToIFactory factory)
{
  factory->ftoi_hash  = NULL;
  factory->long_items = NULL;
  g_free(factory->user_funcs);
  factory->user_data  = NULL;
  /* font desc??? */
  /* factory->font_desc = NULL; */

  /* free the factory */
  g_free(factory);

  return ;
}



/*
 *                          INTERNAL
 */

static void copyCheckMethodTable(const ZMapWindowFToIFactoryMethodsStruct  *table_in, 
                                 ZMapWindowFToIFactoryMethodsStruct       **table_out)
{
  ZMapWindowFToIFactoryMethods methods = NULL;
  int i = 0;

  if(table_in && table_out && *table_out == NULL)
    {  
      /* copy factory_methods into factory->methods */
      methods = (ZMapWindowFToIFactoryMethods)table_in;
      while(methods && methods->method){ methods++; i++; } /* Get the size first! */
      
      /* Allocate */
      *table_out = g_new0(ZMapWindowFToIFactoryMethodsStruct, ++i);
      /* copy ... */
      memcpy(table_out[0], table_in, 
             sizeof(ZMapWindowFToIFactoryMethodsStruct) * i);
      methods = *table_out;
      i = 0;
      /* check that all is going to be ok with methods... */
      while(methods && methods->method)
        {
          if(!(methods->type == i))
            {
              zMapLogFatal("Bad Setup: expected %d, found %d\n", i, methods->type);
            }
          methods++;
          i++;
        }
    }
  else
    zMapAssertNotReached();

  return ;
}



static void datalistRun(gpointer key, gpointer list_data, gpointer user_data)
{
  ZMapFeature feature = (ZMapFeature)list_data;
  RunSet run_data = (RunSet)user_data;

  /* filter on frame! */
  if((run_data->frame != ZMAPFRAME_NONE) &&
     run_data->frame  != zmapWindowFeatureFrame(feature))
    return ;

  zmapWindowFToIFactoryRunSingle(run_data->factory, 
				 run_data->canvas_item,
                                 run_data->container,
                                 run_data->context,
                                 run_data->align,
                                 run_data->block,
                                 run_data->set,
                                 feature);
  
  return ;
}

inline
static void callItemHandler(ZMapWindowFToIFactory     factory,
                            FooCanvasItem            *new_item,
                            ZMapWindowItemFeatureType new_item_type,
                            ZMapFeature               full_feature,
                            ZMapWindowItemFeature     sub_feature,
                            double                    new_item_y1,
                            double                    new_item_y2)
{
  /* what was the non signal part of attachDataToItem.  _ALL_ our canvas feature MUST have these */
#ifdef RDS_DONT_INCLUDE
  g_object_set_data(G_OBJECT(new_item), ITEM_FEATURE_TYPE, GINT_TO_POINTER(new_item_type)) ;

  g_object_set_data(G_OBJECT(new_item), ITEM_FEATURE_DATA, full_feature) ;
#endif

  if(sub_feature != NULL)
    g_object_set_data(G_OBJECT(new_item), ITEM_SUBFEATURE_DATA, sub_feature);
  else
    zMapAssert(new_item_type != ITEM_FEATURE_CHILD &&
               new_item_type != ITEM_FEATURE_BOUNDING_BOX);

  if(factory->long_items && 0)
    zmapWindowLongItemCheck(factory->long_items, new_item, new_item_y1, new_item_y2);

  (factory->user_funcs->item_created)(new_item, new_item_type, full_feature, sub_feature, 
                                      new_item_y1, new_item_y2, factory->user_data);

  return ;
}



static double getWidthFromScore(ZMapFeatureTypeStyle style, double score)
{
  double tmp, width = 0.0 ;
  double fac, max_score, min_score ;

  width = zMapStyleGetWidth(style) ;
  min_score = zMapStyleGetMinScore(style) ;
  max_score = zMapStyleGetMaxScore(style) ;

  fac = width / (max_score - min_score) ;

  if (score <= min_score)
    tmp = 0 ;
  else if (score >= max_score) 
    tmp = width ;
  else 
    tmp = fac * (score - min_score) ;

  width = tmp ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      if (seg->data.f <= bc->meth->minScore) 
	x = 0 ;
      else if (seg->data.f >= bc->meth->maxScore) 
	x = bc->width ;
      else 
	x = fac * (seg->data.f - bc->meth->minScore) ;

      box = graphBoxStart() ;

      if (x > origin + 0.5 || x < origin - 0.5) 
	graphLine (bc->offset+origin, y, bc->offset+x, y) ;
      else if (x > origin)
	graphLine (bc->offset+origin-0.5, y, bc->offset+x, y) ;
      else
	graphLine (bc->offset+origin+0.5, y, bc->offset+x, y) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



  return width ;
}


static FooCanvasItem *drawSimpleFeature(RunSet run_data, ZMapFeature feature,
                                        double feature_offset,
					double x1, double y1, double x2, double y2,
                                        ZMapFeatureTypeStyle style)
{
  FooCanvasGroup        *parent = run_data->container;
  FooCanvasItem   *feature_item = NULL;
  ZMapWindowCanvasItem new_canvas_item;

  /* clip the coords to the block, extending end to canvas drawing coords. */
  /* Feature span coords are already clipped to block sapce.  
   * We just need to Seq2CanOffset */
  zmapWindowSeq2CanOffset(&y1, &y2, feature_offset);

  if((new_canvas_item = zMapWindowCanvasItemCreate(parent, y1, feature, style)))
    {
      zMapWindowCanvasItemSetIntervalType(new_canvas_item, ZMAP_WINDOW_BASIC_BOX);
      
      zMapWindowCanvasItemAddInterval(new_canvas_item, NULL, 0.0, y2 - y1, x1, x2);
  
      feature_item = FOO_CANVAS_ITEM(new_canvas_item);
    }

  return feature_item ;
}


static FooCanvasItem *drawGlyphFeature(RunSet run_data, ZMapFeature feature,
				       double feature_offset,
				       double x1, double y1, double x2, double y2,
				       ZMapFeatureTypeStyle style)
{
  ZMapWindowFToIFactory factory = run_data->factory;
  FooCanvasGroup        *parent = run_data->container;
  ZMapWindowCanvasItem new_canvas_item;
  FooCanvasItem *feature_item = NULL;
  guint line_width;

  line_width = factory->line_width;

  zmapWindowSeq2CanOffset(&y1, &y2, feature_offset) ;	    /* Make sure we cover the whole last base. */

#ifdef RDS_DONT_INCLUDE
  /* There will be other alternatives to splice once we add them to the the canvas. */
  if (feature->flags.has_boundary && 0)
    {
      GdkColor *splice_background ;
      double width, origin, start ;
      double style_width, max_score, min_score ;
      ZMapDrawGlyphType glyph_type ;
      ZMapFrame frame ;
      gboolean status ;
      ZMapStyleColourTarget target ;

      frame = zmapWindowFeatureFrame(feature) ;

      switch (frame)
	{
	case ZMAPFRAME_0:
	  target = ZMAPSTYLE_COLOURTARGET_FRAME0 ;
	  break ;
	case ZMAPFRAME_1:
	  target = ZMAPSTYLE_COLOURTARGET_FRAME1 ;
	  break ;
	case ZMAPFRAME_2:
	  target = ZMAPSTYLE_COLOURTARGET_FRAME2 ;
	  break ;
	default:
	  zMapAssertNotReached() ;
	}

      status = zMapStyleGetColours(style, target, ZMAPSTYLE_COLOURTYPE_NORMAL, &splice_background, NULL, NULL) ;
      zMapAssert(status) ;

      if (feature->boundary_type == ZMAPBOUNDARY_5_SPLICE)
	{
	  glyph_type = ZMAPDRAW_GLYPH_DOWN_BRACKET ;
	}
      else
	{
	  glyph_type = ZMAPDRAW_GLYPH_UP_BRACKET ;
	}

      style_width = zMapStyleGetWidth(style) ;
      min_score = zMapStyleGetMinScore(style) ;
      max_score = zMapStyleGetMaxScore(style) ;

      if (min_score < 0 && 0 < max_score)
	origin = 0 ;
      else
	origin = style_width * (min_score / (max_score - min_score)) ;


      /* Adjust width to score....NOTE that to do acedb like stuff we really need to set an origin
	 and pass it in to this routine....*/
      width = getWidthFromScore(style, feature->score) ;

      x1 = 0.0;                 /* HACK */
      if (width > origin + 0.5 || width < origin - 0.5)
	{
	  start = x1 + origin ;
	  width = width - origin ;
	  /* graphLine (bc->offset+origin, y, bc->offset+x, y) ; */
	}
      else if (width > origin)
	{
	  /*graphLine (bc->offset+origin-0.5, y, bc->offset+x, y) ; */
	  start = x1 + origin - 0.5 ;
	  width = width - origin - 0.5 ;
	}
      else
	{
	  /* graphLine (bc->offset+origin+0.5, y, bc->offset+x, y) ; */
	  start = x1 + origin + 0.5 ;
	  width = width - origin + 0.5 ;
	}

      feature_item = zMapDrawGlyph(parent, start, (y2 + y1) * 0.5,
				   glyph_type,
				   splice_background, width, 2) ;
    }
  else
    {
#endif
      if((new_canvas_item = zMapWindowCanvasItemCreate(parent, y1, feature, style)))
	{
	  int interval_type;
	  /* interval_type = style->interval_type; ... */
	  interval_type = ZMAP_WINDOW_BASIC_BOX;

	  zMapWindowCanvasItemSetIntervalType(new_canvas_item, interval_type);
	  
	  zMapWindowCanvasItemAddInterval(new_canvas_item, NULL, 0.0, y2 - y1, x1, x2);
	  
	  feature_item = FOO_CANVAS_ITEM(new_canvas_item);
	}
      //    }

  return feature_item ;
}




/* Draws a series of boxes connected by centrally placed straight lines which represent the match
 * blocks given by the align array in an alignment feature.
 * 
 * NOTE: returns NULL if the align array is null, i.e. you must call drawSimpleFeature to draw
 * an alignment which has no gaps or just to draw it without gaps.
 *  */

/* Warning about actual data:
 * 
 * feature->homol.aligns[
 * {
 *   t1 = 575457,   q1 = 1043,
 *   t2 = 575457,   q2 = 1043,
 *   t_strand = +,  q_strand = +,
 * },
 * {
 *   t1 = 575461,   q1 = 1040,
 *   t2 = 575463,   q2 = 1042,
 *   t_strand = +,  q_strand = +,
 * },
 * {
 *   t1 = 575467,   q1 = 1033,
 *   t2 = 575470,   q2 = 1039,
 *   t_strand = +,  q_strand = +,
 * },
 * ]
 */

/* made me write GapAlignBlockFromAdjacentBlocks */
static FooCanvasItem *drawAlignFeature(RunSet run_data, ZMapFeature feature,
                                       double feature_offset,
                                       double x1, double y1, double x2, double y2,
                                       ZMapFeatureTypeStyle style)
{
  FooCanvasGroup *parent = run_data->container;
  FooCanvasItem *feature_item = NULL ;
  ZMapWindowCanvasItem new_canvas_item;
  ZMapWindowFToIFactory factory = run_data->factory;
  ZMapFeatureBlock        block = run_data->block;
  GdkColor *outline = NULL, *foreground = NULL, *background = NULL ;
  guint match_threshold = 0, line_width = 0;
  gboolean status ;

  /* Get the colours, it's an error at this stage if none set we don't draw. */
  /* We could speed performance by caching these colours and only changing them when the style
   * quark changes. */
  status = zMapStyleGetColours(style, ZMAPSTYLE_COLOURTARGET_NORMAL, ZMAPSTYLE_COLOURTYPE_NORMAL,
			       &background, &foreground, &outline) ;
  zMapAssert(status) ;

  /* If we are here, style should be ok for display. */

  if (!zMapStyleIsAlignGaps(style) || !(feature->feature.homol.align))
    {
      double feature_start, feature_end;
      feature_start = feature->x1;
      feature_end   = feature->x2; /* I want this function to find these values */
      zmapWindowSeq2CanOffset(&feature_start, &feature_end, feature_offset) ;

      if(!(feature_item = run_data->canvas_item))
	{
	  new_canvas_item = zMapWindowCanvasItemCreate(parent, feature_start, feature, style);
	  feature_item = FOO_CANVAS_ITEM(new_canvas_item);
	}
      else
	{
	  new_canvas_item = ZMAP_CANVAS_ITEM(feature_item);
	  zMapWindowCanvasItemClear(new_canvas_item);
	}

      zMapWindowCanvasItemSetIntervalType(new_canvas_item, ZMAP_WINDOW_ALIGNMENT_NO_GAPS);

      zmapWindowExt2Zero(&feature_start, &feature_end);

      zMapWindowCanvasItemAddInterval(new_canvas_item, NULL, feature_start, feature_end, x1, x2);
    }
  else
    {
      /* Are we drawing gapped alignments? get the match threshold */
      zMapStyleGetGappedAligns(style, &match_threshold) ;
      
      /* line width from the factory? Should this be from the style? */
      line_width = factory->line_width;
      
      if (feature->feature.homol.align)
	{
	  ZMapAlignBlock first_match_block = NULL, prev_match_block;
	  double limits[]       = {0.0, 0.0, 0.0, 0.0};
	  double feature_start, feature_end;
	  double first_item_top    = 0.0;
	  double first_item_bottom = 0.0;
	  double prev_item_bottom;
	  guint i = 0;		/* Get First Block */
	  
	  limits[1] = block->block_to_sequence.q1;
	  limits[3] = block->block_to_sequence.q2;
	  
	  feature_start = feature->x1;
	  feature_end   = feature->x2; /* I want this function to find these values */
#ifdef DEBUG_COORDS
	  printf("feature %d -> %d [%d -> %d]\n", 
		 feature->x1, feature->x2, 
		 feature->x1 - feature->x1,
		 feature->x2 - feature->x1 + 1);
#endif /* DEBUG_COORDS */
	  zmapWindowSeq2CanOffset(&feature_start, &feature_end, feature_offset) ;
#ifdef DEBUG_COORDS
	  printf("feature %f -> %f [%f, %f]\n", 
		 feature_start, feature_end, 
		 feature_start - feature_start, feature_end - feature_start);
#endif /* DEBUG_COORDS */
	  if(!(feature_item = run_data->canvas_item))
	    {
	      new_canvas_item = zMapWindowCanvasItemCreate(parent, feature_start, feature, style);
	      feature_item    = FOO_CANVAS_ITEM(new_canvas_item);
	    }
	  else
	    {
	      new_canvas_item = ZMAP_CANVAS_ITEM(feature_item);
	      zMapWindowCanvasItemClear(new_canvas_item);
	    }
	  
	  /* Calculate total offset for for subparts of the feature. */
	  
	  /* Algorithm here is:
	   * - Get First Block (first)
	   * Loop
	   *   - Get Next Block (this)
	   *   - Calculate Gap
	   *   - Draw Gap _if_ there is one
	   *   - Draw Block (this)
	   * - Draw First Block (first)
	   *
	   * Sadly, it only really avoids a if(prev_match_block) in the loop and
	   * doesn't cure any issue with the the gap lines overlapping the
	   * match blocks (this requires 2 loops or a change to the way we draw all features). 
	   */
	  
	  first_match_block = prev_match_block =
	    &(g_array_index(feature->feature.homol.align, ZMapAlignBlockStruct, i));
	  
	  first_item_top    = first_match_block->t1;
	  first_item_bottom = first_match_block->t2;
	  
	  /* Do a size request to check we need not be clipped */
	  clip_feature_coords(factory, feature, feature_offset,
			      &first_item_top, &first_item_bottom);
	  
	  first_item_top    -= feature_start;
	  first_item_bottom -= feature_start;

	  prev_item_bottom   = first_item_bottom;
	  	  
	  /* Do any more we have... */
	  for (i = 1; prev_match_block && i < feature->feature.homol.align->len ; i++)  
	    {
	      ZMapWindowItemFeature gap_data, align_data ;
	      ZMapAlignBlockStruct gap_block, *match_block;
	      FooCanvasItem *match;
	      double item_top, item_bottom;
	      int q_indel, t_indel;
	      gboolean expect_problems = TRUE;
	      gboolean t_indel_set, q_indel_set;
	      
	      match_block = &(g_array_index(feature->feature.homol.align, ZMapAlignBlockStruct, i));
	      item_top    = match_block->t1;
	      item_bottom = match_block->t2;
	      
	      clip_feature_coords(factory, feature, feature_offset,
				  &item_top, &item_bottom);
	      
	      item_top    -= feature_start;
	      item_bottom -= feature_start;

	      /* Check the strandedness of prev and current
	       * align_spans. This should be removed when Ed finishes
	       * the correct dumping in the sgifaceserver. */
	      if(expect_problems)
		{
		  
		  /* Problems expected are due to some services
		   * providing data to us which relies on the order of
		   * the coordinates to determine strand.  This is all
		   * well and good for everything _except_ single
		   * bases. We probably don't do enough to trap this 
		   * elsewhere, by the time we are here, we should just
		   * be drawing... */
		  
		  if(match_block->t_strand != prev_match_block->t_strand &&
		     match_block->t1 == match_block->t2)
		    match_block->t_strand = prev_match_block->t_strand;
		  
		  if(match_block->q_strand != prev_match_block->q_strand &&
		     match_block->q1 == match_block->q2)
		    match_block->q_strand = prev_match_block->q_strand;

		  /* Sadly we need to check _again_ as the prev block
		   * could be the one with the single base match.
		   * Only a problem for the _first_ one I think...sigh... */
		  /* Anyway we set the prev one's strand to match this 
		   * one, rather than vice versa as above. */
		  if(match_block->t_strand != prev_match_block->t_strand &&
		     prev_match_block->t1 == prev_match_block->t2)
		    prev_match_block->t_strand = match_block->t_strand;
	      
		  if(match_block->q_strand != prev_match_block->q_strand &&
		     prev_match_block->q1 == prev_match_block->q2)
		    prev_match_block->q_strand = match_block->q_strand;
		}
	      
	  

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	      /* THIS NEEDS MORE THOUGHT......WE CAN'T JUST ASSERT HERE, WE EITHER NEED
	       * TO CHUCK OUT THE DATA AT AN EARLIER STAGE (E.G. GFF PARSER) OR HANDLE
	       * IT AT THIS STAGE...BOMBING OUT IS NO GOOD..... */


	      /* These may or may not be good to assert... */
	      /* I don't see any reason why someone shouldn't supply
	       * data that will break these, but I'm assuming the hit
	       * would actually be a separate HSP at that point.
	       * These are _after_ the above "expect_problems" code!
	       */
	      zMapAssert(match_block->t_strand == prev_match_block->t_strand);
	      zMapAssert(match_block->q_strand == prev_match_block->q_strand);
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	  
	      /* Get the Gap AlignBlock from the adjacent Match AlignBlocks. */
	      GapAlignBlockFromAdjacentBlocks(prev_match_block, match_block, &gap_block, &t_indel_set, &q_indel_set);
	  
	      /* Null this... */
	      prev_match_block = NULL;

	      t_indel = gap_block.t2 - gap_block.t1;
	      q_indel = gap_block.q2 - gap_block.q1;
	    
	      if(t_indel_set)
		{
		  FooCanvasItem *gap;
		  double gap_top, gap_bottom;

		  gap_data          = g_new0(ZMapWindowItemFeatureStruct, 1) ;
		  gap_data->subpart = ZMAPFEATURE_SUBPART_GAP ;
		  gap_data->start   = gap_block.t1;
		  gap_data->end     = gap_block.t2;
		  gap_data->query_start = 0;
		  gap_data->query_end   = 0;

		  if(q_indel_set)
		    {
		      gap_data->query_start = gap_block.q1;
		      gap_data->query_end   = gap_block.q2;
		    }
		  gap_top    = gap_block.t1;
		  gap_bottom = gap_block.t2;

		  /* we don't need to clip check here, but item that do have feature_offset seq2can so add here. */
		  zmapWindowSeq2CanOffset(&gap_top, &gap_bottom, feature_start + feature_offset);

		  zMapWindowCanvasItemSetIntervalType(new_canvas_item, ZMAP_WINDOW_ALIGNMENT_GAP);
#ifdef DEBUG_COORDS
		  printf("\tgap %f, %f\n", gap_top, gap_bottom);
#endif /* DEBUG_COORDS */
		  gap = zMapWindowCanvasItemAddInterval(new_canvas_item, gap_data, 
							gap_top, gap_bottom, x1, x2);

		  g_object_set_data(G_OBJECT(gap), ITEM_SUBFEATURE_DATA, gap_data);

#ifdef RDS_BREAKING_STUFF
		  if(q_indel - 1 > match_threshold)
		    line_colour = &c_colours.colinear;
#endif
		  
		}

	      align_data              = g_new0(ZMapWindowItemFeatureStruct, 1) ;
	      align_data->subpart     = ZMAPFEATURE_SUBPART_MATCH ;
	      align_data->start       = match_block->t1 ;
	      align_data->end         = match_block->t2 ;
	      align_data->query_start = match_block->q1 ;
	      align_data->query_end   = match_block->q2 ;

	      zMapWindowCanvasItemSetIntervalType(new_canvas_item, ZMAP_WINDOW_ALIGNMENT_MATCH);
#ifdef DEBUG_COORDS
	      printf("\tmatch %f, %f\n", item_top, item_bottom);
#endif /* DEBUG_COORDS */
	      match = zMapWindowCanvasItemAddInterval(new_canvas_item, align_data, item_top, item_bottom, x1, x2);

	      g_object_set_data(G_OBJECT(match), ITEM_SUBFEATURE_DATA, align_data);

	      /* Save this block for next iteration... Incl. the
	       * Seq2CanOffset of the Bottom of this drawn align box */
	      prev_match_block = match_block;
	      prev_item_bottom = item_bottom;

	    }

	  if(first_match_block)
	    {
	      FooCanvasItem *match;
	      ZMapWindowItemFeature align_data;

	      align_data              = g_new0(ZMapWindowItemFeatureStruct, 1) ;
	      align_data->subpart     = ZMAPFEATURE_SUBPART_MATCH ;
	      align_data->start       = first_match_block->t1 ;
	      align_data->end         = first_match_block->t2 ;
	      align_data->query_start = first_match_block->q1 ;
	      align_data->query_end   = first_match_block->q2 ;
	  
	      match = zMapWindowCanvasItemAddInterval(new_canvas_item, align_data, 
						      first_item_top, first_item_bottom, x1, x2);
	      g_object_set_data(G_OBJECT(match), ITEM_SUBFEATURE_DATA, align_data);
#ifdef DEBUG_COORDS
	      printf("\t1st %f, %f\n", first_item_top, first_item_bottom);
#endif /* DEBUG_COORDS */
	    }
	}
    }
    
  return feature_item ;
}

static void get_exon_regions(ZMapSpan exon_cds, ZMapSpan utr_one, ZMapSpan utr_two,
			     double cds_start, double cds_end, int *first_out, int *last_out)
{
  int first = 0, last = 0;

  if((cds_start > exon_cds->x2) || (cds_end < exon_cds->x1))
    {
      first = last = 1;
      utr_one->x1  = exon_cds->x1;
      utr_one->x2  = exon_cds->x2;
      exon_cds->x1 = exon_cds->x2 = 0.0;
    }  
  else
    {
      if((cds_start > exon_cds->x1) && (cds_start <= exon_cds->x2))
	{
	  utr_one->x1  = exon_cds->x1;
	  /* These are the same so they butt up  */
	  utr_one->x2  = cds_start; 
	  exon_cds->x1 = cds_start;
	  last++;
	}
      if((cds_end >= exon_cds->x1) && (cds_end < exon_cds->x2))
	{
	  ZMapSpan utr_ptr = utr_one;

	  if(last == 1)
	    utr_ptr = utr_two;
	  
	  utr_ptr->x2  = exon_cds->x2;
	  utr_ptr->x1  = cds_end;
	  exon_cds->x2 = cds_end;

	  last++;
	}
    }

  if(first_out)
    *first_out = first;
  if(last_out)
    *last_out  = last;

  return ;
}

static void new_draw_feature_exon(FooCanvasItem *feature_item,
				  double feature_offset,
				  double item_top,   double item_bottom,
				  double cds_start,  double cds_end,
				  double item_right, double line_width,
				  gboolean has_cds)
{
  ZMapWindowCanvasItem canvas_item;
  ZMapSpanStruct regions[3] = {{0}};
  int j, last, first, top;
  int cds_idx = 0;
  gboolean cdsless_use_utr_colours = TRUE;

  regions[cds_idx].x1 = item_top;
  regions[cds_idx].x2 = item_bottom;

  /* top != feature_offset */
  top = (FOO_CANVAS_GROUP(feature_item))->ypos;

  if(has_cds)
    get_exon_regions(&regions[0], &regions[1], &regions[2], 
		     cds_start, cds_end, &first, &last);
  else if(!cdsless_use_utr_colours)
    first = last = cds_idx;
  else
    {
      int utr_idx = 1;
      regions[utr_idx] = regions[cds_idx]; /* struct copy */
      first = last = utr_idx;
    }

  canvas_item = ZMAP_CANVAS_ITEM(feature_item);

  /* draw the cds [0] over the utr [1,2] */
  for(j = last; j >= first; --j)
    {
      FooCanvasItem *exon_item = NULL;
      ZMapWindowItemFeature exon_data;

      exon_data = g_new0(ZMapWindowItemFeatureStruct, 1);
      /* actual start will be canvas coord + feature_offset */
      exon_data->start = regions[j].x1 + feature_offset;
      exon_data->end   = regions[j].x2;
      /* we have to translate these to the feature_item coord space */
      regions[j].x2   -= top;
      regions[j].x1   -= top;

      /* Do something about data here... */
      if(j == cds_idx)
	{
	  exon_data->subpart = ZMAPFEATURE_SUBPART_EXON_CDS;
	  zMapWindowCanvasItemSetIntervalType(canvas_item, ZMAP_WINDOW_TRANSCRIPT_EXON_CDS);
	}
      else
	{
	  exon_data->subpart = ZMAPFEATURE_SUBPART_EXON;
	  zMapWindowCanvasItemSetIntervalType(canvas_item, ZMAP_WINDOW_TRANSCRIPT_EXON);
	}

      exon_item = zMapWindowCanvasItemAddInterval(canvas_item,
						  exon_data,
						  (double)(regions[j].x1),
						  (double)(regions[j].x2),
						  (double)0.0, 
						  (double)item_right);

    }

  return ;
}

static gboolean invoke_size_request(ZMapWindowFToIFactory factory,
				    ZMapFeature           feature,
				    double *start, double *end)
{
  ZMapWindowFToIFactoryItemFeatureSizeRequest feature_size_request;
  double points_inout[] = {0.0, 0.0, 0.0, 0.0};
  gboolean drawable = FALSE;
  
  if(start && end)
    {
      feature_size_request = factory->user_funcs->feature_size_request;

      points_inout[1] = *start;
      points_inout[3] = *end;

      if(!((feature_size_request)(feature, factory->limits, 
				  &points_inout[0], factory->user_data)))
	{
	  drawable = TRUE;
	  *start   = points_inout[1];
	  *end     = points_inout[3];
	}
    }

  return drawable;
}

static gboolean clip_feature_coords(ZMapWindowFToIFactory factory,
				    ZMapFeature           feature,
				    double                offset,
				    double               *start_inout,
				    double               *end_inout)
{
  gboolean drawable = FALSE;

  /* make sure we're within the block size we're drawing */
  if((drawable = invoke_size_request(factory, feature, start_inout, end_inout)))
    {
      /* alter the start and end to be block coords "zeroed" in block space */
      /* N.B. That the end is extended + 1, as that's how we draw items. */
      zmapWindowSeq2CanOffset(start_inout, end_inout, offset);
    }

  return drawable;
}

static FooCanvasItem *drawTranscriptFeature(RunSet run_data,  ZMapFeature feature,
                                            double feature_offset,
                                            double x1, double y1, double x2, double y2,
                                            ZMapFeatureTypeStyle style)
{
  ZMapWindowFToIFactory factory = run_data->factory;
  FooCanvasGroup        *parent = run_data->container;
  FooCanvasItem   *feature_item = run_data->canvas_item;
  ZMapWindowCanvasItem canvas_item;
  gboolean has_cds = FALSE, status = TRUE;
  double cds_start = 0.0, cds_end = 0.0 ; /* cds_start < cds_end */
  double feature_start, feature_end;
  double item_left, item_right;

  feature_start = y1;
  feature_end   = y2;

  zmapWindowSeq2CanOffset(&feature_start, &feature_end, feature_offset) ;  

  if(status)
    {
      if(!feature_item)
	{
	  canvas_item = zMapWindowCanvasItemCreate(parent, feature_start, feature, style);
	  feature_item = FOO_CANVAS_ITEM(canvas_item);
	}
      else
	{
	  canvas_item = ZMAP_CANVAS_ITEM(feature_item);
	  zMapWindowCanvasItemClear(canvas_item);
	  foo_canvas_item_set(feature_item, "y", feature_start, NULL);
	}
    }

  if(status && feature->feature.transcript.flags.cds)
    {
      has_cds = TRUE;

      /* Set up the cds coords if there is one. */
      cds_start = feature->feature.transcript.cds_start ;
      cds_end   = feature->feature.transcript.cds_end ;
      
      /* clip the coords to the block, extending end to canvas drawing coords. */
      /* we _must_ do this to the cds as it will be compared later to the exon coords */
      if(!clip_feature_coords(factory, feature, feature_offset, &cds_start, &cds_end))
        {
	  g_warning("CDS coords clipped");
	}
    }

  item_left  = x1;
  item_right = x2 - factory->line_width;

  if (status && feature_item &&
      feature->feature.transcript.introns && 
      feature->feature.transcript.exons)
    {
      double line_width = 1.5;
      double parent_ypos = 0.0;
      int i;
      
      parent_ypos = FOO_CANVAS_GROUP(feature_item)->ypos;
      
      zMapWindowCanvasItemSetIntervalType(canvas_item, ZMAP_WINDOW_TRANSCRIPT_INTRON);

      for (i = 0 ; i < feature->feature.transcript.introns->len ; i++)  
	{
	  ZMapWindowItemFeature intron;
	  FooCanvasItem  *intron_line ;
	  double item_top, item_bottom ;
	  ZMapSpan intron_span ;
	  
	  intron_span = &g_array_index(feature->feature.transcript.introns, ZMapSpanStruct, i) ;
	  
	  item_top    = intron_span->x1;
	  item_bottom = intron_span->x2;

	  /* clip the coords to the block, extending end to canvas drawing coords. */
	  if(!clip_feature_coords(factory, feature, feature_offset, &item_top, &item_bottom))
	    continue;

	  /* set up the item sub feature data */
	  intron          = g_new0(ZMapWindowItemFeatureStruct, 1);
	  intron->start   = intron_span->x1;
	  intron->end     = intron_span->x2;
	  intron->subpart = ZMAPFEATURE_SUBPART_INTRON;

	  /* this is a sub item we're drawing _within_ the feature_item,
	   * so we need to zero in its space. new_draw_feature_exon does
	   * this for exons */
	  item_top       -= parent_ypos;
	  item_bottom    -= parent_ypos;

	  /* And draw the item. */
	  intron_line     = zMapWindowCanvasItemAddInterval(canvas_item, intron,
							    item_top, item_bottom,
							    item_left, item_right);
	}
      
      for (i = 0; i < feature->feature.transcript.exons->len; i++)
	{
	  ZMapSpan exon_span ;
	  double item_top, item_bottom ;
	  
	  exon_span = &g_array_index(feature->feature.transcript.exons, ZMapSpanStruct, i);
	  
	  item_top    = exon_span->x1;
	  item_bottom = exon_span->x2;
	  
	  /* clip the coords to the block, extending end to canvas drawing coords. */
	  if (!clip_feature_coords(factory, feature, feature_offset, &item_top, &item_bottom))
	    continue;

	  new_draw_feature_exon(feature_item, feature_offset,
				item_top, item_bottom, 
				cds_start, cds_end,
				item_right, line_width, has_cds);
	}
    }
  else if(status && feature_item)
    {
      double item_top, item_bottom;
      float line_width = 1.5 ;
      
      /* item_top gets feature_offset added and item_bottom gets 1 added, 
       * as feature_start & feature_end have already been through 
       * zmapWindowSeq2CanOffset(), but we still need to clip_feature_coords(). */
      item_top    = feature_start + feature_offset;
      item_bottom = feature_end   + feature_offset - 1.0;
      
      /* clip the coords to the block, extending end to canvas drawing coords. */
      if(clip_feature_coords(factory, feature, feature_offset, &item_top, &item_bottom))
	{
	  /* This is a single exon transcript.  We can't just get away
	   * with using the simple feature code.. Single exons may have a
	   * cds */
	  new_draw_feature_exon(feature_item, feature_offset,
				item_top, item_bottom, 
				cds_start, cds_end,
				item_right, line_width, has_cds);
	}
    }
  else
    {
      zMapLogWarning("Must have failed to create a canvas item for feature '%s' [%s]",
		     g_quark_to_string(feature->original_id),
		     g_quark_to_string(feature->unique_id));
    }

  if(feature_item)
    {
      GError *error = NULL;
      if(!(zMapWindowCanvasItemCheckData(canvas_item, &error)))
	{
	  zMapLogCritical("Error: %s", (error ? error->message : "No error produced"));
	  g_error_free(error);
	}
    }	

  return feature_item;
}

static FooCanvasItem *drawSeqFeature(RunSet run_data,  ZMapFeature feature,
                                     double feature_offset,
                                     double x1, double y1, double x2, double y2,
                                     ZMapFeatureTypeStyle style)
{
  ZMapFeatureBlock feature_block = run_data->block;
  FooCanvasItem  *text_item_parent = NULL;

  if(!feature_block->sequence.sequence)
    {
      zMapLogWarning("%s", "Trying to draw a seq feature, but there's no sequence!");
      return NULL;
    }

  text_item_parent = drawFullColumnTextFeature(run_data, feature, feature_offset,
                                               x1, y1, x2, y2, style);

  return text_item_parent;
}

static FooCanvasItem *drawPepFeature(RunSet run_data,  ZMapFeature feature,
                                     double feature_offset,
                                     double x1, double y1, double x2, double y2,
                                     ZMapFeatureTypeStyle style)
{
  FooCanvasItem *text_item_parent;

  text_item_parent = drawFullColumnTextFeature(run_data, feature, feature_offset, 
                                               x1, y1, x2, y2, style);

  return text_item_parent;
}


static gint canvas_allocate_protein_cb(FooCanvasItem   *item,
				       ZMapTextDrawData draw_data,
				       gint             max_width,
				       gint             max_buffer_size,
				       gpointer         user_data)
{
  int buffer_size = max_buffer_size;
  int char_per_line, line_count;
  int cx1, cy1, cx2, cy2, cx, cy; /* canvas coords of scroll region and item */
  int table;
  int width, height;
  double world_range, raw_chars_per_line, raw_lines;
  int real_chars_per_line, canvas_range;
  int real_lines, real_lines_space;

  /* A bit I don't like. Handles the top/bottom border. */
  if(draw_data->y1 < 0.0)
    {
      double t = 0.0 - draw_data->y1;
      draw_data->y1  = 0.0;
      draw_data->y2 -= t;
    }
  /* scroll region and text position to canvas coords */
  foo_canvas_w2c(item->canvas, draw_data->x1, draw_data->y1, &cx1, &cy1);
  foo_canvas_w2c(item->canvas, draw_data->x2, draw_data->y2, &cx2, &cy2);
  foo_canvas_w2c(item->canvas, draw_data->wx, draw_data->wy, &cx,  &cy);

  /* We need to know the extents of a character */
  width  = draw_data->table.ch_width;
  height = draw_data->table.ch_height;

  /* world & canvas range to work out rough chars per line */
  world_range        = (draw_data->y2 - draw_data->y1 + 1) / 3.0;
  canvas_range       = (cy2 - cy1 + 1);
  raw_chars_per_line = ((world_range * height) / canvas_range);
  
  /* round up raw to get real. */
  if((double)(real_chars_per_line = (int)raw_chars_per_line) < raw_chars_per_line)
    real_chars_per_line++;
  

  /* how many lines? */
  raw_lines = world_range / real_chars_per_line;
  
  /* round up raw to get real */
  if((double)(real_lines = (int)raw_lines) < raw_lines)
    real_lines++;
  
  /* How much space do real_lines takes up? */
  real_lines_space = real_lines * height;
  
  if(real_lines_space > canvas_range)
    {
      /* Ooops... Too much space, try one less */
      real_lines--;
      real_lines_space = real_lines * height;
    }
  
  /* Make sure we fill the space... */
  if(real_lines_space < canvas_range)
    {
      double spacing_dbl = canvas_range;
      double delta_factor = 0.15;
      int spacing;
      spacing_dbl -= (real_lines * height);
      spacing_dbl /= real_lines;
      
      /* need a fudge factor here! We want to round up if we're
       * within delta factor of next integer */
      
      if(((double)(spacing = (int)spacing_dbl) < spacing_dbl) &&
	 ((spacing_dbl + delta_factor) > (double)(spacing + 1)))
	spacing_dbl = (double)(spacing + 1);
      
      draw_data->table.spacing = (int)(spacing_dbl * PANGO_SCALE);
      draw_data->table.spacing = spacing_dbl;
    }
  
  /* Now we've set the number of lines we have */
  line_count    = real_lines;
  /* Not too wide! Default! */
  char_per_line = max_width;
  /* Record this so we can do index calculations later */
  draw_data->table.untruncated_width = real_chars_per_line;

  if(real_chars_per_line <= char_per_line)
    {
      char_per_line = real_chars_per_line;
      draw_data->table.truncated = FALSE;
    }
  else
    draw_data->table.truncated = TRUE; /* truncated to max_width. */

  /* Record the table dimensions. */
  draw_data->table.width  = char_per_line;
  draw_data->table.height = line_count;
  /* which has table number of cells */
  table = char_per_line * line_count;

  /* We absolutely must not be bigger than buffer_size. */
  buffer_size = MIN(buffer_size, table);

  return buffer_size;
}

static gint canvas_allocate_dna_cb(FooCanvasItem   *item,
				   ZMapTextDrawData draw_data,
				   gint             max_width,
				   gint             buffer_size,
				   gpointer         user_data)
{

  buffer_size = zMapWindowTextItemCalculateZoomBufferSize(item, draw_data, buffer_size);

  return buffer_size;
}

static gint canvas_fetch_feature_text_cb(FooCanvasItem *text_item, 
					 ZMapTextDrawData draw_data,
					 char *buffer_in_out,
					 gint  buffer_size,
					 gpointer user_data)
{
  ZMapFeature feature = (ZMapFeature)user_data;
  /*  ZMapFeatureAny feature_any = (ZMapFeatureAny)user_data; */
  char *buffer_ptr = buffer_in_out;
  char *seq = NULL, *seq_ptr;
  int itr, start;
  int table_size;
  int untruncated_size;
  int bases;
  gboolean if_last = FALSE, else_last = FALSE, debug = FALSE;
  gboolean report_table_size = FALSE;

  table_size       = draw_data->table.width * draw_data->table.height;
  bases            = draw_data->table.untruncated_width;
  untruncated_size = draw_data->table.height * bases;

  memset(&buffer_in_out[table_size], 0, buffer_size - table_size);

  if(report_table_size)
    printf("table size = %d\n", table_size);

  start   = (int)(draw_data->wy);

  if (feature->type == ZMAPSTYLE_MODE_RAW_SEQUENCE)
    {
      seq_ptr = feature->feature.sequence.sequence + start;
    }
  else if(feature->type == ZMAPSTYLE_MODE_PEP_SEQUENCE)
    {
      int protein_start;
      /* Get the protein index that the world y corresponds to. */
      protein_start = (int)(draw_data->wy) - feature->x1;
      protein_start = (int)(protein_start / 3);

      seq_ptr  = feature->feature.sequence.sequence;
      seq_ptr += protein_start;
    }
  else
    seq_ptr = feature->description;

  if(debug)
    printf("123456789|123456789|123456789|123456789|\n");

  /* make sure we include the end of the last row. */
  table_size++;

  for(itr = 0; itr < table_size; itr++, buffer_ptr++, seq_ptr++)
    {
      if(draw_data->table.truncated && 
	 (buffer_ptr != buffer_in_out) &&
	 ((itr) % (draw_data->table.width)) == 0)
	{
	  int e;

	  seq_ptr += (bases - draw_data->table.width);

	  if(debug)
	    printf("\t%d...\n", (bases - draw_data->table.width));

	  buffer_ptr-=3;	/* rewind. */

	  for(e = 0; e<3; e++, buffer_ptr++)
	    {
	      *buffer_ptr = '.';
	    }

	  if_last   = TRUE;
	  else_last = FALSE;

	  /* need to do what we've missed doing in else... */
	  *buffer_ptr = *seq_ptr;
	  if(debug)
	    printf("%c", *seq_ptr);
	}
      else
	{
	  if(debug)
	    printf("%c", *seq_ptr);

	  *buffer_ptr = *seq_ptr;
	  if_last   = FALSE;
	  else_last = TRUE;
	}
    }

  /* because of table_size++ above. */
  *(--buffer_ptr) = '\0';

  if(debug)
    printf("\n");

  if(seq)
    g_free(seq);

  return buffer_ptr - buffer_in_out;
}


static gboolean item_to_char_cell_coords2(FooCanvasPoints **points_out,
					  FooCanvasItem    *subject, 
					  gpointer          user_data)
{
  FooCanvasItem *overlay_item;
  FooCanvasGroup *overlay_group, *container;
  ZMapFeature subject_feature, overlay_feature;
  FooCanvasPoints *points;
  double first[ITEMTEXT_CHAR_BOUND_COUNT], last[ITEMTEXT_CHAR_BOUND_COUNT];
  int index1, index2;
  gboolean first_found, last_found;
  gboolean do_overlay = FALSE;
  gboolean redraw = FALSE;

  if(FOO_IS_CANVAS_ITEM(user_data) &&
     FOO_IS_CANVAS_GROUP(user_data))
    {
      overlay_group = FOO_CANVAS_GROUP(user_data);
      overlay_item  = FOO_CANVAS_ITEM(overlay_group->item_list->data);

      container = (FooCanvasGroup *)zmapWindowContainerCanvasItemGetContainer(overlay_item);

      do_overlay = (gboolean)(FOO_CANVAS_ITEM(container)->object.flags & FOO_CANVAS_ITEM_VISIBLE);
    }

  if(do_overlay)
    {
      ZMapWindowItemFeatureType feature_type;
      ZMapFrame subject_frame, overlay_frame;
#ifdef RDS_DONT_INCLUDE
      subject_feature  = g_object_get_data(G_OBJECT(subject),
					   ITEM_FEATURE_DATA);
      overlay_feature  = g_object_get_data(G_OBJECT(overlay_group), 
					   ITEM_FEATURE_DATA); 
      feature_type     = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(subject),
							   ITEM_FEATURE_TYPE));
#endif
      index1 = subject_feature->x1;
      index2 = subject_feature->x2;
      do_overlay = FALSE;

      if(feature_type == ITEM_FEATURE_CHILD)
	{
	  ZMapWindowItemFeature sub_feature_data;

	  sub_feature_data = g_object_get_data(G_OBJECT(subject),
					       ITEM_SUBFEATURE_DATA);
	  switch(sub_feature_data->subpart)
	    {
	    case ZMAPFEATURE_SUBPART_EXON:
	    case ZMAPFEATURE_SUBPART_EXON_CDS:
	      {
		gboolean select_only_matching_frame = FALSE;
		index1 = sub_feature_data->start;
		index2 = sub_feature_data->end;
		do_overlay = TRUE;

		if(overlay_feature->type == ZMAPSTYLE_MODE_PEP_SEQUENCE)
		  {
		    if(sub_feature_data->subpart == ZMAPFEATURE_SUBPART_EXON_CDS)
		      {
			subject_frame = zMapFeatureTranscriptFrame(subject_feature);
			subject_frame = zMapFeatureSubPartFrame(subject_feature, index1);
		      }
		    else
		      subject_frame = zMapFeatureFrame(subject_feature);

		    overlay_frame = zMapFeatureFrame(overlay_feature);

		    if(select_only_matching_frame &&
		       subject_frame != overlay_frame)
		      do_overlay = FALSE;
		    else
		      {
			int cds_index1, cds_index2;
			int end_phase = 0, start_phase = 0;

			/* We can't highlight the phase codons accurately... */
			
			/* I'm fairly sure what we need is zMapFeatureTranslationStartCoord(feature) */

			/* These come back 1 based. */
			zMapFeatureWorld2CDS(subject_feature, index1, index2,
					     &cds_index1, &cds_index2);
			/* Coord space: 1 2 3 | 4 5 6 | 7 8 9 | . . . */
			/* Exon coords: ^         ^ ^       ^         */
			/* Start phase: 1-1%3=0 - - 6-1%3=2 (dashes=missing bases) */

			/* missing start bases, need to skip forward to get codon start */
			start_phase = (cds_index1 - 1) % 3;
			/* missing end bases, need to skip backward to get full codon */
			end_phase   = (cds_index2 % 3);
#ifdef RDS_DONT_INCLUDE			
			printf("indices are %d %d\n", index1, index2);
			printf("phases are %d %d\n", start_phase, end_phase);
#endif
			if(start_phase != 0)
			  index1 += 3 - start_phase;
			if(end_phase != 0)
			  index2 -= end_phase;
#ifdef RDS_DONT_INCLUDE
			printf("indices are %d %d\n", index1, index2);
#endif
		      }
		  }
	      }
	      break;
	    case ZMAPFEATURE_SUBPART_MATCH:
	      {
		index1 = sub_feature_data->start;
		index2 = sub_feature_data->end;
		do_overlay = TRUE;

		if(overlay_feature->type == ZMAPSTYLE_MODE_PEP_SEQUENCE)
		  {
		    subject_frame = zMapFeatureFrame(subject_feature);
		    overlay_frame = zMapFeatureFrame(overlay_feature);
		    if(subject_frame != overlay_frame)
		      do_overlay = FALSE;
		  }
	      }
	      break;
	    default:
	      break;
	    }
	}
      else if(feature_type == ITEM_FEATURE_SIMPLE)
	{
	  switch(subject_feature->type)
	    {
	    case ZMAPSTYLE_MODE_ALIGNMENT:
	      do_overlay = TRUE;
	      if(overlay_feature->type == ZMAPSTYLE_MODE_PEP_SEQUENCE)
		{
		  subject_frame = zMapFeatureFrame(subject_feature);
		  overlay_frame = zMapFeatureFrame(overlay_feature);
		  if(subject_frame != overlay_frame)
		    do_overlay = FALSE;
		}
	      break;
	    case ZMAPSTYLE_MODE_GRAPH:
	    case ZMAPSTYLE_MODE_TEXT:
	    case ZMAPSTYLE_MODE_BASIC:
	      do_overlay = TRUE;
	      break;
	    default:
	      break;
	    }
	}
    }

  if(do_overlay)
    {
      if(index1 > index2)
	ZMAP_SWAP_TYPE(int, index1, index2);

      /* From the text indices, get the bounds of that char */
      first_found = zmapWindowItemTextIndex2Item(overlay_item, index1, &first[0]);
      last_found  = zmapWindowItemTextIndex2Item(overlay_item, index2, &last[0]);
      
      if(first_found && last_found)
	{
	  double minx, maxx;
	  foo_canvas_item_get_bounds(overlay_item, &minx, NULL, &maxx, NULL);

	  points = foo_canvas_points_new(8);

	  zmapWindowItemTextOverlayFromCellBounds(points,
						  &first[0],
						  &last[0],
						  minx, maxx);
	  zmapWindowItemTextOverlayText2Overlay(overlay_item, points);
	  
	  /* set the points */
	  if(points_out)
	    {
	      *points_out = points;
	      /* record such */
	      redraw = TRUE;
	    }
	  else
	    foo_canvas_points_free(points);
	}
    }

  return redraw;
}

/* item_to_char_cell_coords: This is a ZMapWindowOverlaySizeRequestCB callback
 * The overlay code calls this function to request whether to draw an overlay
 * on a region and specifically where to draw it.  returns TRUE to make the 
 * draw and FALSE to not...
 * Logic:  The text context is used to find the coords for the first and last
 * characters of the dna corresponding to the start and end of the feature 
 * attached to the subject given.  TRUE is only returned if at least one of 
 * the start and end are not defaulted, i.e. shown.
 */
static FooCanvasItem *drawFullColumnTextFeature(RunSet run_data,  ZMapFeature feature,
                                                double feature_offset,
                                                double x1, double y1, double x2, double y2,
                                                ZMapFeatureTypeStyle style)
{
  FooCanvasGroup *parent = run_data->container;
  double feature_start, feature_end;
  FooCanvasItem *item;
  FooCanvasZMapAllocateCB allocate_func_cb = NULL;
  FooCanvasZMapFetchTextCB fetch_text_func_cb = NULL;
  ZMapWindowCanvasItem canvas_item;

  feature_start  = feature->x1;
  feature_end    = feature->x2;

  zmapWindowSeq2CanOffset(&feature_start, &feature_end, feature_offset) ;

  canvas_item = zMapWindowCanvasItemCreate(parent, y1, feature, style);

  if(feature->type == ZMAPSTYLE_MODE_RAW_SEQUENCE)
    {
      allocate_func_cb   = canvas_allocate_dna_cb;
      fetch_text_func_cb = canvas_fetch_feature_text_cb;
    }
  else if(feature->type == ZMAPSTYLE_MODE_PEP_SEQUENCE)
    {
      allocate_func_cb   = canvas_allocate_protein_cb;
      fetch_text_func_cb = canvas_fetch_feature_text_cb;
    }

  item = zMapWindowCanvasItemAddInterval(canvas_item, NULL, 
					 feature_start, feature_end,
					 x1, x2);
  foo_canvas_item_set(item,
		      "allocate_func",   allocate_func_cb,
		      "fetch_text_func", fetch_text_func_cb,
		      "callback_data",   feature,
		      NULL);

  return (FooCanvasItem *)canvas_item;
}

static FooCanvasItem *drawSimpleAsTextFeature(RunSet run_data, ZMapFeature feature,
                                              double feature_offset,
                                              double x1, double y1, double x2, double y2,
                                              ZMapFeatureTypeStyle style)
{
  /*  ZMapWindowFToIFactory factory = run_data->factory; */
  FooCanvasGroup        *parent = run_data->container;
  FooCanvasItem *item = NULL, *text_item = NULL;
  ZMapWindowCanvasItem canvas_item;
  double parent_ypos;
  char *text_string = NULL;
  
  text_string = (char *)g_quark_to_string(feature->locus_id);

  zmapWindowSeq2CanOffset(&y1, &y2, feature_offset);

  canvas_item = zMapWindowCanvasItemCreate(parent, y1, feature, style);
  item        = FOO_CANVAS_ITEM(canvas_item);
  parent_ypos = FOO_CANVAS_GROUP(item)->ypos;

  y1 -= parent_ypos;
  y2 -= parent_ypos;

  text_item = zMapWindowCanvasItemAddInterval(canvas_item, NULL, y1, y2, x1, x2);

  foo_canvas_item_set(text_item,
		      "text",       text_string,
		      NULL);

  return item;
}




static FooCanvasItem *drawSimpleGraphFeature(RunSet run_data, ZMapFeature feature,
					     double feature_offset,
					     double x1, double y1, double x2, double y2,
					     ZMapFeatureTypeStyle style)
{
  ZMapWindowFToIFactory factory = run_data->factory ;
  gboolean status ;
  FooCanvasGroup *parent = run_data->container ;
  FooCanvasItem *feature_item ;
  GdkColor *outline = NULL, *foreground = NULL, *background = NULL ;
  guint line_width ;
  double numerator, denominator, dx ;
  double width, max_score, min_score ;

  width = zMapStyleGetWidth(style) ;
  min_score = zMapStyleGetMinScore(style) ;
  max_score = zMapStyleGetMaxScore(style) ;

  line_width = factory->line_width;

  zmapWindowSeq2CanOffset(&y1, &y2, feature_offset) ;	    /* Make sure we cover the whole last base. */

  status = zMapStyleGetColours(style, ZMAPSTYLE_COLOURTARGET_NORMAL, ZMAPSTYLE_COLOURTYPE_NORMAL,
			       &background, &foreground, &outline) ;
  zMapAssert(status) ;

  numerator = feature->score - min_score ;
  denominator = max_score - min_score ;

  if (denominator == 0)				    /* catch div by zero */
    {
      if (numerator < 0)
	dx = 0 ;
      else if (numerator > 1)
	dx = 1 ;
    }
  else
    {
      dx = numerator / denominator ;
      if (dx < 0)
	dx = 0 ;
      if (dx > 1)
	dx = 1 ;
    }
  x1 = 0.0 + (width * zMapStyleBaseline(style)) ;
  x2 = 0.0 + (width * dx) ;

  /* If the baseline is not zero then we can end up with x2 being less than x1 so swop them for
   * drawing, perhaps the drawing code should take care of this. */
  if (x1 > x2)
    {
      double tmp ;

      tmp = x1 ;
      x1 = x2 ;
      x2 = tmp ;
    }

  {
    ZMapWindowCanvasItem canvas_item;

    canvas_item =  zMapWindowCanvasItemCreate(parent, y1, feature, style);

    zMapWindowCanvasItemAddInterval(canvas_item, NULL, 0.0, y2 - y1, x1, x2);

    feature_item = (FooCanvasItem *)canvas_item;
  }

  return feature_item ;
}

/*!
 * \brief Accessory function for drawAlignFeature. 
 * 
 * The Gaps array from acedb and indeed any other source might not be sorted correctly.
 * Indeed I'm not sure it can be when the strands of the reference and the hit don't match.
 * Therefore we need a way to get the coords for the Gap from the Adjacent Matches in the
 * Gaps Array (Incorrectly named!).  The drawAlignFeature was doing this a couple of times
 * and incorrectly working out the actual indel in one if not both of the sequences. 
 * Something I'm guessing it's been doing since it was written.  This function provides a
 * single way to get the coords of the gap for both the reference and hit sequence.
 *
 * @param block_a        One of the Two Adjacent Match Blocks
 * @param block_b        One of the Two Adjacent Match Blocks
 * @param gap_block_out  Pointer to an empty struct to set values in.
 * @param t_indel_gt_1   pointer to boolean to notify indel in reference > 1 bp
 * @param q_indel_gt_1   pointer to boolean to notify indel in hit sequence > 1 bp
 *
 * @return void
 * 
 * N.B. if t_indel_gt_1 or q_indel_gt_1 return false the corresponding gap_block_out coords
 * will be equal to the adjacent match blocks
 *
 * N.B.B. The strand assertion in this function is ifdef'd out see rt
 * 79058.  And hence the same as the reason for ifdef'ing in calling
 * function (line ~ 1110)
 *
 */
/* feel free to optimise, it might need it... Way too many asserts... */
static void GapAlignBlockFromAdjacentBlocks(ZMapAlignBlock block_a, ZMapAlignBlock block_b, 
					    ZMapAlignBlockStruct *gap_span_out,
					    gboolean *t_indel_gt_1,
					    gboolean *q_indel_gt_1)
{
  if(gap_span_out)
    {
      int t_indel, q_indel, t_order = 0, q_order = 0;
      gboolean inconsistency_warning = FALSE, t = FALSE, q = FALSE;

      /* ordering == -1 when a < b, and == +1 when a > b. 
       * If it ends up as 0 they equal and this is BAD */
#ifdef NEEDS_THINKING_ABOUT___      
      /* Sanity first, insanity later */
      zMapAssert(block_a->t1 <= block_a->t2);
      zMapAssert(block_a->q1 <= block_a->q2);
      zMapAssert(block_b->t1 <= block_b->t2);
      zMapAssert(block_b->q1 <= block_b->q2);

      zMapAssert(block_a->q_strand == block_b->q_strand);
      zMapAssert(block_a->t_strand == block_b->t_strand);
#endif
      /* First copy across the strand to the gap span */
      gap_span_out->q_strand = block_a->q_strand;
      gap_span_out->t_strand = block_a->t_strand;


      /* get orders... */
      /* q1 < q2 _always_! so a->q2 < b->q1 means a < b and v.v.*/

      if(block_a->t2 < block_b->t1)
	{
	  t_order = -1;
	  gap_span_out->t1 = block_a->t2;
	  gap_span_out->t2 = block_b->t1;
	}
      else
	{
	  t_order =  1;
	  gap_span_out->t1 = block_b->t2;
	  gap_span_out->t2 = block_a->t1;
	}

      if(block_a->q2 < block_b->q1)
	{
	  q_order = -1;
	  gap_span_out->q1 = block_a->q2;
	  gap_span_out->q2 = block_b->q1;
	}
      else
	{
	  q_order =  1;
	  gap_span_out->q1 = block_b->q2;
	  gap_span_out->q2 = block_a->q1;
	}
#ifdef NEEDS_THINKING_ABOUT___
      zMapAssert(gap_span_out->t1 < gap_span_out->t2);
      zMapAssert(gap_span_out->q1 < gap_span_out->q2);
#endif
      /* What are the indels?  */
      t_indel = gap_span_out->t2 - gap_span_out->t1;
      q_indel = gap_span_out->q2 - gap_span_out->q1;

      /* If we've got an indel then the gap is actually + and - 1 from that.
       * We need to let our caller know what we know though.
       */
      if(t_indel > 1)
	{
	  (gap_span_out->t1)++;
	  (gap_span_out->t2)--;
	  t = TRUE;
	}

      if(q_indel > 1)
	{
	  (gap_span_out->q1)++;
	  (gap_span_out->q2)--;
	  q = TRUE;
	}

      if(t_indel_gt_1)
	*t_indel_gt_1 = t;
      if(q_indel_gt_1)
	*q_indel_gt_1 = q;
#ifdef NEEDS_THINKING_ABOUT___
      zMapAssert(gap_span_out->t1 <= gap_span_out->t2);
      zMapAssert(gap_span_out->q1 <= gap_span_out->q2);
#endif
      if(inconsistency_warning && 
	 q_order != t_order && 
	 gap_span_out->q_strand == gap_span_out->t_strand)
	{
	  zMapLogWarning("ZMapAlignBlocks have inconsistent order and strand\n"
			 "\tBlock A->t1 = %d,  Block B->t1 = %d\n"
			 "\tBlock A->t2 = %d,  Block B->t2 = %d\n"
			 "\tBlock A->t_strand = %s,  Block B->t_strand = %s\n"
			 "\tBlock A->q1 = %d,  Block B->q1 = %d\n"
			 "\tBlock A->q2 = %d,  Block B->q2 = %d\n"
			 "\tBlock A->q_strand = %s,  Block B->q_strand = %s",
			 block_a->t1, block_b->t1,
			 block_a->t2, block_b->t2,
			 zMapFeatureStrand2Str(block_a->t_strand),
			 zMapFeatureStrand2Str(block_b->t_strand),
			 block_a->q1, block_b->q1,
			 block_a->q2, block_b->q2,
			 zMapFeatureStrand2Str(block_a->q_strand), 
			 zMapFeatureStrand2Str(block_b->q_strand)
			 );
	}
    }

  return ;
}

static void GapAlignBlockFromAdjacentBlocks2(ZMapAlignBlock block_a, ZMapAlignBlock block_b, 
					     ZMapAlignBlockStruct *gap_span_out,
					     gboolean *t_indel_gt_1,
					     gboolean *q_indel_gt_1)
{
  if(gap_span_out)
    {
      int t_indel, q_indel, t_order = 0, q_order = 0;
      gboolean t = FALSE, q = FALSE;

      /* ordering == -1 when a < b, and == +1 when a > b. 
       * If it ends up as 0 they equal and this is BAD */

      /* First copy across the strand to the gap span */
      gap_span_out->q_strand = block_a->q_strand;
      gap_span_out->t_strand = block_a->t_strand;


      /* get orders... */
      /* q1 < q2 _always_! so a->q2 < b->q1 means a < b and v.v.*/

      if(block_a->t2 < block_b->t1)
	{
	  t_order = -1;
	  gap_span_out->t1 = block_a->t2;
	  gap_span_out->t2 = block_b->t1;
	}
      else
	{
	  t_order =  1;
	  gap_span_out->t1 = block_b->t2;
	  gap_span_out->t2 = block_a->t1;
	}

      if(block_a->q2 < block_b->q1)
	{
	  q_order = -1;
	  gap_span_out->q1 = block_a->q2;
	  gap_span_out->q2 = block_b->q1;
	}
      else
	{
	  q_order =  1;
	  gap_span_out->q1 = block_b->q2;
	  gap_span_out->q2 = block_a->q1;
	}

      /* What are the indels?  */
      t_indel = gap_span_out->t2 - gap_span_out->t1;
      q_indel = gap_span_out->q2 - gap_span_out->q1;

      /* If we've got an indel then the gap is actually + and - 1 from that.
       * We need to let our caller know what we know though.
       */
      if(t_indel > 1)
	{
	  t = TRUE;
	}

      if(q_indel > 1)
	{
	  q = TRUE;
	}

      (gap_span_out->t1)++;
      (gap_span_out->t2)--;
      (gap_span_out->q1)++;
      (gap_span_out->q2)--;

      if(t_indel_gt_1)
	*t_indel_gt_1 = t;
      if(q_indel_gt_1)
	*q_indel_gt_1 = q;
    }

  return ;
}


static FooCanvasItem *invalidFeature(RunSet run_data,  ZMapFeature feature,
                                     double feature_offset,
                                     double x1, double y1, double x2, double y2,
                                     ZMapFeatureTypeStyle style)
{

  zMapAssertNotReached();

  return NULL;
}

static void null_item_created(FooCanvasItem            *new_item,
                              ZMapWindowItemFeatureType new_item_type,
                              ZMapFeature               full_feature,
                              ZMapWindowItemFeature     sub_feature,
                              double                    new_item_y1,
                              double                    new_item_y2,
                              gpointer                  handler_data)
{
  return ;
}

static gboolean null_top_item_created(FooCanvasItem *top_item,
                                      ZMapFeatureContext context,
                                      ZMapFeatureAlignment align,
                                      ZMapFeatureBlock block,
                                      ZMapFeatureSet set,
                                      ZMapFeature feature,
                                      gpointer handler_data)
{
  return TRUE;
}

static gboolean null_feature_size_request(ZMapFeature feature, 
                                          double *limits_array, 
                                          double *points_array_inout, 
                                          gpointer handler_data)
{
  return FALSE;
}

