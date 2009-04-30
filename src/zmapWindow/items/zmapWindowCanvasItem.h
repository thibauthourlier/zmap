/*  File: zmapWindowCanvasItem.h
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2008: Genome Research Ltd.
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
 * Exported functions: See XXXXXXXXXXXXX.h
 * HISTORY:
 * Last edited: Apr 24 15:18 2009 (rds)
 * Created: Wed Dec  3 08:21:03 2008 (rds)
 * CVS info:   $Id: zmapWindowCanvasItem.h,v 1.2 2009-04-30 08:38:52 rds Exp $
 *-------------------------------------------------------------------
 */

#ifndef ZMAP_WINDOW_CANVAS_ITEM_H
#define ZMAP_WINDOW_CANVAS_ITEM_H

#include <glib-object.h>
#include <libfoocanvas/libfoocanvas.h>
#include <ZMap/zmapFeature.h>
#include <ZMap/zmapStyle.h>
#include <zmapWindow_P.h>
#include <zmapWindowGlyphItem.h>

#define ZMAP_WINDOW_CANVAS_ITEM_NAME 	"ZMapWindowCanvasItem"

/* GParamSpec names */
#define ZMAP_WINDOW_CANVAS_INTERVAL_TYPE "interval-type"


#define ZMAP_TYPE_CANVAS_ITEM           (zMapWindowCanvasItemGetType())
#define ZMAP_CANVAS_ITEM(obj)	        (G_TYPE_CHECK_INSTANCE_CAST((obj), ZMAP_TYPE_CANVAS_ITEM, zmapWindowCanvasItem))
#define ZMAP_CANVAS_ITEM_CONST(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), ZMAP_TYPE_CANVAS_ITEM, zmapWindowCanvasItem const))
#define ZMAP_CANVAS_ITEM_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),  ZMAP_TYPE_CANVAS_ITEM, zmapWindowCanvasItemClass))
#define ZMAP_IS_CANVAS_ITEM(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), ZMAP_TYPE_CANVAS_ITEM))
#define ZMAP_CANVAS_ITEM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj),  ZMAP_TYPE_CANVAS_ITEM, zmapWindowCanvasItemClass))


/* Instance */
typedef struct _zmapWindowCanvasItemStruct  zmapWindowCanvasItem, *ZMapWindowCanvasItem ;


/* Class */
typedef struct _zmapWindowCanvasItemClassStruct  zmapWindowCanvasItemClass, *ZMapWindowCanvasItemClass ;


/* Public funcs */
GType zMapWindowCanvasItemGetType(void);

ZMapWindowCanvasItem zMapWindowCanvasItemCreate(FooCanvasGroup      *parent,
						double               feature_start,
						ZMapFeature          feature_any,
						ZMapFeatureTypeStyle feature_style);

FooCanvasItem *zMapWindowCanvasItemAddInterval(ZMapWindowCanvasItem  canvas_item,
					       ZMapWindowItemFeature sub_feature,
					       double top,  double bottom, 
					       double left, double right);


ZMapFeature zMapWindowCanvasItemGetFeature(ZMapWindowCanvasItem canvas_item);

void zMapWindowCanvasItemCheckSize(ZMapWindowCanvasItem canvas_item);

void zMapWindowCanvasItemSetIntervalType(ZMapWindowCanvasItem canvas_item, guint type);

void zMapWindowCanvasItemGetBumpBounds(ZMapWindowCanvasItem canvas_item,
				       double *x1_out, double *y1_out,
				       double *x2_out, double *y2_out);

gboolean zMapWindowCanvasItemCheckData(ZMapWindowCanvasItem canvas_item, GError **error);

void zMapWindowCanvasItemClear(ZMapWindowCanvasItem canvas_item);
void zMapWindowCanvasItemClearOverlay(ZMapWindowCanvasItem canvas_item);
void zMapWindowCanvasItemClearUnderlay(ZMapWindowCanvasItem canvas_item);


FooCanvasItem *zMapWindowCanvasItemGetInterval(ZMapWindowCanvasItem canvas_item,
					       double x, double y);

ZMapWindowCanvasItem zMapWindowCanvasItemIntervalGetObject(FooCanvasItem *item);

void zMapWindowCanvasItemSetIntervalColours(ZMapWindowCanvasItem canvas_item,
					    ZMapStyleColourType  colour_type,
					    GdkColor            *default_fill_colour);
void zMapWindowCanvasItemUnmark(ZMapWindowCanvasItem canvas_item);
void zMapWindowCanvasItemMark(ZMapWindowCanvasItem canvas_item,
			      GdkColor            *colour,
			      GdkBitmap           *bitmap);

void zMapWindowCanvasItemReparent(FooCanvasItem *item, FooCanvasGroup *new_group);

ZMapWindowCanvasItem zMapWindowCanvasItemDestroy(ZMapWindowCanvasItem canvas_item);


#include <zmapWindowBasicFeature.h>
#include <zmapWindowTranscriptFeature.h>
#include <zmapWindowAlignmentFeature.h>
#include <zmapWindowTextFeature.h>
#include <zmapWindowSequenceFeature.h>

#include <zmapWindowCollectionFeature.h>

#endif /* ZMAP_WINDOW_CANVAS_ITEM_H */
