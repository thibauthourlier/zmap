/*  File: zmapXML_P.h
 *  Author: Roy Storey (rds@sanger.ac.uk)
 *  Copyright (c) 2006-2017: Genome Research Ltd.
 *-------------------------------------------------------------------
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------
 * This file is part of the ZMap genome database package
 * originally written by:
 * 
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *   Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *       Gemma Guest (Sanger Institute, UK) gb10@sanger.ac.uk
 *      Steve Miller (Sanger Institute, UK) sm23@sanger.ac.uk
 *  
 * Description: 
 *
 * Exported functions: See XXXXXXXXXXXXX.h
 *-------------------------------------------------------------------
 */

#ifndef ZMAP_XML_P_H
#define ZMAP_XML_P_H

#include <expat.h>

#include <ZMap/zmapXML.hpp>

#define ZMAP_XML_BASE_ATTR "xml:base"
#define ZMAP_XML_ERROR_CONTEXT_SIZE 10

enum { ZMAPXMLWRITER_FLUSH_COUNT = 16 };

/* For an XML tag holds the element name and attributes, i.e.   <element_name attributes>
 * Used to track which tag we are processing. */

typedef struct _zmapXMLDocumentStruct
{
  GQuark version;
  GQuark encoding;
  int standalone;
  ZMapXMLElement root;
} zmapXMLDocumentStruct, *ZMapXMLDocument ;

typedef struct _zmapXMLAttributeStruct
{
  gboolean dirty;
  GQuark name;
  GQuark value;
} zmapXMLAttributeStruct;

typedef gboolean (*ZMapXMLSuspendedCB)(gpointer user_data, ZMapXMLParser parser);

typedef struct _zmapXMLParserStruct
{
  XML_Parser expat ; 

  GQueue *elementStack ;        /* Stack of zmapXMLElementStructs */
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 4
#define ZMAP_XML_PARSE_USE_DEPTH
  int depth;                    /* Annoyingly g_queue_get_length is a 2.4 addition,
                                 * so it turns out we need to know how deep we are
                                 * using this int. (Grrr.)
                                 */
#endif
  ZMapXMLDocument document;

  const char *last_errmsg, *aborted_msg ;
  void *user_data ;       /* Caller stores any data they need here. */

  GArray *elements, *attributes;
  int max_size_e, max_size_a;   /* Maximum number of elements and attributes */
  int array_point_e;

  ZMapXMLMarkupObjectHandler startMOHandler;
  ZMapXMLMarkupObjectHandler   endMOHandler;

  /* Hopefully these will replace the two above! */
  GList *startTagHandlers, *endTagHandlers;

  ZMapXMLSuspendedCB suspended_cb;

  GList *xmlBaseHandlers, *xmlBaseStack;
  GQuark xmlbase;

  unsigned int debug : 1;
  unsigned int validating : 1;
  unsigned int useXMLBase : 1;
  unsigned int error_free_abort : 1;
} zmapXMLParserStruct ;

typedef struct _ZMapXMLWriterStruct
{
  GString *xml_output;
  GArray  *element_stack;
  gint flush_counter;
  ZMapXMLWriterOutputCallback output_callback;
  gpointer                    output_userdata;
  ZMapXMLWriterErrorCode errorCode;
  char *error_msg;
  gboolean stack_top_has_content;
  gboolean compress_empty_elements ;
} ZMapXMLWriterStruct;


/* DOCUMENTS */
ZMapXMLDocument zMapXMLDocumentCreate(const XML_Char *version, const XML_Char *encoding, int standalone) ;
void zMapXMLDocumentSetRoot(ZMapXMLDocument doc, ZMapXMLElement root) ;
char *zMapXMLDocumentVersion(ZMapXMLDocument doc) ;
char *zMapXMLDocumentEncoding(ZMapXMLDocument doc) ;
gboolean zMapXMLDocumentIsStandalone(ZMapXMLDocument doc) ;
void zMapXMLDocumentReset(ZMapXMLDocument doc) ;
void zMapXMLDocumentDestroy(ZMapXMLDocument doc) ;

/* ATTRIBUTES */
ZMapXMLAttribute zmapXMLAttributeCreate(const XML_Char *name,
                                         const XML_Char *value);
void zmapXMLAttributeMarkDirty(ZMapXMLAttribute attr);
void zmapXMLAttributeFree(ZMapXMLAttribute attr);

/* ELEMENTS */
ZMapXMLElement zmapXMLElementCreate(const XML_Char *name);
gboolean zmapXMLElementAddAttribute(ZMapXMLElement ele, ZMapXMLAttribute attr);
gboolean zmapXMLElementAddAttrPair(ZMapXMLElement ele,
                                    const XML_Char *name,
                                    const XML_Char *value);
void zmapXMLElementAddContent(ZMapXMLElement ele, 
                               const XML_Char *content,
                               int len);
void zmapXMLElementAddChild(ZMapXMLElement parent, ZMapXMLElement child);
gboolean zmapXMLElementSignalParentChildFree(ZMapXMLElement child);
void zmapXMLElementMarkDirty(ZMapXMLElement ele);
void zmapXMLElementMarkAttributesDirty(ZMapXMLElement ele);
void zmapXMLElementFree(ZMapXMLElement ele);
void zmapXMLElementFreeAttrList(ZMapXMLElement ele);

/* PARSER */

#endif /* !ZMAP_XML_P_H */

