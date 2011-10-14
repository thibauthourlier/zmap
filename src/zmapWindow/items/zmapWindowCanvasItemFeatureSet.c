/*  File: zmapWindowCanvasFeaturesetItem.c
 *  Author: malcolm hinsley (mh17@sanger.ac.uk)
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
 * originally written by:
 *
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *     Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description:
 *
 * Exported functions: See XXXXXXXXXXXXX.h
 * HISTORY:
 * Last edited: Jun  3 09:51 2009 (rds)
 * Created: Fri Jan 16 11:20:07 2009 (rds)
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.h>


/*
derived from basic feature and then graph item
a futile wrapper for featureset items till we can remove ZMapCanvasItems
this does nothing excpet interface to existing canvas item/ group code
*/




#include <math.h>
#include <string.h>
#include <zmapWindowCanvasItemFeatureSet_I.h>
#include <zmapWindowCanvasFeatureset_I.h>


static void zmap_window_featureset_item_class_init  (ZMapWindowCanvasFeaturesetItemClass featureset_class);
static void zmap_window_featureset_item_init        (ZMapWindowCanvasFeaturesetItem      group);
static void zmap_window_featureset_item_set_property(GObject               *object,
                                       guint                  param_id,
                                       const GValue          *value,
                                       GParamSpec            *pspec);
static void zmap_window_featureset_item_get_property(GObject               *object,
                                       guint                  param_id,
                                       GValue                *value,
                                       GParamSpec            *pspec);

static void zmap_window_featureset_item_destroy     (GObject *object);

#if 0
static FooCanvasItem *zmap_window_featureset_item_add_interval(ZMapWindowCanvasItem   featureset,
                                               ZMapFeatureSubPartSpan unused,
                                               double top,  double bottom,
                                               double left, double right);
#endif

static void zmap_window_featureset_item_set_colour(ZMapWindowCanvasItem   thing,
						      FooCanvasItem         *interval,
						      ZMapFeature			feature,
						      ZMapFeatureSubPartSpan sub_feature,
						      ZMapStyleColourType    colour_type,
							int colour_flags,
						      GdkColor              *default_fill,
                                          GdkColor              *border);

static gboolean zmap_window_featureset_item_set_feature(FooCanvasItem *item, double x, double y);

static gboolean zmap_window_featureset_item_show_hide(FooCanvasItem *item, gboolean show);

#if 0
static gboolean zmap_window_featureset_item_set_style(FooCanvasItem *item, ZMapFeatureTypeStyle style);
#endif

ZMapWindowCanvasItemClass parent_class_G;

GType zMapWindowCanvasFeaturesetItemGetType(void)
{
  static GType group_type = 0 ;

  if (!group_type)
    {
      static const GTypeInfo group_info = {
      sizeof (zmapWindowCanvasFeaturesetItemClass),
      (GBaseInitFunc) NULL,
      (GBaseFinalizeFunc) NULL,
      (GClassInitFunc) zmap_window_featureset_item_class_init,
      NULL,           /* class_finalize */
      NULL,           /* class_data */
      sizeof (zmapWindowCanvasFeaturesetItem),
      0,              /* n_preallocs */
      (GInstanceInitFunc) zmap_window_featureset_item_init,
      NULL
      };

      group_type = g_type_register_static(zMapWindowCanvasItemGetType(),
                                ZMAP_WINDOW_CANVAS_FEATURESET_ITEM_NAME,
                                &group_info,
                                0) ;
    }

  return group_type;
}



