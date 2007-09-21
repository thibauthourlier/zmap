/*  File: zmapWindowState.c
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2007: Genome Research Ltd.
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
 * Last edited: Sep 21 12:09 2007 (rds)
 * Created: Mon Jun 11 09:49:16 2007 (rds)
 * CVS info:   $Id: zmapWindowState.h,v 1.1 2007-09-21 15:20:08 rds Exp $
 *-------------------------------------------------------------------
 */

#ifndef ZMAP_WINDOW_STATE_H
#define ZMAP_WINDOW_STATE_H

#include <zmapWindow_P.h>


/* create, copy, destroy */
ZMapWindowState zmapWindowStateCreate(void);
ZMapWindowState zmapWindowStateCopy(ZMapWindowState state);
ZMapWindowState zmapWindowStateDestroy(ZMapWindowState state);


/* set/save state information */
gboolean zmapWindowStateSaveMark(ZMapWindowState state, ZMapWindowMark mark);
gboolean zmapWindowStateSaveZoom(ZMapWindowState state, double zoom_factor);


/* restore everything */
void zmapWindowStateRestore(ZMapWindowState state, ZMapWindow window);


#endif /* ZMAP_WINDOW_STATE_H */
