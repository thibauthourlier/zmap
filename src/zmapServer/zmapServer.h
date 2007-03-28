/*  File: zmapServer.h
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006: Genome Research Ltd.
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
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk
 *
 * Description: Generalised server interface, hides acedb/das/file
 *              details from caller.
 *
 * HISTORY:
 * Last edited: Mar 27 14:36 2007 (edgrif)
 * Created: Wed Aug  6 15:48:47 2003 (edgrif)
 * CVS info:   $Id: zmapServer.h,v 1.5 2007-03-28 16:05:31 edgrif Exp $
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_SERVER_H
#define ZMAP_SERVER_H

#include <glib.h>
#include <ZMap/zmapFeature.h>
#include <ZMap/zmapUrl.h>


/* Opaque type, represents a connection to a database server. */
typedef struct _ZMapServerStruct *ZMapServer ;

/* Possible responses to a server request. */
typedef enum {ZMAP_SERVERRESPONSE_OK,
	      ZMAP_SERVERRESPONSE_BADREQ, ZMAP_SERVERRESPONSE_UNSUPPORTED,
	      ZMAP_SERVERRESPONSE_REQFAIL, ZMAP_SERVERRESPONSE_TIMEDOUT,
	      ZMAP_SERVERRESPONSE_SERVERDIED} ZMapServerResponseType ;

/* This routine must be called before any other server routines and must only be called once.
 * It is the callers responsibility to make sure this happens.
 * Provide matching Termination routine ???? */
gboolean zMapServerGlobalInit(zMapURL url, void **server_global_data_out) ;
gboolean zMapServerCreateConnection(ZMapServer *server_out, void *server_global_data,
				    zMapURL url,  char *format,
				    int timeout, char *version_str);
ZMapServerResponseType zMapServerOpenConnection(ZMapServer server) ;
ZMapServerResponseType zMapServerGetStyles(ZMapServer server, GData **types_out) ;
ZMapServerResponseType zMapServerStylesHaveMode(ZMapServer server, gboolean *have_mode) ;
ZMapServerResponseType zMapServerGetFeatureSets(ZMapServer server, GList **feature_sets_out) ;
ZMapServerResponseType zMapServerSetContext(ZMapServer server, ZMapFeatureContext feature_context) ;
ZMapFeatureContext zMapServerCopyContext(ZMapServer server) ;
ZMapServerResponseType zMapServerGetFeatures(ZMapServer server, ZMapFeatureContext feature_context) ;
ZMapServerResponseType zMapServerGetSequence(ZMapServer server, ZMapFeatureContext feature_context) ;
char *zMapServerLastErrorMsg(ZMapServer server) ;
ZMapServerResponseType zMapServerCloseConnection(ZMapServer server) ;
gboolean zMapServerFreeConnection(ZMapServer server) ;


#endif /* !ZMAP_SERVER_H */