/* get the bounds of the current feature which has been set by the caller */
/* item is a foo canvas group, we have one foo canvas item in the item list */
void zMapWindowCanvasFeaturesetItemGetFeatureBounds(FooCanvasItem *foo, double *rootx1, double *rooty1, double *rootx2, double *rooty2)
{
	FooCanvasGroup *group = FOO_CANVAS_GROUP(foo);
	ZMapWindowFeaturesetItem fi;
	ZMapWindowCanvasItem item = (ZMapWindowCanvasItem) foo;

	zMapAssert(group && group->item_list);

	fi = (ZMapWindowFeaturesetItem) group->item_list->data;
	if(rootx1)
		*rootx1 = fi->dx;
	if(rootx2)
		*rootx2 = fi->dx + fi->width;
	if(rooty1)
		*rooty1 = item->feature->x1;
	if(rooty2)
		*rooty2 = item->feature->x2;
}


static void zmap_window_featureset_item_class_init(ZMapWindowCanvasFeaturesetItemClass featureset_class)
{
  ZMapWindowCanvasItemClass canvas_class ;
  GObjectClass *gobject_class ;

  gobject_class = (GObjectClass *) featureset_class;
  canvas_class  = (ZMapWindowCanvasItemClass) featureset_class;

  parent_class_G = gtk_type_class (zMapWindowCanvasItemGetType());

  gobject_class->set_property = zmap_window_featureset_item_set_property;
  gobject_class->get_property = zmap_window_featureset_item_get_property;

  gobject_class->dispose = zmap_window_featureset_item_destroy;

#if OLD_FEATURE
  canvas_class->add_interval = zmap_window_featureset_item_add_interval;
#endif

  featureset_class->canvas_item_set_colour = canvas_class->set_colour;
  canvas_class->set_colour = zmap_window_featureset_item_set_colour;

  canvas_class->set_feature = zmap_window_featureset_item_set_feature;
//  canvas_class->set_style = zmap_window_featureset_item_set_style;
  canvas_class->showhide = zmap_window_featureset_item_show_hide;

  canvas_class->check_data   = NULL;

  zmapWindowItemStatsInit(&(canvas_class->stats), ZMAP_TYPE_WINDOW_CANVAS_FEATURESET_ITEM) ;

  return ;
}

static void zmap_window_featureset_item_init        (ZMapWindowCanvasFeaturesetItem featureset)
{

  return ;
}



/* record the current feature found by cursor movement which continues moving as we run more code using the feature */
static gboolean zmap_window_featureset_item_set_feature(FooCanvasItem *item, double x, double y)
{
	FooCanvasItem *foo;
	FooCanvasGroup *group;
	ZMapWindowCanvasItem canvas_item = (ZMapWindowCanvasItem) item;

	group = (FooCanvasGroup *) item;
	if(!group->item_list)
		return FALSE;

	foo = group->item_list->data;

	if (g_type_is_a(G_OBJECT_TYPE(foo), ZMAP_TYPE_WINDOW_FEATURESET_ITEM))
	{
		ZMapWindowFeaturesetItem fi = (ZMapWindowFeaturesetItem) foo;

		if(fi->point_feature)
		{
			canvas_item->feature = fi->point_feature;
			return TRUE;
		}
	}
	return FALSE;
}

#if 0
/* redisplay the column using an alternate style */
static gboolean zmap_window_featureset_item_set_style(FooCanvasItem *item, ZMapFeatureTypeStyle style)
{
	FooCanvasItem *foo;
	FooCanvasGroup *group;

	group = (FooCanvasGroup *) item;
	if(!group->item_list)
		return FALSE;

	foo = group->item_list->data;

	if (g_type_is_a(G_OBJECT_TYPE(foo), ZMAP_TYPE_WINDOW_CANVAS_FEATURESET_ITEM))
	{
		ZMapWindowFeaturesetItem di = (ZMapWindowFeaturesetItem) foo;

		zMapWindowFeaturesetDensityItemSetStyle(di,style);
	}
	return FALSE;
}
#endif


