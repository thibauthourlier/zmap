/*  File: zmapWindowContainerUtils_P.h
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2009: Genome Research Ltd.
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
 * Last edited: Jun 12 09:30 2009 (rds)
 * Created: Wed May 20 08:33:22 2009 (rds)
 * CVS info:   $Id: zmapWindowContainerUtils_P.h,v 1.3 2009-06-17 09:46:16 rds Exp $
 *-------------------------------------------------------------------
 */

#ifndef ZMAP_WINDOW_CONTAINER_UTILS_P_H
#define ZMAP_WINDOW_CONTAINER_UTILS_P_H



/* enums, macros etc... */
#define ZMAP_PARAM_STATIC    (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)
#define ZMAP_PARAM_STATIC_RW (ZMAP_PARAM_STATIC   | G_PARAM_READWRITE)
#define ZMAP_PARAM_STATIC_RO (ZMAP_PARAM_STATIC   | G_PARAM_READABLE)
#define ZMAP_PARAM_STATIC_WO (ZMAP_PARAM_STATIC   | G_PARAM_WRITABLE)

#define CONTAINER_DATA     "container_data"
#define CONTAINER_TYPE_KEY "container_type"

#ifdef RDS_DONT_INCLUDE
/* This is _very_ annoying and will be removed ASAP! */
#ifndef ZMAP_WINDOW_P_H

#ifndef ITEM_FEATURE_DATA
#define ITEM_FEATURE_DATA  "item_feature_data"
#endif /* !ITEM_FEATURE_DATA */

#endif /* !ZMAP_WINDOW_P_H */
#endif /* RDS_DONT_INCLUDE */

enum
  {
    CONTAINER_GROUP_INVALID,
    CONTAINER_GROUP_ROOT,
    CONTAINER_GROUP_PARENT,
    CONTAINER_GROUP_OVERLAYS,
    CONTAINER_GROUP_FEATURES,
    CONTAINER_GROUP_BACKGROUND,
    CONTAINER_GROUP_UNDERLAYS,
    CONTAINER_GROUP_COUNT
  };


#endif /* !ZMAP_WINDOW_CONTAINER_UTILS_P_H */
