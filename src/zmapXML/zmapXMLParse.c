/*  File: zmapXMLParse.c
 *  Author: Roy Storey (rds@sanger.ac.uk)
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
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk
 *
 * Description: 
 *
 * Exported functions: See XXXXXXXXXXXXX.h
 * HISTORY:
 * Last edited: Feb 28 15:39 2006 (rds)
 * Created: Fri Aug  5 12:49:50 2005 (rds)
 * CVS info:   $Id: zmapXMLParse.c,v 1.11 2006-03-01 14:10:49 rds Exp $
 *-------------------------------------------------------------------
 */

#include <stdio.h>
#include <expat.h>
#include <glib.h>
#include <ZMap/zmapUtils.h>
#include <zmapXML_P.h>

typedef struct _tagHandlerItemStruct
{
  GQuark id;
  ZMapXMLMarkupObjectHandler handler;
} tagHandlerItemStruct, *tagHandlerItem;

static void setupExpat(zmapXMLParser parser);
static void initElements(GArray *array);
static void initAttributes(GArray *array);
static void freeUpTheQueue(zmapXMLParser parser);
static char *getOffendingXML(zmapXMLParser parser, int context);
static zmapXMLElement parserFetchNewElement(zmapXMLParser parser, 
                                            const XML_Char *name);
static zmapXMLAttribute parserFetchNewAttribute(zmapXMLParser parser,
                                                const XML_Char *name,
                                                const XML_Char *value);
static void pushXMLBase(zmapXMLParser parser, const char *xmlBase);
/* A "user" level ZMapXMLMarkupObjectHandler to handle removing xml bases. */
static gboolean popXMLBase(void *userData, 
                           zmapXMLElement element, 
                           zmapXMLParser parser);

/* Expat handlers we set */
static void start_handler(void *userData, 
                          const char *el, 
                          const char **attr);
static void end_handler(void *userData, 
                        const char *el);
static void character_handler(void *userData,
                              const XML_Char *s, 
                              int len);
static void xmldecl_handler(void* data, 
                            XML_Char const* version, 
                            XML_Char const* encoding,
                            int standalone);
/* End of Expat handlers */

static ZMapXMLMarkupObjectHandler getObjHandler(zmapXMLElement element,
                                                GList *tagHandlerItemList);

/* So what does this do?
 * ---------------------

 * A zmapXMLParser wraps an expat parser so all the settings for the
 * parser are in the one place making it simpler to create, hold on
 * to, destroy an expat parser with the associated start and end
 * handlers in multiple instances.  There is also some extra leverage
 * in the way we parse.  Rather than creating custom objects straight
 * off we create zmapXMLElements which also have an interface.  This
 * obviously uses more RAMs but hopefully the shepherding is of good 
 * quality.  There's also a start and end "Markup Object" handlers to
 * be set which are called from the expat start/end handlers we 
 * register here.  With these it's possible to step through the 
 * zmapXMLElements which will likely just be sub trees of the document.
 * You can of course not register these extra handlers and wait until
 * the complete zmapXMLdocument has been parsed and iterate through the 
 * zmapXMLElements yourself. Recommended ONLY if document is small 
 * (< 100 elements, < 200 attributes).

 */

zmapXMLParser zMapXMLParser_create(void *user_data, gboolean validating, gboolean debug)
{
  zmapXMLParser parser = NULL;

  parser     = (zmapXMLParser)g_new0(zmapXMLParserStruct, 1) ;

  /* Handle arguments */
  validating         = FALSE;           /* IGNORE! We DO NOT validate */
  parser->validating = validating;
  parser->debug      = debug;
  parser->user_data  = user_data;
  /* End effect of arguments */

  if (!(parser->expat = XML_ParserCreate(NULL)))
    zMapLogFatal("%s", "XML_ParserCreate() failed.");

  setupExpat(parser);

  parser->elementStack = g_queue_new() ;
  parser->last_errmsg  = NULL ;

  /* We don't need these generally, but users will */
  parser->startMOHandler = NULL;
  parser->endMOHandler   = NULL;

  parser->xmlbase = g_quark_from_string(ZMAP_XML_BASE_ATTR);

  parser->max_size   = 100;
  parser->elements   = g_array_sized_new(TRUE, TRUE, sizeof(zmapXMLElementStruct),   parser->max_size);
  parser->attributes = g_array_sized_new(TRUE, TRUE, sizeof(zmapXMLAttributeStruct), parser->max_size * 2);

  g_array_set_size(parser->elements, parser->max_size);
  g_array_set_size(parser->attributes, parser->max_size * 2);

  initElements(parser->elements);
  initAttributes(parser->attributes);

  return parser ;
}


