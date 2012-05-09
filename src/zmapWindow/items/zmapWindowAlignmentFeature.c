/*  File: zmapWindowAlignmentFeature.c
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2006-2012: Genome Research Ltd.
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
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *     Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description: Implements a custom foocanvas item that represents
 *              an "alignment", i.e. an HSP.
 *
 * Exported functions: See zmapWindowAlignmentFeature.h
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.h>






#include <zmapWindowCanvasItem_I.h>
#include <zmapWindowAlignmentFeature_I.h>


#define DEFAULT_LINE_WIDTH 1


enum
  {
    ALIGNMENT_INTERVAL_0,		/* invalid */
    ALIGNMENT_INTERVAL_TYPE
  };

/* Reduce the number of variables in drawAlignFeature slightly */
typedef struct
{
  gboolean colour_init;
  GdkColor perfect;
  GdkColor colinear;
  GdkColor noncolinear;
  char *perfect_colour;
  char *colinear_colour;
  char *noncolinear_colour;

} AlignColinearityColoursStruct;


static void zmap_window_alignment_feature_class_init  (ZMapWindowAlignmentFeatureClass alignment_class);
static void zmap_window_alignment_feature_init        (ZMapWindowAlignmentFeature      alignment);
static void zmap_window_alignment_feature_set_property(GObject               *object,
							guint                  param_id,
							const GValue          *value,
							GParamSpec            *pspec);
static void zmap_window_alignment_feature_get_property(GObject               *object,
							guint                  param_id,
							GValue                *value,
							GParamSpec            *pspec);
#ifdef ALIGN_REQUIRES_DESTROY
static void zmap_window_alignment_feature_destroy     (GObject *object);
#endif /* ALIGN_REQUIRES_DESTROY */

static void zmap_window_alignment_feature_set_colour(ZMapWindowCanvasItem   alignment,
						     FooCanvasItem         *interval,
						     ZMapFeature			feature,
						     ZMapFeatureSubPartSpan sub_feature_in,
						     ZMapStyleColourType    colour_type,
						     int colour_flags,
						     GdkColor              *default_fill,
                                         GdkColor              *border);
static FooCanvasItem *zmap_window_alignment_feature_add_interval(ZMapWindowCanvasItem   alignment,
								 ZMapFeatureSubPartSpan sub_feature,
								 double top,  double bottom,
								 double left, double right);



static AlignColinearityColoursStruct c_colours_G = {
  FALSE, {0}, {0}, {0},
  /*  ZMAP_WINDOW_MATCH_PERFECT, */ "orange",
  ZMAP_WINDOW_MATCH_COLINEAR,
  ZMAP_WINDOW_MATCH_NOTCOLINEAR
};


GType zMapWindowAlignmentFeatureGetType(void)
{
  static GType group_type = 0;

  if (!group_type) {
    static const GTypeInfo group_info = {
      sizeof (zmapWindowAlignmentFeatureClass),
      (GBaseInitFunc) NULL,
      (GBaseFinalizeFunc) NULL,
      (GClassInitFunc) zmap_window_alignment_feature_class_init,
      NULL,           /* class_finalize */
      NULL,           /* class_data */
      sizeof (zmapWindowAlignmentFeature),
      0,              /* n_preallocs */
      (GInstanceInitFunc) zmap_window_alignment_feature_init,
      NULL
    };

    group_type = g_type_register_static (zMapWindowCanvasItemGetType(),
					 ZMAP_WINDOW_ALIGNMENT_FEATURE_NAME,
					 &group_info,
					 0);
  }

  return group_type;
}


