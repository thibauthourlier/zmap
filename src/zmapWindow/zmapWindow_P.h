/*  File: zmapWindow_P.h
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) Sanger Institute, 2003
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
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk,
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk and
 *	Simon Kelley (Sanger Institute, UK) srk@sanger.ac.uk
 *
 * Description: 
 * Exported functions: See XXXXXXXXXXXXX.h
 * HISTORY:
 * Last edited: Jun 25 12:11 2004 (edgrif)
 * Created: Fri Aug  1 16:45:58 2003 (edgrif)
 * CVS info:   $Id: zmapWindow_P.h,v 1.7 2004-06-25 13:42:26 edgrif Exp $
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_WINDOW_P_H
#define ZMAP_WINDOW_P_H

#include <gtk/gtk.h>
#include <ZMap/zmapWindow.h>


/* Test scaffoling */
#include <ZMap/zmapFeature.h>


typedef struct _ZMapWindowStruct
{
  gchar *sequence ;

  GtkWidget *parent_widget ;
  GtkWidget *toplevel ;
  GtkWidget *text ;

  GdkAtom zmap_atom ;

  zmapVoidIntCallbackFunc app_routine ;
  void *app_data ;

} ZMapWindowStruct ;


typedef struct
{
  ZMapWindow window ;
  void *data ;						    /* void for now, union later ?? */
} zmapWindowDataStruct, *zmapWindowData ;





/* Used in our event communication.... */
#define ZMAP_ATOM  "ZMap_Atom"


GtkWidget *zmapWindowMakeMenuBar(ZMapWindow window) ;
GtkWidget *zmapWindowMakeButtons(ZMapWindow window) ;
GtkWidget *zmapWindowMakeFrame(ZMapWindow window) ;




/* TEST SCAFFOLDING............... */
ZMapFeatureContext testGetGFF(void) ;



#endif /* !ZMAP_WINDOW_P_H */