zmapXMLElement zMapXMLParser_getRoot(zmapXMLParser parser)
{
  zmapXMLElement root = NULL;
  if(parser->document)
    root = parser->document->root;
  return root;
}

char *zMapXMLParserGetBase(zmapXMLParser parser)
{
  char *base = NULL;
  if(parser->expat != NULL)
    base = (char *)XML_GetBase(parser->expat);
  return base;
}
long zMapXMLParserGetCurrentByteIndex(zmapXMLParser parser)
{
  long index = 0;
  if(parser->expat != NULL)
    index = XML_GetCurrentByteIndex(parser->expat);
  return index;
}
/* If return is non null it needs freeing sometime in the future! */
char *zMapXMLParserGetFullXMLTwig(zmapXMLParser parser, int offset)
{
  char *copy = NULL, *tmp1 = NULL;
  const char *tmp = NULL;
  int current = 0, size = 0, byteCount = 0, twigSize = 0;
  if((tmp = XML_GetInputContext(parser->expat, &current, &size)) != NULL)
    {
      byteCount = XML_GetCurrentByteCount(parser->expat);
      if(byteCount && current && size &&
         (twigSize = (int)((byteCount + current) - offset + 1)) <= size)
        {
          tmp1 = (char *)(tmp + offset);
          copy = g_strndup(tmp1, twigSize);
        }
    }

  return copy;                  /* PLEASE free me later */
}

#define BUFFER_SIZE 200
gboolean zMapXMLParser_parseFile(zmapXMLParser parser,
                                  FILE *file)
{
  int len = 0;
  int done = 0;
  char buffer[BUFFER_SIZE];
  
  while (!done)
    {
      len = fread(buffer, 1, BUFFER_SIZE, file);
      if (ferror(file))
        {
          if (parser->last_errmsg)
            g_free(parser->last_errmsg) ;
          parser->last_errmsg = g_strdup_printf("File Read error");
          return 0;
        }

      done = feof(file);
      if(zMapXMLParser_parseBuffer(parser, (void *)buffer, len) != TRUE)
        return 0;
    }
  
  return 1;
}

gboolean zMapXMLParser_parseBuffer(zmapXMLParser parser, 
                                    void *data, 
                                    int size)
{
  gboolean result = TRUE ;
  int isFinal ;
  isFinal = (size ? 0 : 1);

#ifdef NEW_FEATURE_NOT_ON_ALPHAS
  XML_ParsingStatus status;
  XML_GetParsingStatus(parser->expat, &status);

  if(status.parsing == XML_FINISHED)
    zMapXMLParserReset(parser);
#endif

  if (XML_Parse(parser->expat, (char *)data, size, isFinal) != XML_STATUS_OK)
    {
      char *offend = NULL;
      if (parser->last_errmsg)
        g_free(parser->last_errmsg) ;
      
      offend = getOffendingXML(parser, ZMAP_XML_ERROR_CONTEXT_SIZE);
      parser->last_errmsg = g_strdup_printf("[zmapXMLParse] Parse error line %d column %d: %s\n"
                                            "[zmapXMLParse] XML near error <!-- >>>>%s<<<< -->\n",
                                            XML_GetCurrentLineNumber(parser->expat),
                                            XML_GetCurrentColumnNumber(parser->expat),
                                            XML_ErrorString(XML_GetErrorCode(parser->expat)),
                                            offend) ;
      if(offend)
        g_free(offend);
      result = FALSE ;
    }
  /* Because XML_ParsingStatus XML_GetParsingStatus aren't on alphas! */
  if(isFinal)
    result = zMapXMLParserReset(parser);

  return result ;
}