/* object impl */
static void zmap_window_alignment_feature_class_init  (ZMapWindowAlignmentFeatureClass alignment_class)
{
  ZMapWindowCanvasItemClass canvas_class;
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) alignment_class;
  canvas_class  = (ZMapWindowCanvasItemClass)alignment_class;

  gobject_class->set_property = zmap_window_alignment_feature_set_property;
  gobject_class->get_property = zmap_window_alignment_feature_get_property;

  g_object_class_override_property(gobject_class, ALIGNMENT_INTERVAL_TYPE,
				   ZMAP_WINDOW_CANVAS_INTERVAL_TYPE);

  canvas_class->set_colour   = zmap_window_alignment_feature_set_colour;
  canvas_class->add_interval = zmap_window_alignment_feature_add_interval;

  if(c_colours_G.colour_init == FALSE)
    {
      gboolean colinear_colours_from_style = FALSE;

      /* I don't understand this bit....WHAT's IT ALL ABOUT....... */

      if(colinear_colours_from_style)
	{
	  /* These will _never_ parse. Write, if this is required... */
	  c_colours_G.perfect_colour     = "ZMapStyle->perfect_colour";
	  c_colours_G.colinear_colour    = "ZMapStyle->perfect_colour";
	  c_colours_G.noncolinear_colour = "ZMapStyle->perfect_colour";
	}

      gdk_color_parse(c_colours_G.perfect_colour,     &c_colours_G.perfect) ;
      gdk_color_parse(c_colours_G.colinear_colour,    &c_colours_G.colinear) ;
      gdk_color_parse(c_colours_G.noncolinear_colour, &c_colours_G.noncolinear) ;

      c_colours_G.colour_init = TRUE ;
    }

  zmapWindowItemStatsInit(&(canvas_class->stats), ZMAP_TYPE_WINDOW_ALIGNMENT_FEATURE) ;

  return ;
}

static void zmap_window_alignment_feature_init        (ZMapWindowAlignmentFeature alignment)
{

  return ;
}

static void zmap_window_alignment_feature_set_property(GObject               *object,
							guint                  param_id,
							const GValue          *value,
							GParamSpec            *pspec)

