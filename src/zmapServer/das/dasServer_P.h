/*  File: dasServer_P.h
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006-2012: Genome Research Ltd.
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
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *         Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk,
 *     Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description:
 *-------------------------------------------------------------------
 */
#ifndef DAS_SERVER_P_H
#define DAS_SERVER_P_H

#include <curl/curl.h>
#include <curl/easy.h>
#include <ZMap/zmapDAS.h>
#include <ZMap/zmapXML.h>
#include <ZMap/zmapFeature.h>


#define ZMAP_DAS_FORMAT_SEGMENT "segment=%s:%d,%d;"
#define ZMAP_URL_FORMAT_UN_PWD "%s://%s:%s@%s:%d/%s/%s"
#define ZMAP_URL_FORMAT        "%s://%s:%d/%s/%s"

/* http://www.biodas.org/documents/spec.html */

typedef enum {
  TAG_DAS_UNKNOWN,
  TAG_DASONE_UNKNOWN,
  TAG_DASONE_ERROR,
  TAG_DASONE_DSN,
  TAG_DASONE_ENTRY_POINTS,
  TAG_DASONE_DNA,
  TAG_DASONE_SEQUENCE,
  TAG_DASONE_TYPES,
  TAG_DASONE_INTERNALFEATURES,  /* THIS ISN'T NEEDED. */
  TAG_DASONE_FEATURES,
  TAG_DASONE_LINK,
  TAG_DASONE_STYLESHEET,
  TAG_DASONE_SEGMENT
} dasXMLTagType;

/* Holds all the state we need to manage the das connection. */
typedef struct _DasServerStruct
{
  char *config_file ;

  GQuark protocol, host, path, user, passwd; /* url bits */
  int   port ;

  CURL *curl_handle ;

  int chunks ;						    /* for debugging at the moment... */

  ZMapXMLParser parser;
  ZMapDAS1Parser das;

  gboolean debug;

  /* error stuff... */
  CURLcode curl_error ;					    /* curl specific error stuff */
  char *curl_errmsg ;
  char *last_errmsg ;					    /* The general das msg stuf, could be
                                                               curl could be my code. */

  GList *hostAbilities;
  GList *dsn_list;
  ZMapDAS1Segment current_segment;

  ZMapDAS1QueryType request_type;
  void *data ;

  GHashTable *feature_cache;

  ZMapFeatureContext req_context ;

  ZMapFeatureContext cur_context ;
  GHashTable *curr_styles ;

  gint zmap_start,zmap_end;

} DasServerStruct, *DasServer ;


#endif /* !DAS_SERVER_P_H */