void zMapXMLParser_setMarkupObjectHandler(zmapXMLParser parser, 
                                          ZMapXMLMarkupObjectHandler start,
                                          ZMapXMLMarkupObjectHandler end)
{
  parser->startMOHandler = start ;
  parser->endMOHandler   = end   ;
  return ;
}

void zMapXMLParser_setMarkupObjectTagHandlers(zmapXMLParser parser,
                                              ZMapXMLObjTagFunctions starts,
                                              ZMapXMLObjTagFunctions ends)
{
  ZMapXMLObjTagFunctions p = NULL;

  zMapAssert(starts || ends);

  p = starts;
  while(p && p->element_name != NULL)
    {
      tagHandlerItem item = g_new0(tagHandlerItemStruct, 1);
      item->id      = g_quark_from_string(p->element_name);
      item->handler = p->handler;
      parser->startTagHandlers = g_list_prepend(parser->startTagHandlers, item);
      p++;
    }

  p = ends;
  while(p && p->element_name != NULL)
    {
      tagHandlerItem item = g_new0(tagHandlerItemStruct, 1);
      item->id      = g_quark_from_string(p->element_name);
      item->handler = p->handler;
      parser->endTagHandlers = g_list_prepend(parser->endTagHandlers, item);
      p++;
    }

  return ;
}

char *zMapXMLParser_lastErrorMsg(zmapXMLParser parser)
{
  return parser->last_errmsg;
}

gboolean zMapXMLParserReset(zmapXMLParser parser)
{
  gboolean result = TRUE;

  printf("\n\n ++++ PARSER is being reset ++++ \n\n");

  /* Clean up our data structures */
  /* Check out this memory leak. 
   * It'd be nice to force the user to clean up the document.
   * a call to zMapXMLDocument_destroy(doc) should go to its root
   * and call zMapXMLElement_destroy(root) which will mean everything
   * is free....
   */
  parser->document = NULL;

  freeUpTheQueue(parser);

  if((result = XML_ParserReset(parser->expat, NULL))) /* encoding as it was created */
    setupExpat(parser);
  else
    parser->last_errmsg = "Failed Resetting the parser.";

  /* The expat parser doesn't clean up the userdata it gets sent.
   * That's us anyway, so that's good, and we don't need to do anything here!
   */

  /* Do so reinitialising for our stuff. */
  parser->elementStack = g_queue_new() ;
  parser->last_errmsg  = NULL ;

  initElements(parser->elements);
  initAttributes(parser->attributes);

  return result;
}

void zMapXMLParser_destroy(zmapXMLParser parser)
{
  XML_ParserFree(parser->expat) ;

  freeUpTheQueue(parser);

  if (parser->last_errmsg)
    g_free(parser->last_errmsg) ;

  parser->user_data = NULL;

  /* We need to free the GArrays here!!! */

  g_free(parser) ;

  return ;
}

/* INTERNAL */
static void setupExpat(zmapXMLParser parser)
{
  if(parser == NULL)
    return;

  XML_SetElementHandler(parser->expat, start_handler, end_handler);
  XML_SetCharacterDataHandler(parser->expat, character_handler);
  XML_SetXmlDeclHandler(parser->expat, xmldecl_handler);
  XML_SetUserData(parser->expat, (void *)parser);

  return ;
}

static void freeUpTheQueue(zmapXMLParser parser)
{
  if (!g_queue_is_empty(parser->elementStack))
    {
      gpointer dummy ;

      while ((dummy = g_queue_pop_head(parser->elementStack)))
	{
	  g_free(dummy) ;
	}
    }
  g_queue_free(parser->elementStack) ;
  parser->elementStack = NULL;
  /* Queue has gone */

  return ;
}