{
  ZMapWindowCanvasItem canvas_item;
  ZMapWindowAlignmentFeature alignment;

  g_return_if_fail(ZMAP_IS_WINDOW_ALIGNMENT_FEATURE(object));

  alignment = ZMAP_WINDOW_ALIGNMENT_FEATURE(object);
  canvas_item = ZMAP_CANVAS_ITEM(object);

  switch(param_id)
    {
    case ALIGNMENT_INTERVAL_TYPE:
      {
	gint interval_type = 0;

	if((interval_type = g_value_get_uint(value)) != 0)
	  canvas_item->interval_type = interval_type;
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
      break;
    }

  return ;
}

static void zmap_window_alignment_feature_get_property(GObject               *object,
							guint                  param_id,
							GValue                *value,
							GParamSpec            *pspec)
{
  return ;
}
#ifdef ALIGN_REQUIRES_DESTROY
static void zmap_window_alignment_feature_destroy     (GObject *object)
{

  return ;
}
#endif /* ALIGN_REQUIRES_DESTROY */


static ZMapWindowContainerFeatureSet zmap_window_canvas_item_get_featureset(ZMapWindowCanvasItem canvas_item)
{
  FooCanvasItem *item;
  ZMapWindowContainerGroup container_parent = NULL;
  ZMapWindowCanvasItem canvas_item_parent = NULL;

  zMapLogReturnValIfFail(canvas_item != NULL, NULL);
  zMapLogReturnValIfFail(canvas_item->feature != NULL, NULL);

  canvas_item_parent = zMapWindowCanvasItemIntervalGetTopLevelObject((FooCanvasItem *)canvas_item);

  item = FOO_CANVAS_ITEM(canvas_item_parent);

  if(item->parent && item->parent->parent)
      container_parent = zmapWindowContainerCanvasItemGetContainer(item);

  return (ZMapWindowContainerFeatureSet)container_parent;
}


static void zmap_window_alignment_feature_set_colour(ZMapWindowCanvasItem   alignment,
						     FooCanvasItem         *interval,
						     ZMapFeature            feature,
						     ZMapFeatureSubPartSpan sub_feature,
						     ZMapStyleColourType    colour_type,
						     int colour_flags,
						     GdkColor              *default_fill,
                                         GdkColor              *border)
{
  GdkColor *background, *foreground, *outline, *fill;
  ZMapFeatureTypeStyle style;

  g_return_if_fail(alignment != NULL);
  g_return_if_fail(interval  != NULL);

  fill = outline = foreground = background = NULL;

  style = (ZMAP_CANVAS_ITEM_GET_CLASS(alignment)->get_style)(alignment);

  if((zMapStyleGetColours(style, STYLE_PROP_COLOURS, colour_type,
			  &background, &foreground, &outline)))
    {
      if(colour_type == ZMAPSTYLE_COLOURTYPE_SELECTED)
      {
            if(default_fill)
	            background = default_fill;
            if(border)
                  outline = border;
      }
      else
      {
            ZMapWindow window;
            ZMapWindowContainerFeatureSet fset;

            if(alignment->feature->feature.homol.flags.masked)
            {
                  /* if being chicken first get the container group and test for level is featureset */
                  fset = zmap_window_canvas_item_get_featureset(alignment);
                  /* this colour must be set or else we don't get here */
                  window = zMapWindowContainerFeatureSetGetWindow(fset);
                  zMapWindowGetMaskedColour(window,&background,&outline);
            }
      }

      switch(sub_feature->subpart)
	{
	case ZMAPFEATURE_SUBPART_GAP:
	  {
	    GdkColor *fill;

	    fill = &c_colours_G.perfect;

	    foo_canvas_item_set(interval,
				"fill_color_gdk", outline,
				NULL);
	  }
	  break;
	case ZMAPFEATURE_SUBPART_MATCH:
	default:
	  {
	    if(!outline)
	      g_object_get(G_OBJECT(interval),
			   "outline_color_gdk", &outline,
			   NULL);

	    foo_canvas_item_set(interval,
				"fill_color_gdk",    background,
				"outline_color_gdk", outline,
				NULL);
	  }
	  break;
	}
    }

  return ;
}

static FooCanvasItem *zmap_window_alignment_feature_add_interval(ZMapWindowCanvasItem   alignment,
								 ZMapFeatureSubPartSpan sub_feature_in,
								 double point_a, double point_b,
								 double width_a, double width_b)
{
  ZMapFeatureSubPartSpanStruct local;
  ZMapFeatureSubPartSpan sub_feature = NULL;
  FooCanvasItem *item = NULL;
  ZMapFeatureTypeStyle style;

  g_return_val_if_fail(alignment != NULL, NULL);

  style = (ZMAP_CANVAS_ITEM_GET_CLASS(alignment)->get_style)(alignment);

  if(sub_feature_in)
    sub_feature = sub_feature_in;
  else
    {
      sub_feature = &local;
      sub_feature->subpart = ZMAPFEATURE_SUBPART_GAP | ZMAPFEATURE_SUBPART_MATCH;
    }

  switch(sub_feature->subpart)
    {
    case ZMAPFEATURE_SUBPART_GAP:
      {
	FooCanvasPoints points;
	double line_x_axis;
	double coords[4];
	int line_width = 1;

	line_x_axis = ((width_b - width_a) / 2) + width_a;

	coords[0] = line_x_axis;
	coords[1] = point_a;
	coords[2] = line_x_axis;
	coords[3] = point_b;

	points.coords     = &coords[0];
	points.num_points = 2;
	points.ref_count  = 1;

	item = foo_canvas_item_new_position(FOO_CANVAS_GROUP(alignment),
					    FOO_TYPE_CANVAS_LINE,
					    FOO_CANVAS_GROUP_BOTTOM,
					    "points",         &points,
					    "width_pixels",   line_width,
					    NULL);
	item = zmapWindowLongItemCheckPointFull(item, &points, 0.0, 0.0, 0.0, 0.0);
      }
      break;
    case ZMAPFEATURE_SUBPART_MATCH:
      {
	item = foo_canvas_item_new(FOO_CANVAS_GROUP(alignment),
				   FOO_TYPE_CANVAS_RECT,
				   "x1", width_a, "y1", point_a,
				   "x2", width_b, "y2", point_b,
				   NULL);
	item = zmapWindowLongItemCheckPointFull(item, NULL, width_a, point_a, width_b, point_b);
      }
      break;
    default:
      {
	FooCanvasItem *tmp;
	item = foo_canvas_item_new(FOO_CANVAS_GROUP(alignment),
				   FOO_TYPE_CANVAS_RECT,
				   "x1", width_a, "y1", point_a,
				   "x2", width_b, "y2", point_b,
				   NULL);
	tmp = zmapWindowLongItemCheckPointFull(item, NULL, width_a, point_a, width_b, point_b);
	if(tmp != item)
	  item = tmp;
      }
      break;
    }

  return item;
}



