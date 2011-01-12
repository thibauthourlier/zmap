/*  File: zmapWindowCanvas.h
 *  Author: Roy Storey (rds@sanger.ac.uk)
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
 * Last edited: Dec 15 13:58 2010 (edgrif)
 * Created: Wed Apr 29 14:45:58 2009 (rds)
 * CVS info:   $Id: zmapWindowCanvas.h,v 1.7 2011-01-12 16:56:35 mh17 Exp $
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_WINDOW_CANVAS_H
#define ZMAP_WINDOW_CANVAS_H

#include <libzmapfoocanvas/libfoocanvas.h>


#define ZMAP_WINDOW_CANVAS_NAME "zmapWindowCanvas"

#define ZMAP_TYPE_CANVAS            (zMapWindowCanvasGetType ())
#define ZMAP_CANVAS(obj)            (GTK_CHECK_CAST ((obj),         ZMAP_TYPE_CANVAS, zmapWindowCanvas))
#define ZMAP_CANVAS_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), ZMAP_TYPE_CANVAS, zmapWindowCanvasClass))
#define ZMAP_IS_CANVAS(obj)         (GTK_CHECK_TYPE ((obj),         ZMAP_TYPE_CANVAS))
#define ZMAP_IS_CANVAS_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), ZMAP_TYPE_CANVAS))
#define ZMAP_CANVAS_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj),    ZMAP_TYPE_CANVAS, zmapWindowCanvasClass))

typedef enum
  {
    ZMAP_CANVAS_UPDATE_CROP_REQUIRED   = 1 << 5,
    ZMAP_CANVAS_UPDATE_NEED_REPOSITION = 1 << 6,
  } ZMapWindowCanvasUpdateType ;


typedef struct _zmapWindowCanvasStruct zmapWindowCanvas, *ZMapWindowCanvas;
typedef struct _zmapWindowCanvasClassStruct zmapWindowCanvasClass, *ZMapWindowCanvasClass;


GType      zMapWindowCanvasGetType(void) ;
GtkWidget *zMapWindowCanvasNew          (double max_zoom) ;
gboolean   zMapWindowCanvasBusy         (ZMapWindowCanvas canvas) ;
gboolean   zMapWindowCanvasUnBusy       (ZMapWindowCanvas canvas) ;
void       zMapWindowCanvasLongItemCheck(ZMapWindowCanvas canvas,
					 FooCanvasItem   *item,
					 double start, double end) ;
void zMapWindowCanvasLongItemRemove(ZMapWindowCanvas canvas, FooCanvasItem *item_to_remove);

gboolean zMapWindowCanvasUnBusy(ZMapWindowCanvas canvas);
gboolean zMapWindowCanvasBusy(ZMapWindowCanvas canvas);

#endif /* ZMAP_WINDOW_CANVAS_H */