static void pushXMLBase(zmapXMLParser parser, const char *xmlBase)
{
  /* Add to internal list of stuff */
  parser->xmlBaseStack = g_list_append(parser->xmlBaseStack, (char *)xmlBase);

  XML_SetBase(parser->expat, xmlBase);

  return ;
}

/* N.B. element passed in is NULL! */
static gboolean popXMLBase(void *userData, 
                           zmapXMLElement element, 
                           zmapXMLParser parser)
{
  char *previousXMLBase = NULL, *current = NULL;

  /* Remove from list.  Is this correct? */
  if((current = zMapXMLParserGetBase(parser)) != NULL)
    {
      g_list_remove(parser->xmlBaseStack, current);
      
      previousXMLBase = (char *)((g_list_last(parser->xmlBaseStack))->data);
    }

  XML_SetBase(parser->expat, previousXMLBase);

  return FALSE;                   /* Ignored anyway! */
}

/* HANDLERS */

/* Set handlers for start and end tags. Attributes are passed to the start handler
 * as a pointer to a vector of char pointers. Each attribute seen in a start (or empty)
 * tag occupies 2 consecutive places in this vector: the attribute name followed by the
 * attribute value. These pairs are terminated by a null pointer. */

static void start_handler(void *userData, 
                          const char *el, 
                          const char **attr)
{
  zmapXMLParser parser = (zmapXMLParser)userData ;
  zmapXMLElement current_ele = NULL ;
  zmapXMLAttribute attribute = NULL ;
  ZMapXMLMarkupObjectHandler handler = NULL;
  int depth, i;
  gboolean currentHasXMLBase = FALSE;

#ifdef ZMAP_XML_PARSE_USE_DEPTH
  depth = parser->depth;
#else
  depth = (int)g_queue_get_length(parser->elementStack);
#endif

  if (parser->debug)
    {
      for (i = 0; i < depth; i++)
	printf("  ");
      
      printf("<%s ", el) ;

      for (i = 0; attr[i]; i += 2)
        printf(" %s='%s'", attr[i], attr[i + 1]);

      printf(">\n");
    }

#define RDS_NOT_SURE_ON_THIS_YET
  /* Push the current tag on to our stack of tags. */
#ifndef RDS_NOT_SURE_ON_THIS_YET
  current_ele = zmapXMLElement_create(el);
#else
  current_ele = parserFetchNewElement(parser, el);
#endif

  zMapAssert(current_ele);

  for(i = 0; attr[i]; i+=2){
#ifndef RDS_NOT_SURE_ON_THIS_YET
    attribute = zmapXMLAttribute_create(attr[i], attr[i + 1]);
#else
    attribute = parserFetchNewAttribute(parser, attr[i], attr[i+1]);
#endif
    zMapAssert(attribute);

    zmapXMLElement_addAttribute(current_ele, attribute);
    if(attribute->name == parser->xmlbase)
      parser->useXMLBase = currentHasXMLBase = TRUE;
  }

  if(!depth)
    {
      /* In case there wasn't a <?xml version="1.0" ?> */
      if(!(parser->document))
        parser->document = zMapXMLDocument_create("1.0", "UTF-8", -1);
      zMapXMLDocument_setRoot(parser->document, current_ele);
    }
  else
    {
      zmapXMLElement parent_ele;
      parent_ele = g_queue_peek_head(parser->elementStack);
      zmapXMLElement_addChild(parent_ele, current_ele);
    }
    
  g_queue_push_head(parser->elementStack, current_ele);

#ifdef ZMAP_XML_PARSE_USE_DEPTH
  (parser->depth)++;
#endif

  if(parser->useXMLBase && currentHasXMLBase &&
     ((attribute = zMapXMLElement_getAttributeByName(current_ele, ZMAP_XML_BASE_ATTR)) != NULL))
    {
      tagHandlerItem item = g_new0(tagHandlerItemStruct, 1);
      item->id      = current_ele->name;
      item->handler = popXMLBase;
      pushXMLBase(parser, g_quark_to_string(zMapXMLAttribute_getValue(attribute)));
      parser->xmlBaseHandlers = g_list_prepend(parser->xmlBaseHandlers, item);      
    }

  if(((handler = parser->startMOHandler) != NULL) || 
     ((handler = getObjHandler(current_ele, parser->startTagHandlers)) != NULL))
     (*handler)((void *)parser->user_data, current_ele, parser);

  return ;
}


