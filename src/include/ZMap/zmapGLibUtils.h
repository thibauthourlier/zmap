/*  File: zmapGLibUtils.h
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) Sanger Institute, 2005
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
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *
 * Description: Extra Home grown functions that are compatible with
 *              glib but not included with their distribution.
 *
 * HISTORY:
 * Last edited: Sep  6 18:05 2006 (edgrif)
 * Created: Thu Oct 13 15:56:54 2005 (edgrif)
 * CVS info:   $Id: zmapGLibUtils.h,v 1.9 2006-09-15 09:09:54 edgrif Exp $
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_GLIBUTILS_H
#define ZMAP_GLIBUTILS_H

#include <glib.h>




/*! @addtogroup zmapGLibutils
 * @{
 *  */

/*!
 * Represents a quark set. */
typedef struct _ZMapQuarkSetStruct *ZMapQuarkSet ;



typedef enum
  {
    ZMAP_GLIST_FORWARD,
    ZMAP_GLIST_REVERSE
  } ZMapGListDirection;


typedef gboolean (*ZMapGFuncCond)(gpointer data, gpointer user_data) ;



/*! @} end of zmapGLibutils docs. */



char *zMap_g_remove_char(char *string, char ch) ;

void zMap_g_list_foreach_reverse(GList *list, GFunc func, gpointer user_data);
void zMap_g_list_foreach_directional(GList *list, GFunc func, gpointer user_data,
                                     ZMapGListDirection forward);
gboolean zMap_g_list_cond_foreach(GList *list, ZMapGFuncCond func, gpointer user_data) ;
GList *zMap_g_list_move(GList *list, gpointer user_data, gint new_index) ;
void zMap_g_quark_list_print(GList *quark_list) ;



gboolean zMap_g_string_replace(GString *string, char *target, char *source) ;


/* Returns a pointer to an element of the array instead of the element itself. */
#define zMap_g_array_index_ptr(a, t, i)      (&(((t*) (a)->data) [(i)]))



GArray *zMap_g_array_element(GArray           *array,
			     guint             index,
			     gpointer *element
			     );


/* This is our version of the Glib Quark code (see www.gtk.org for more information).
 * Our version allows you to throw away the quark table which is important for us
 * as may have thousands of quarks.
 */
ZMapQuarkSet zMap_g_quark_create_set(guint block_size) ;
GQuark zMap_g_quark_try_string(ZMapQuarkSet quark_set, gchar *string);
GQuark zMap_g_quark_from_string(ZMapQuarkSet quark_set, gchar *string);
gchar *zMap_g_quark_to_string(ZMapQuarkSet quark_set, GQuark quark) ;
void zMap_g_quark_destroy_set(ZMapQuarkSet quark_set) ;


#endif /* !ZMAP_GLIBUTILS_H */