static gboolean zmap_window_featureset_item_show_hide(FooCanvasItem *item, gboolean show)
{
	FooCanvasItem *foo;
	FooCanvasGroup *group;

	group = (FooCanvasGroup *) item;
	if(!group->item_list)
		return FALSE;

	foo = group->item_list->data;

	if (g_type_is_a(G_OBJECT_TYPE(foo), ZMAP_TYPE_WINDOW_FEATURESET_ITEM))
	{
		ZMapWindowCanvasItem canvas_item = (ZMapWindowCanvasItem) item;
		/* find the feature struct and set a flag */
#warning this should be a class function
		zmapWindowFeaturesetItemShowHide(foo,canvas_item->feature,show);

	}
	return FALSE;
}




static void zmap_window_featureset_item_set_colour(ZMapWindowCanvasItem   item,
						      FooCanvasItem         *interval,
						      ZMapFeature			feature,
						      ZMapFeatureSubPartSpan sub_feature,
						      ZMapStyleColourType    colour_type,
							int colour_flags,
						      GdkColor              *fill,
                                          GdkColor              *border)
{
	if (g_type_is_a(G_OBJECT_TYPE(interval), ZMAP_TYPE_WINDOW_FEATURESET_ITEM))
	{
#warning this should be a class function
		zmapWindowFeaturesetItemSetColour(item,interval,feature,sub_feature,colour_type,colour_flags,fill,border);
	}
#if OLD_FEATURE
	else
	{
		/* revert to normal canvas item handling */
		ZMapWindowCanvasFeaturesetItemClass class = ZMAP_WINDOW_CANVAS_FEATURESET_ITEM_GET_CLASS(item);
		if(class->canvas_item_set_colour)
			(* class->canvas_item_set_colour) (item,interval,feature,sub_feature,colour_type,colour_flags,fill,border);
	}
#endif
}



#if OLD_FEATURE
static FooCanvasItem *zmap_window_featureset_item_add_interval(ZMapWindowCanvasItem   featureset,
                                               ZMapFeatureSubPartSpan unused,
                                               double top,  double bottom,
                                               double left, double right)
{
  FooCanvasItem *item = NULL;

  ZMapFeature feature;
  feature = featureset->feature;

	/* NOTE: will only be called for non-density mode featuresets */

  item = foo_canvas_item_new(FOO_CANVAS_GROUP(featureset),
                               FOO_TYPE_CANVAS_RECT,
                               "x1", left,  "y1", top,
                               "x2", right, "y2", bottom,
                               NULL);

  return item;
}
#endif

static void zmap_window_featureset_item_set_property(GObject               *object,
                                       guint                  param_id,
                                       const GValue          *value,
                                       GParamSpec            *pspec)

{
  ZMapWindowCanvasItem canvas_item;
  ZMapWindowCanvasFeaturesetItem featureset;

  g_return_if_fail(ZMAP_IS_WINDOW_CANVAS_FEATURESET_ITEM(object));

  featureset = ZMAP_WINDOW_CANVAS_FEATURESET_ITEM(object);
  canvas_item = ZMAP_CANVAS_ITEM(object);

  switch(param_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
      break;
    }

  return ;
}

static void zmap_window_featureset_item_get_property(GObject               *object,
                                       guint                  param_id,
                                       GValue                *value,
                                       GParamSpec            *pspec)
{
  return ;
}


static void zmap_window_featureset_item_destroy     (GObject *object)
{
//	ZMapWindowCanvasFeaturesetItem item = (ZMapWindowCanvasFeaturesetItem) object;
	ZMapWindowCanvasItem canvas_item;
	ZMapWindowCanvasItemClass canvas_item_class ;

  	canvas_item = ZMAP_CANVAS_ITEM(object);
  	canvas_item_class = ZMAP_CANVAS_ITEM_GET_CLASS(canvas_item) ;

	/* canvasitem destroy that calls foo group destroy that calls foo item destroy */
	/* how efficent! */
  	if(GTK_OBJECT_CLASS (parent_class_G)->destroy)
    		(GTK_OBJECT_CLASS (parent_class_G)->destroy)(GTK_OBJECT(object));



  return ;
}

