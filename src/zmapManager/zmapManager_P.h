/*  File: zmapManager_P.h
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
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk and
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk
 *
 * Description: 
 * Exported functions: See XXXXXXXXXXXXX.h
 * HISTORY:
 * Last edited: May 27 10:08 2005 (rds)
 * Created: Thu Jul 24 14:39:06 2003 (edgrif)
 * CVS info:   $Id: zmapManager_P.h,v 1.5 2005-06-01 13:14:10 rds Exp $
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_MANAGER_P_H
#define ZMAP_MANAGER_P_H

#include <glib.h>
#include <ZMap/zmapManager.h>


/* Contains the list of ZMaps maintained by manager, plus a record of the callback stuff
 * registered by the application. */
typedef struct _ZMapManagerStruct
{
  GList *zmap_list ;

  zmapAppCallbackFunc gui_zmap_deleted_func ;
  zmapAppCallbackFunc gui_zmap_set_info_func ;
  void *gui_data ;

} ZMapManagerStruct ;


#endif /* !ZMAP_MANAGER_P_H */