static void end_handler(void *userData, 
                        const char *el)
{
  zmapXMLParser parser = (zmapXMLParser)userData ;
  zmapXMLElement current_ele ;
  gpointer dummy ;
  ZMapXMLMarkupObjectHandler handler = NULL, xmlBaseHandler = NULL;
  int i ;

  /* Now get rid of head element. */
  dummy       = g_queue_pop_head(parser->elementStack) ;
  current_ele = (zmapXMLElement)dummy ;

  if (parser->debug)
    {
#ifdef ZMAP_XML_PARSE_USE_DEPTH
      int depth = parser->depth;
#else
      int depth = g_queue_get_length(parser->elementStack);
#endif
      for (i = 0; i < depth; i++)
	printf("  ") ;

      printf("</%s>\n", el) ;
    }
#ifdef ZMAP_XML_PARSE_USE_DEPTH
  (parser->depth)--;
#endif

  /* Need to get this BEFORE we possibly destroy the current_ele below */
  if(parser->useXMLBase)
    xmlBaseHandler = getObjHandler(current_ele, parser->xmlBaseHandlers);

  if(((handler = parser->endMOHandler) != NULL) || 
     ((handler = getObjHandler(current_ele, parser->endTagHandlers)) != NULL))
    {
      if(((*handler)((void *)parser->user_data, current_ele, parser)) == TRUE)
        {
          /* We can free the current_ele and all its children */
          /* First we need to tell the parent that its child is being freed. */
          if(!(zmapXMLElement_signalParentChildFree(current_ele)))
             printf("Empty'ing document... this might cure memory leak...\n ");
#ifndef RDS_NOT_SURE_ON_THIS_YET          
          zmapXMLElement_free(current_ele); 
#else
          zmapXMLElementMarkDirty(current_ele);
#endif
        }
    }

  /* We need to do this AFTER the endTagHandler as the xml:base 
   * still applies in that handler! */
  if(xmlBaseHandler != NULL)
    (*xmlBaseHandler)((void *)parser->user_data, NULL, parser);

  return ;
}

/* Set a text handler. The string your handler receives is NOT zero terminated.
 * You have to use the length argument to deal with the end of the string.
 * A single block of contiguous text free of markup may still result in a
 * sequence of calls to this handler. In other words, if you're searching for a pattern
 * in the text, it may be split across calls to this handler. 

 * Best to delay anything that wants the whole string to the end handler.
 */
static void character_handler(void *userData, 
                              const XML_Char *s, 
                              int len)
{
  zmapXMLElement content4ele;
  zmapXMLParser parser = (zmapXMLParser)userData ; 

  content4ele = g_queue_peek_head(parser->elementStack);

  zmapXMLElement_addContent(content4ele, s, len);

  return ;
}

static void xmldecl_handler(void* data, 
                            XML_Char const* version, 
                            XML_Char const* encoding,
                            int standalone)
{
    zmapXMLParser parser = (zmapXMLParser)data;

    if (parser == NULL)
      return ;

    if (!(parser->document))
      parser->document = zMapXMLDocument_create(version, encoding, standalone);

    return ;
}

