/*  File: zmapWindowCanvasBasic.c
 *  Author: malcolm hinsley (mh17@sanger.ac.uk)
 *  Copyright (c) 2006-2010: Genome Research Ltd.
 *-------------------------------------------------------------------
 * ZMap is free software; you can refeaturesetstribute it and/or
 * mofeaturesetfy it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is featuresetstributed in the hope that it will be useful,
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
 * implements callback functions for FeaturesetItem basic features
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.h>





#include <math.h>
#include <string.h>
#include <ZMap/zmapFeature.h>
#include <zmapWindowCanvasFeatureset_I.h>
#include <zmapWindowCanvasBasic_I.h>


static void zMapWindowCanvasBasicPaintFeature(ZMapWindowFeaturesetItem featureset, ZMapWindowCanvasFeature feature, GdkDrawable *drawable)
{
	static int i = 0;
	gulong pixel;
	GdkColor c;
	FooCanvasItem *item = (FooCanvasItem *) featureset;
      int cx1, cy1, cx2, cy2;

	double x1,x2;

	if(i++ < 4)
		printf("basic paint feature! %s\n",g_quark_to_string(feature->feature->unique_id));

	/* draw a box */

	/* colours are not defined for the CanvasFeatureSet
	 * as we can have several styles in a column
	 * but they are cached by the calling function
	 */

#warning  basic features don't do score ?? if they do we need to handle it
	feature->width = x2 = featureset->width;
	x1 = featureset->dx + featureset->width / 2 - x2 / 2;
	x2 += x1;

		/* get item canvas coords, following example from FOO_CANVAS_RE (used by graph items) */
	foo_canvas_w2c (item->canvas, x1 + featureset->dx, feature->y1 - featureset->start + featureset->dy, &cx1, &cy1);
	foo_canvas_w2c (item->canvas, x2 + featureset->dx, feature->y2 - featureset->start + featureset->dy + 1, &cx2, &cy2);
      						/* + 1 to draw to the end of the last base */

		/* we have pre-calculated pixel colours */
	if (zMapWindowCanvasFeaturesetGetFill(featureset, feature, &pixel))
	{
		c.pixel = pixel;
		gdk_gc_set_foreground (featureset->gc, &c);
		gdk_draw_rectangle (drawable, featureset->gc,TRUE, cx1, cy1, cx2 - cx1, cy2 - cy1);
	}
	if (zMapWindowCanvasFeaturesetGetOutline(featureset, feature, &pixel))
	{
		c.pixel = pixel;
		gdk_gc_set_foreground (featureset->gc, &c);
		gdk_draw_rectangle (drawable, featureset->gc,FALSE, cx1, cy1, cx2 - cx1 - 1, cy2 - cy1 - 1);
	}
}



void zMapWindowCanvasBasicInit(void)
{
	gpointer funcs[FUNC_N_FUNC] = { NULL };

	funcs[FUNC_PAINT] = zMapWindowCanvasBasicPaintFeature;

	zMapWindowCanvasFeatureSetSetFuncs(FEATURE_BASIC,funcs);
}

