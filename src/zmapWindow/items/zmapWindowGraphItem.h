/*  File: zmapWindowGraphItem.h
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
 *-------------------------------------------------------------------
 */

#ifndef ZMAP_WINDOW_GRAPH_ITEM_H
#define ZMAP_WINDOW_GRAPH_ITEM_H

//#include <ZMap/zmapStyle.h>         // for shape struct and style data


#define ZMAP_WINDOW_GRAPH_ITEM_NAME "ZMapWindowGraphItem"

#define ZMAP_TYPE_WINDOW_GRAPH_ITEM        (zMapWindowGraphItemGetType())

#if GOBJ_CAST
#define ZMAP_WINDOW_GRAPH_ITEM(obj)       ((ZMapWindowGraphItem) obj)
#define ZMAP_WINDOW_GRAPH_ITEM_CONST(obj) ((ZMapWindowGraphItem const) obj)
#else
#define ZMAP_WINDOW_GRAPH_ITEM(obj)	      (G_TYPE_CHECK_INSTANCE_CAST((obj), ZMAP_TYPE_WINDOW_GRAPH_ITEM, zmapWindowGraphItem))
#define ZMAP_WINDOW_GRAPH_ITEM_CONST(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), ZMAP_TYPE_WINDOW_GRAPH_ITEM, zmapWindowGraphItem const))
#endif
#define ZMAP_WINDOW_GRAPH_ITEM_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),  ZMAP_TYPE_WINDOW_GRAPH_ITEM, zmapWindowGraphItemClass))
#define ZMAP_IS_WINDOW_GRAPH_ITEM(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZMAP_TYPE_WINDOW_GRAPH_ITEM))
#define ZMAP_WINDOW_GRAPH_ITEM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj),  ZMAP_TYPE_WINDOW_GRAPH_ITEM, zmapWindowGraphItemClass))





/* Instance */
typedef struct _zmapWindowGraphItemStruct  zmapWindowGraphItem, *ZMapWindowGraphItem ;


/* Class */
typedef struct _zmapWindowGraphItemClassStruct  zmapWindowGraphItemClass, *ZMapWindowGraphItemClass ;


/* Public funcs */
GType zMapWindowGraphItemGetType(void);

int zmapWindowIsGraphItem(FooCanvasItem *foo);



#endif /* ZMAP_WINDOW_GRAPH_ITEM_H */