static ZMapXMLMarkupObjectHandler getObjHandler(zmapXMLElement element,
                                                GList *tagHandlerItemList)
{
  ZMapXMLMarkupObjectHandler handler = NULL;
  GList *list = NULL;
  GQuark id = 0;
  id   = element->name;
  list = tagHandlerItemList;

  while(list){
    tagHandlerItem item = (tagHandlerItem)(list->data);
    if(id == item->id){
      handler = item->handler;
      break;
    }
    list = list->next;
  }

  return handler;
}
static zmapXMLAttribute parserFetchNewAttribute(zmapXMLParser parser,
                                                const XML_Char *name,
                                                const XML_Char *value)
{
  zmapXMLAttribute attr = NULL;
  int i = 0, save = -1;

  zMapAssert(parser->attributes);

  for(i = 0; i < parser->attributes->len; i++)
  {
    /* This is hideous. Ed has a written a zMap_g_array_index, but it auto expands  */
    if(((&(g_array_index(parser->attributes, zmapXMLAttributeStruct, i))))->dirty == TRUE)
      { save = i; break; }
  }

  if((save != -1) && (attr = &(g_array_index(parser->attributes, zmapXMLAttributeStruct, save))) != NULL)
    {
      attr->dirty = FALSE;
      attr->name  = g_quark_from_string(g_ascii_strdown((char *)name, -1));
      attr->value = g_quark_from_string((char *)value);
    }

  return attr;
}

static zmapXMLElement parserFetchNewElement(zmapXMLParser parser, 
                                            const XML_Char *name)
{
  zmapXMLElement element = NULL;
  int i = 0, save = -1;

  zMapAssert(parser->elements);

  for(i = 0; i < parser->elements->len; i++)
  {
    /* This is hideous. Ed has a written a zMap_g_array_index, but it auto expands  */
    if(((&(g_array_index(parser->elements, zmapXMLElementStruct, i))))->dirty == TRUE)
      { save = i; break; }
  }

  if((save != -1) && (element = &(g_array_index(parser->elements, zmapXMLElementStruct, save))) != NULL)
    {
      element->dirty    = FALSE;
      element->name     = g_quark_from_string(g_ascii_strdown((char *)name, -1));
      element->contents = g_string_sized_new(100);
    }

  return element;
}

static void initElements(GArray *array)
{
  zmapXMLElement element = NULL;
  int i;

  for(i = 0; i < array->len; i++)
    {
      element = &(g_array_index(array, zmapXMLElementStruct, i));
      element->dirty = TRUE;      
    }

  return ;
}
static void initAttributes(GArray *array)
{
  zmapXMLAttribute attribute = NULL;
  int i;

  for(i = 0; i < array->len; i++)
    {
      attribute = &(g_array_index(array, zmapXMLAttributeStruct, i));
      attribute->dirty = TRUE;      
    }

  return ;
}

/* getOffendingXML - parser, context - Returns a string (free it!)

 * This returns a string from the current input context which will
 * roughly be 1 tag or context bytes either side of the current
 * position.  

 * e.g. getOffendingXML(parser, 10) context = <alpha><beta><gamma>...
 * will return <alpha><beta><gamma> if current position is start beta
 */
/* If return is non null it needs freeing sometime in the future! */
static char *getOffendingXML(zmapXMLParser parser, int context)
{
  char *bad_xml = NULL;
  const char *tmpCntxt = NULL;
  char *tmp = NULL, *end = NULL;
  int curr = 0, size = 0, byte = 0;

  if((tmpCntxt = XML_GetInputContext(parser->expat, &curr, &size)) != NULL)
    {
      byte  = XML_GetCurrentColumnNumber(parser->expat);
      if(byte > 0 && curr && size)
        {
          if(byte + context < size)
            {
              tmp = tmpCntxt + byte + context;
              while(*tmp != '\0' && *tmp != '>'){ tmp++; }
              end = tmp;
            }
          else
            end = tmpCntxt + size - 1;

          if(byte > context)
            {
              tmp = tmpCntxt + byte - context;
              while(*tmp != '<'){ tmp--; }
            }
          else
            tmp = tmpCntxt;

          bad_xml = g_strndup(tmp, end - tmp + 1);
        }
      else                      /* There's not much we can do here */
        bad_xml = g_strndup(tmpCntxt, context);
    }

  return bad_xml;               /* PLEASE free me later */
}

