/*  File: zmapFeature.c
 *  Author: Rob Clack (rnc@sanger.ac.uk)
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
 * originated by
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk
 *
 * Description: Utility routines for handling features/sets/blocks etc.
 *              
 * Exported functions: See ZMap/zmapFeature.h
 * HISTORY:
 * Last edited: Nov 13 09:42 2008 (edgrif)
 * Created: Tue Nov 2 2004 (rnc)
 * CVS info:   $Id: zmapFeatureUtils.c,v 1.60 2008-12-12 09:07:47 edgrif Exp $
 *-------------------------------------------------------------------
 */


#include <string.h>
#include <unistd.h>

#include <zmapFeature_P.h>
#include <ZMap/zmapPeptide.h>
#include <ZMap/zmapUtils.h>
#include <zmapStyle_P.h>


typedef struct
{
  ZMapMapBlock map;
  int limit_start;
  int limit_end;
  int counter;
} SimpleParent2ChildDataStruct, *SimpleParent2ChildData;

static ZMapFrame feature_frame(ZMapFeature feature, int start_coord);
static int feature_block_frame_offset(ZMapFeature feature);
static void get_feature_list_extent(gpointer list_data, gpointer span_data);

static void translation_set_populate(ZMapFeatureSet feature_set, ZMapFeatureTypeStyle style, char *seq_name, char *seq);


static gint findStyleName(gconstpointer list_data, gconstpointer user_data) ;
static void addTypeQuark(GQuark style_id, gpointer data, gpointer user_data) ;

static void map_parent2child(gpointer exon_data, gpointer user_data);
static int sortGapsByTarget(gconstpointer a, gconstpointer b) ;


/*!
 * Function to do some validity checking on a ZMapFeatureAny struct. Always more you
 * could do but this is better than nothing.
 * 
 * Returns TRUE if the struct is OK, FALSE otherwise.
 * 
 * @param   any_feature    The feature to validate.
 * @return  gboolean       TRUE if feature is valid, FALSE otherwise.
 *  */
gboolean zMapFeatureIsValid(ZMapFeatureAny any_feature)
{
  gboolean result = FALSE ;

  if (any_feature
      && zMapFeatureTypeIsValid(any_feature->struct_type)
      && any_feature->unique_id != ZMAPFEATURE_NULLQUARK
      && any_feature->original_id != ZMAPFEATURE_NULLQUARK)
    {
      switch (any_feature->struct_type)
	{
	case ZMAPFEATURE_STRUCT_CONTEXT:
	case ZMAPFEATURE_STRUCT_ALIGN: 
	case ZMAPFEATURE_STRUCT_BLOCK:
	case ZMAPFEATURE_STRUCT_FEATURESET:
	  result = TRUE ;
	  break ;
	case ZMAPFEATURE_STRUCT_FEATURE:
	  if (any_feature->children == NULL)
	    result = TRUE ;
	  break ;
	default:
	  zMapAssertNotReached() ;
	}
    }

  return result ;
}
#ifdef NOT_YET
static int get_feature_allowed_types(ZMapStyleMode mode)
{
  int allowed = 0;

  switch(mode)
    {
    case ZMAPSTYLE_MODE_RAW_TEXT:
    case ZMAPSTYLE_MODE_RAW_SEQUENCE:
    case ZMAPSTYLE_MODE_PEP_SEQUENCE:
      allowed = (ZMAPFEATURE_TYPE_BASIC | ZMAPFEATURE_TYPE_TEXT);
      break;
    case ZMAPSTYLE_MODE_TRANSCRIPT:
      allowed = (ZMAPFEATURE_TYPE_BASIC | ZMAPFEATURE_TYPE_TRANSCRIPT);
      break;
    case ZMAPSTYLE_MODE_ALIGNMENT:
      allowed = (ZMAPFEATURE_TYPE_BASIC | ZMAPFEATURE_TYPE_ALIGNMENT);
      break;
    case ZMAPSTYLE_MODE_GRAPH:
    case ZMAPSTYLE_MODE_GLYPH:
      allowed = (ZMAPFEATURE_TYPE_BASIC);
      break;
    case ZMAPSTYLE_MODE_BASIC:
      allowed = (ZMAPFEATURE_TYPE_BASIC);
      break;
    case ZMAPSTYLE_MODE_META:
    case ZMAPSTYLE_MODE_INVALID:
    default:
      break;
    }

  return allowed;
}

gboolean zMapFeatureIsDrawable(ZMapFeatureAny any_feature)
{
  gboolean result = FALSE;

  switch(any_feature->struct_type)
    {
    case ZMAPFEATURE_STRUCT_FEATURE:
      {
	ZMapFeature feature;
	int allowed = ZMAPFEATURE_TYPE_INVALID;

	feature = (ZMapFeature)any_feature;

	if(feature->style)
	  allowed = get_feature_allowed_types();

	if(allowed & feature->type)
	  result = TRUE;
      }
      break;
    default:
      break;
    }

  return result;
}
#endif /* NOT_YET */
/*!
 * Function to do some validity checking on a ZMapFeatureAny struct that in addition
 * checks to see if it is of the requested type.
 * 
 * Returns TRUE if the struct is OK, FALSE otherwise.
 * 
 * @param   any_feature    The feature to validate.
 * @param   type           The type that the feature must be.
 * @return  gboolean       TRUE if feature is valid, FALSE otherwise.
 *  */
gboolean zMapFeatureIsValidFull(ZMapFeatureAny any_feature, ZMapFeatureStructType type)
{
  gboolean result = FALSE ;

  if (zMapFeatureIsValid(any_feature) && any_feature->struct_type == type)
    result = TRUE ;

  return result ;
}

gboolean zMapFeatureTypeIsValid(ZMapFeatureStructType group_type)
{
  gboolean result = FALSE ;

  if (group_type >= ZMAPFEATURE_STRUCT_CONTEXT
      && group_type <= ZMAPFEATURE_STRUCT_FEATURE)
    result = TRUE ;

  return result ;
}

gboolean zMapFeatureIsSane(ZMapFeature feature, char **insanity_explained)
{
  gboolean sane = TRUE;
  char *insanity = NULL;

  if(sane)
    {
      if(feature->type <= ZMAPSTYLE_MODE_INVALID ||
         feature->type >  ZMAPSTYLE_MODE_PEP_SEQUENCE)
        {
          insanity = g_strdup_printf("Feature '%s' [%s] has invalid type.",
                                     (char *)g_quark_to_string(feature->original_id),
                                     (char *)g_quark_to_string(feature->unique_id));
          sane = FALSE;
        }
    }

  if(sane)
    {
      if(feature->x1 > feature->x2)
        {
          insanity = g_strdup_printf("Feature '%s' [%s] has start > end.",
                                     (char *)g_quark_to_string(feature->original_id),
                                     (char *)g_quark_to_string(feature->unique_id));
          sane = FALSE;
        }
    }

  if(sane)
    {
      switch(feature->type)
        {
        case ZMAPSTYLE_MODE_TRANSCRIPT:
          {
            GArray *array;
            ZMapSpan span;
            int i = 0;

            if(sane && (array = feature->feature.transcript.exons))
              {
                for(i = 0; sane && i < array->len; i++)
                  {
                    span = &(g_array_index(array, ZMapSpanStruct, i));
                    if(span->x1 > span->x2)
                      {
                        insanity = g_strdup_printf("Exon %d in feature '%s' has start > end.",
                                                   i + 1,
                                                   (char *)g_quark_to_string(feature->original_id));
                        sane = FALSE;
                      }
                  }
              }

            if(sane && (array = feature->feature.transcript.introns))
              {
                for(i = 0; sane && i < array->len; i++)
                  {
                    span = &(g_array_index(array, ZMapSpanStruct, i));
                    if(span->x1 > span->x2)
                      {
                        insanity = g_strdup_printf("Intron %d in feature '%s' has start > end.",
                                                   i + 1,
                                                   (char *)g_quark_to_string(feature->original_id));
                        sane = FALSE;
                      }
                  }
              }
            if(sane && feature->feature.transcript.flags.cds)
              {
                if(feature->feature.transcript.cds_start > feature->feature.transcript.cds_end)
                  {
                    insanity = g_strdup_printf("CDS for feature '%s' has start > end.",
                                               (char *)g_quark_to_string(feature->original_id));
                    sane = FALSE;
                  }
              }
          }
          break;
        case ZMAPSTYLE_MODE_ALIGNMENT:
        case ZMAPSTYLE_MODE_BASIC:
        case ZMAPSTYLE_MODE_RAW_SEQUENCE:
        case ZMAPSTYLE_MODE_PEP_SEQUENCE:
          zMapLogWarning("%s", "This part of zMapFeatureIsSane() needs writing!");
          break;
        default:
          zMapAssertNotReached();
          break;
        }
    }

  if(insanity_explained)
    *insanity_explained = insanity;

  return sane;
}

/* we might get off with insanity. */
gboolean zMapFeatureAnyIsSane(ZMapFeatureAny feature, char **insanity_explained)
{
  gboolean sane = TRUE, insanity_alloc = FALSE;
  char *insanity = NULL;

  if(sane && !zMapFeatureIsValid(feature))
    {
      if(feature->original_id == ZMAPFEATURE_NULLQUARK)
        insanity = "Feature has bad name.";
      else if(feature->unique_id == ZMAPFEATURE_NULLQUARK)
        insanity = "Feature has bad identifier.";
      else
        {
          insanity = g_strdup_printf("Feature '%s' [%s] has bad type.",
                                     (char *)g_quark_to_string(feature->original_id),
                                     (char *)g_quark_to_string(feature->unique_id));
          insanity_alloc = TRUE;
        }
      sane = FALSE;
    }

  if(sane)
    {
      switch(feature->struct_type)
        {
        case ZMAPFEATURE_STRUCT_FEATURE:
          {
            ZMapFeature real_feature = (ZMapFeature)feature;
            sane = zMapFeatureIsSane(real_feature, &insanity);
            insanity_alloc = TRUE;
          }
          break;
        case ZMAPFEATURE_STRUCT_CONTEXT:
          zMapLogWarning("%s", "This part of zMapFeatureAnyIsSane() needs writing!");
          break;
        case ZMAPFEATURE_STRUCT_ALIGN:
        case ZMAPFEATURE_STRUCT_BLOCK:
        case ZMAPFEATURE_STRUCT_FEATURESET:
          /* Nothing to check beyond ZMapFeatureAny basics */
          break;
        default:
          zMapAssertNotReached();
          break;
        }
    }

  if(insanity_explained)
    *insanity_explained = g_strdup(insanity);
  
  if(insanity_alloc && insanity)
    g_free(insanity);

  return sane;
}

/*!
 * Returns the original name of any feature type. The returned string belongs
 * to the feature and must _NOT_ be free'd. This function can never return
 * NULL as all features must have valid names.
 * 
 * @param   any_feature    The feature.
 * @return  char *         The name of the feature.
 *  */
char *zMapFeatureName(ZMapFeatureAny any_feature)
{
  char *feature_name = NULL ;

  zMapAssert(zMapFeatureIsValid(any_feature)) ;

  feature_name = (char *)g_quark_to_string(any_feature->original_id) ;

  return feature_name ;
}


/*!
 * Does a case <i>insensitive</i> comparison of the features name and
 * the supplied name, return TRUE if they are the same.
 * 
 * @param   any_feature    The feature.
 * @param   name           The name to be compared..
 * @return  gboolean       TRUE if the names are the same.
 *  */
gboolean zMapFeatureNameCompare(ZMapFeatureAny any_feature, char *name)
{
  gboolean result = FALSE ;

  zMapAssert(zMapFeatureIsValid(any_feature) && name && *name) ;

  if (g_ascii_strcasecmp(g_quark_to_string(any_feature->original_id), name) == 0)
    result = TRUE ;

  return result ;
}


/*!
 * Function to return the _parent_ group of group_type of the feature any_feature.
 * This is a generalised function to stop all the poking about through the context
 * hierachy that is otherwise required. Note you can only go _UP_ the tree with
 * this function because going down is a one-to-many mapping.
 * 
 * Returns the feature group or NULL if there is no parent group or there is some problem
 * with the arguments like asking for a group at or below the level of any_feature.
 * 
 * @param   any_feature    The feature for which you wish to find the parent group.
 * @param   group_type     The type/level of the parent group you want to find.
 * @return  ZMapFeatureAny The parent group or NULL.
 *  */
ZMapFeatureAny zMapFeatureGetParentGroup(ZMapFeatureAny any_feature, ZMapFeatureStructType group_type)
{
  ZMapFeatureAny result = NULL ;

  zMapAssert(zMapFeatureIsValid(any_feature)
	     && group_type >= ZMAPFEATURE_STRUCT_CONTEXT
	     && group_type <= ZMAPFEATURE_STRUCT_FEATURE) ;

  if (any_feature->struct_type >= group_type)
    {
      ZMapFeatureAny group = any_feature ;

      while (group && group->struct_type > group_type)
	{
	  group = group->parent ;
	}

      result = group ;
    }

  return result ;
}




/* Given a feature name produce the canonicalised name as used that is used in producing
 * unique feature names.
 * 
 * NOTE that the name is canonicalised in place so caller must provide a string for this
 * to be done in.
 *  */
char *zMapFeatureCanonName(char *feature_name)
{
  char *ptr ;
  int len ;

  zMapAssert(feature_name && *feature_name) ;

  len = strlen(feature_name);

  /* lower case the feature name, only the feature part though,
   * numbers don't matter. Here we do as g_strdown does, but in place
   * rather than a g_strdup first. */
  for(ptr = feature_name; ptr <= feature_name + len; ptr++)
    {
      *ptr = g_ascii_tolower(*ptr);
    }

  return feature_name ;
}



/* This function creates a unique id for a feature. This is essential if we are to use the
 * g_datalist package to hold and reference features. Code should _ALWAYS_ use this function
 * to produce these IDs.
 * Caller must g_free() returned string when finished with. */
char *zMapFeatureCreateName(ZMapStyleMode feature_type,
			    char *feature,
			    ZMapStrand strand, int start, int end, int query_start, int query_end)
{
  char *feature_name = NULL, *ptr ;
  int len;

  zMapAssert(feature_type && feature) ;

  /* Get the length of the feature (saving time??) for later */
  len = strlen(feature);

  if (feature_type == ZMAPSTYLE_MODE_ALIGNMENT)
    feature_name = g_strdup_printf("%s_%d.%d_%d.%d", feature,
				   start, end, query_start, query_end) ;
  else
    feature_name = g_strdup_printf("%s_%d.%d", feature, start, end) ;

  /* lower case the feature name, only the feature part though,
   * numbers don't matter. Here we do as g_strdown does, but in place
   * rather than a g_strdup first. */
  for(ptr = feature_name; ptr <= feature_name + len; ptr++)
    {
      *ptr = g_ascii_tolower(*ptr);
    }

  return feature_name ;
}


/* Like zMapFeatureCreateName() but returns a quark representing the feature name. */
GQuark zMapFeatureCreateID(ZMapStyleMode feature_type,
			   char *feature,
			   ZMapStrand strand, int start, int end,
			   int query_start, int query_end)
{
  GQuark feature_id = 0 ;
  char *feature_name ;

  if ((feature_name = zMapFeatureCreateName(feature_type, feature, strand, start, end,
					    query_start, query_end)))
    {
      feature_id = g_quark_from_string(feature_name) ;
      g_free(feature_name) ;
    }

  return feature_id ;
}


GQuark zMapFeatureBlockCreateID(int ref_start, int ref_end, ZMapStrand ref_strand,
                                int non_start, int non_end, ZMapStrand non_strand)
{
  GQuark block_id = 0;
  char *id_base ;

  id_base = g_strdup_printf("%d.%d.%s_%d.%d.%s", 
			    ref_start, ref_end,
			    (ref_strand == ZMAPSTRAND_FORWARD ? "+" : "-"), 
			    non_start, non_end,
			    (non_strand == ZMAPSTRAND_FORWARD ? "+" : "-")) ;
  block_id = g_quark_from_string(id_base) ;
  g_free(id_base) ;

  return block_id;
}

gboolean zMapFeatureBlockDecodeID(GQuark id, 
                                  int *ref_start, int *ref_end, ZMapStrand *ref_strand,
                                  int *non_start, int *non_end, ZMapStrand *non_strand)
{
  gboolean valid = FALSE;
  char *block_id;
  char *format_str = "%d.%d.%1s_%d.%d.%1s";
  char *ref_strand_str, *non_strand_str;
  int fields;
  enum {EXPECTED_FIELDS = 6};

  block_id = (char *)g_quark_to_string(id);

  if((fields = sscanf(block_id, format_str, 
                      ref_start, ref_end, &ref_strand_str[0],
                      non_start, non_end, &non_strand_str[0])) != EXPECTED_FIELDS)
    {
      *ref_start = 0;
      *ref_end   = 0;
      *non_start = 0;
      *non_end   = 0;
    }
  else
    {
      zMapFeatureFormatStrand(ref_strand_str, ref_strand);
      zMapFeatureFormatStrand(non_strand_str, non_strand);
      valid = TRUE;
    }

  return valid;
}

GQuark zMapFeatureSetCreateID(char *set_name)
{
  return zMapStyleCreateID(set_name);
}

/* Free return when finished! */
char *zMapFeatureMakeDNAFeatureName(ZMapFeatureBlock block)
{
  char *dna_name = NULL;

  dna_name = g_strdup_printf("%s (%s)", "DNA", g_quark_to_string(block->original_id));

  return dna_name;
}


void zMapFeatureSortGaps(GArray *gaps)
{
  zMapAssert(gaps) ;

  /* Sort the array of gaps. performance hit? */
  g_array_sort(gaps, sortGapsByTarget);

  return ;
}



/* In zmap we hold coords in the forward orientation always and get strand from the strand
 * member of the feature struct. This function looks at the supplied strand and sets the
 * coords accordingly. */
/* ACTUALLY I REALISE I'M NOT QUITE SURE WHAT I WANT THIS FUNCTION TO DO.... */
gboolean zMapFeatureSetCoords(ZMapStrand strand, int *start, int *end, int *query_start, int *query_end)
{
  gboolean result = FALSE ;

  zMapAssert(start && end && query_start && query_end) ;

  if (strand == ZMAPSTRAND_REVERSE)
    {
      if ((start && end) && start > end)
	{
	  int tmp ;

	  tmp = *start ;
	  *start = *end ;
	  *end = tmp ;

	  if (query_start && query_end)
	    {
	      tmp = *query_start ;
	      *query_start = *query_end ;
	      *query_end = tmp ;
	    }

	  result = TRUE ;
	}
    }
  else
    result = TRUE ;


  return result ;
}


/* Only a featureset or a feature have a style. */
ZMapFeatureTypeStyle zMapFeatureGetStyle(ZMapFeatureAny feature)
{
  ZMapFeatureTypeStyle style ;

  switch(feature->struct_type)
    {
    case ZMAPFEATURE_STRUCT_FEATURE:
      style = ((ZMapFeature)(feature))->style;
      break;
    case ZMAPFEATURE_STRUCT_FEATURESET:
      style = ((ZMapFeatureSet)(feature))->style;
      break;
    case ZMAPFEATURE_STRUCT_BLOCK:
    case ZMAPFEATURE_STRUCT_ALIGN:
    default:
      zMapWarning("%s", "FeatureAny (type == block || align || context) logically has no style.");
      zMapAssertNotReached() ;
      break;
    }

  return style ;
}


char *zMapFeatureSetGetName(ZMapFeatureSet feature_set)
{
  char *set_name ;

  set_name = (char *)g_quark_to_string(feature_set->original_id) ;

  return set_name ;
}



/* Retrieve a style struct for the given style id. */
ZMapFeatureTypeStyle zMapFindStyle(GData *styles, GQuark style_id)
{
  ZMapFeatureTypeStyle style = NULL ;

  style = (ZMapFeatureTypeStyle)g_datalist_id_get_data(&styles, style_id) ;

  return style ;
}


/* Check that a style name exists in a list of styles. */
gboolean zMapStyleNameExists(GList *style_name_list, char *style_name)
{
  gboolean result = FALSE ;
  GList *list ;
  GQuark style_id ;

  style_id = zMapStyleCreateID(style_name) ;

  if ((list = g_list_find_custom(style_name_list, GUINT_TO_POINTER(style_id), findStyleName)))
    result = TRUE ;

  return result ;
}



/* Retrieve a Glist of the names of all the styles... */
GList *zMapStylesGetNames(GData *styles)
{
  GList *quark_list = NULL ;

  zMapAssert(styles) ;

  g_datalist_foreach(&styles, addTypeQuark, (void *)&quark_list) ;

  return quark_list ;
}

/* GFunc() callback function, appends style names to a string, its called for lists
 * of either style name GQuarks or lists of style structs. */
static void addTypeQuark(GQuark style_id, gpointer data, gpointer user_data)
{
  ZMapFeatureTypeStyle style = (ZMapFeatureTypeStyle)data ;
  GList **quarks_out = (GList **)user_data ;
  GList *quark_list = *quarks_out ;

  quark_list = g_list_append(quark_list, GUINT_TO_POINTER(style->unique_id)) ;

  *quarks_out = quark_list ;

  return ;
}




/* For blocks within alignments other than the master alignment, it is not possible to simply
 * use the x1,x2 positions in the feature struct as these are the positions in the original
 * feature. We need to know the coordinates in the master alignment. */
void zMapFeature2MasterCoords(ZMapFeature feature, double *feature_x1, double *feature_x2)
{
  double master_x1 = 0.0, master_x2 = 0.0 ;
  ZMapFeatureBlock block ;
  double feature_offset = 0.0 ;

  zMapAssert(feature->parent && feature->parent->parent) ;

  block = (ZMapFeatureBlock)feature->parent->parent ;

  feature_offset = block->block_to_sequence.t1 - block->block_to_sequence.q1 ;
  
  master_x1 = feature->x1 + feature_offset ;
  master_x2 = feature->x2 + feature_offset ;

  *feature_x1 = master_x1 ;
  *feature_x2 = master_x2 ;

  return ;
}

gboolean zMapFeature3FrameTranslationCreateSet(ZMapFeatureBlock block, ZMapFeatureSet *set_out) 
{
  ZMapFeatureTypeStyle style = NULL;
  ZMapFeatureContext context = NULL;
  ZMapFeatureSet feature_set = NULL;
  GQuark style_id = 0;
  gboolean created = FALSE;

  style_id = zMapStyleCreateID(ZMAP_FIXED_STYLE_3FT_NAME);


  /* No sequence. No Translation _return_ EARLY */
  if(!(block->sequence.length))
    return created;

  if ((context = (ZMapFeatureContext)(zMapFeatureGetParentGroup((ZMapFeatureAny)block, ZMAPFEATURE_STRUCT_CONTEXT))))
    {
      if ((style = zMapFindStyle(context->styles, style_id)) != NULL)
        {
          feature_set = zMapFeatureSetCreate(ZMAP_FIXED_STYLE_3FT_NAME, NULL);
          feature_set->style = style;
          created = TRUE;
        }
    }

  if(set_out && created)
    *set_out = feature_set;
  else
    created = FALSE;

  return created;
}

static void fudge_rev_comp_translation(gpointer key, gpointer value, gpointer user_data)
{
  ZMapFeature feature = (ZMapFeature)value;
  zmapFeatureRevComp(Coord, GPOINTER_TO_INT(user_data), feature->x1, feature->x2);
  return ;
}

void zMapFeature3FrameTranslationRevComp(ZMapFeatureSet feature_set, int origin)
{
  zMapFeature3FrameTranslationPopulate(feature_set);
  
  /* We have to do this as the features get rev comped later, but
   * we're actually recreating the translation in the new orientation
   * so the numbers don't need rev comping then, so we do it here. 
   * I figured doing it twice was less hassle than special case 
   * elsewhere... RDS */
  g_hash_table_foreach(feature_set->features, fudge_rev_comp_translation, GINT_TO_POINTER(origin));

  return ;
}

char *zMapFeature3FrameTranslationFeatureName(ZMapFeatureSet feature_set, ZMapFrame frame)
{
  char *feature_name = NULL, *frame_str;

  switch (frame)
    {
    case ZMAPFRAME_0:
      frame_str = "0" ;
      break ;
    case ZMAPFRAME_1:
      frame_str = "1" ;
      break ;
    case ZMAPFRAME_2:
      frame_str = "2" ;
      break ;
    default:
      frame_str = "." ;
    }

  feature_name = g_strdup_printf("%s_frame-%s", g_quark_to_string(feature_set->unique_id), frame_str);

  return feature_name;
}

void zMapFeature3FrameTranslationPopulate(ZMapFeatureSet feature_set)
{
  ZMapFeatureTypeStyle style;
  ZMapFeatureBlock block;
  char *seq = NULL, *seq_name;

  block = (ZMapFeatureBlock)(zMapFeatureGetParentGroup((ZMapFeatureAny)feature_set, ZMAPFEATURE_STRUCT_BLOCK));

  zMapAssert(block);            /* No block! BIG error! */
  zMapAssert(block->sequence.length); /* No sequence. Why we got a 3ft anyway? Error! */

  style = feature_set->style;
  zMapAssert(style);

  seq_name = (char *)g_quark_to_string(block->original_id);
  
  seq = block->sequence.sequence ;

  translation_set_populate(feature_set, style, seq_name, seq);

  return ;
}

gboolean zMapFeatureGetFeatureListExtent(GList *feature_list, int *start_out, int *end_out)
{
  gboolean done = FALSE;
  ZMapSpanStruct span = {0};
  ZMapFeature feature;

  if(feature_list && (feature = (ZMapFeature)(g_list_nth_data(feature_list, 0))))
    {
      span.x1 = feature->x1;
      span.x2 = feature->x2;

      g_list_foreach(feature_list, get_feature_list_extent, &span);

      if(start_out)
        *start_out = span.x1;
      
      if(end_out)
        *end_out = span.x2;

      done = TRUE;
    }

  return done;
}

void zMapFeatureTranscriptExonForeach(ZMapFeature feature, GFunc function, gpointer user_data)
{
  GArray *exons;
  unsigned index;
  int multiplier = 1, start = 0, end, i;
  gboolean forward = TRUE;

  zMapAssert(feature->type == ZMAPSTYLE_MODE_TRANSCRIPT);

  exons = feature->feature.transcript.exons;

  if(exons->len > 1)
    {
      ZMapSpan first, last;
      first = &(g_array_index(exons, ZMapSpanStruct, 0));
      last  = &(g_array_index(exons, ZMapSpanStruct, exons->len - 1));
      zMapAssert(first && last);

      if(first->x1 > last->x1)
        forward = FALSE;
    }

  if(forward)
    end = exons->len;
  else
    {
      multiplier = -1;
      start = (exons->len * multiplier) + 1;
      end   = 1;
    }

  for(i = start; i < end; i++)
    {
      ZMapSpan exon_span;

      index = i * multiplier;

      exon_span = &(g_array_index(exons, ZMapSpanStruct, index));

      (function)(exon_span, user_data);
    }

  return ;
}

gboolean zMapFeatureWorld2Transcript(ZMapFeature feature, 
				     int w1, int w2,
				     int *t1, int *t2)
{
  gboolean is_transcript = FALSE;

  if(feature->type == ZMAPSTYLE_MODE_TRANSCRIPT)
    {
      if(feature->x1 > w2 || feature->x2 < w1)
	is_transcript = FALSE;
      else
	{
	  SimpleParent2ChildDataStruct parent_data = { NULL };
	  ZMapMapBlockStruct map_data = { 0 };
	  map_data.p1 = w1;
	  map_data.p2 = w2;

	  parent_data.map         = &map_data;
	  parent_data.limit_start = feature->x1;
	  parent_data.limit_end   = feature->x2;
	  parent_data.counter     = 0;

	  zMapFeatureTranscriptExonForeach(feature, map_parent2child, 
					   &parent_data);	  
	  if(t1)
	    *t1 = map_data.c1;
	  if(t2)
	    *t2 = map_data.c2;

	  is_transcript = TRUE;
	}
    }
  else
    is_transcript = FALSE;

  return is_transcript;  
}

gboolean zMapFeatureWorld2CDS(ZMapFeature feature,
			      int exon1, int exon2,
			      int *cds1, int *cds2)
{
  gboolean cds_exon = FALSE;

  if(feature->type == ZMAPSTYLE_MODE_TRANSCRIPT &&
     feature->feature.transcript.flags.cds)
    {
      int cds_start, cds_end;

      cds_start = feature->feature.transcript.cds_start;
      cds_end   = feature->feature.transcript.cds_end;

      if(cds_start > exon2 || cds_end < exon1)
	cds_exon = FALSE;
      else
	{
	  SimpleParent2ChildDataStruct exon_cds_data = { NULL };
	  ZMapMapBlockStruct map_data = { 0 };
	  map_data.p1 = exon1;
	  map_data.p2 = exon2;

	  exon_cds_data.map         = &map_data;
	  exon_cds_data.limit_start = cds_start;
	  exon_cds_data.limit_end   = cds_end;
	  exon_cds_data.counter     = 0;

	  zMapFeatureTranscriptExonForeach(feature, map_parent2child, 
					   &exon_cds_data);	  
	  if(cds1)
	    *cds1 = map_data.c1;
	  if(cds2)
	    *cds2 = map_data.c2;

	  cds_exon = TRUE;
	}
    }
  else
    cds_exon = FALSE;

  return cds_exon;
}

GArray *zMapFeatureWorld2CDSArray(ZMapFeature feature)
{
  GArray *cds_array = NULL, *exon_array;

  if(ZMAPFEATURE_HAS_CDS(feature) && ZMAPFEATURE_HAS_EXONS(feature))
    {
      ZMapSpanStruct span;
      int i;

      cds_array  = g_array_sized_new(FALSE, FALSE, sizeof(ZMapSpanStruct), 128);
      exon_array = feature->feature.transcript.exons;

      for(i = 0; i < exon_array->len; i++)
	{
	  span = g_array_index(exon_array, ZMapSpanStruct, i);
	  zMapFeatureWorld2CDS(feature, span.x1, span.x2, &(span.x1), &(span.x2));
	  cds_array = g_array_append_val(cds_array, span);
	}
    }
  return cds_array;
}

GArray *zMapFeatureWorld2TranscriptArray(ZMapFeature feature)
{
  GArray *t_array = NULL, *exon_array;

  if(ZMAPFEATURE_HAS_EXONS(feature))
    {
      ZMapSpanStruct span;
      int i;

      t_array    = g_array_sized_new(FALSE, FALSE, sizeof(ZMapSpanStruct), 128);
      exon_array = feature->feature.transcript.exons;

      for(i = 0; i < exon_array->len; i++)
	{
	  span = g_array_index(exon_array, ZMapSpanStruct, i);
	  zMapFeatureWorld2Transcript(feature, span.x1, span.x2, &(span.x1), &(span.x2));
	  t_array = g_array_append_val(t_array, span);
	}
    }

  return t_array;
}

ZMapFrame zMapFeatureFrame(ZMapFeature feature)
{
  ZMapFrame frame = ZMAPFRAME_NONE ;

  frame = feature_frame(feature, feature->x1);

  return frame ;
}

ZMapFrame zMapFeatureSubPartFrame(ZMapFeature feature, int coord)
{
  ZMapFrame frame = ZMAPFRAME_NONE ;

  if(coord >= feature->x1 && coord <= feature->x2)
    frame = feature_frame(feature, coord);

  return frame ;
}

/* Returns ZMAPFRAME_NONE if not a transcript */
ZMapFrame zMapFeatureTranscriptFrame(ZMapFeature feature)
{
  ZMapFrame frame = ZMAPFRAME_NONE;
  
  if(feature->type == ZMAPSTYLE_MODE_TRANSCRIPT)
    {
      int start;
      if(feature->feature.transcript.flags.cds)
	start = feature->feature.transcript.cds_start;
      else
	start = feature->x1;

      frame = feature_frame(feature, start);      
    }
  else
    zMapLogWarning("Feature %s is not a Transcript.", g_quark_to_string(feature->unique_id));

  return frame;
}

/* 
 *              Internal routines.
 */

/* Encapulates the rules about which frame a feature is in and what enum to return.
 * 
 * For ZMap this amounts to:
 * 
 * ((coord mod 3) + 1) gives the enum....
 * 
 * Using the offset of 1 is almost certainly wrong for the reverse strand and 
 * possibly wrong for forward.  Need to think about this one ;)
 *  */
static ZMapFrame feature_frame(ZMapFeature feature, int start_coord)
{
  ZMapFrame frame = ZMAPFRAME_NONE;
  int offset, start;

  zMapAssert(zMapFeatureIsValid((ZMapFeatureAny)feature)) ;

  offset = feature_block_frame_offset(feature);
  start  = ((int)(start_coord - offset) % 3) + ZMAPFRAME_0 ;

  switch (start)
    {
    case ZMAPFRAME_0:
      frame = ZMAPFRAME_0 ;
      break ;
    case ZMAPFRAME_1:
      frame = ZMAPFRAME_1 ;
      break ;
    case ZMAPFRAME_2:
      frame = ZMAPFRAME_2 ;
      break ;
    default:
      frame = ZMAPFRAME_NONE ;
      break ;
    }

  return frame;
}

/* Returns the start of the block corrected according to the frame the
 * block is offset by in it's parent */
static int feature_block_frame_offset(ZMapFeature feature)
{
  ZMapFeatureBlock block;
  int frame = 0;
  int offset;

  zMapAssert(feature->parent && feature->parent->parent);

  block  = (ZMapFeatureBlock)(feature->parent->parent);
  offset = block->block_to_sequence.q1; /* start of block */

  /* This should be 0, 1 or 2 */
  if(block->block_to_sequence.t1 >= block->block_to_sequence.q1)
    frame = (block->block_to_sequence.t1 - block->block_to_sequence.q1) % 3;
  else
    frame = (block->block_to_sequence.q1 - block->block_to_sequence.t1) % 3;

  frame += ZMAPFRAME_0;         /* translate to the enums. */

  switch(frame)
    {
    case ZMAPFRAME_0:
      break;
    case ZMAPFRAME_1:
      offset -= 1;
      break;
    case ZMAPFRAME_2:
      offset -= 2;
      break;
    case ZMAPFRAME_NONE:
    default:
      zMapAssertNotReached();
      break;
    }

  return offset;
}

static void translation_set_populate(ZMapFeatureSet feature_set, ZMapFeatureTypeStyle style, char *seq_name, char *seq)
{
  ZMapFeatureBlock block;
  int i, block_position;
  ZMapFeature frame_feature;

  frame_feature = zMapFeatureCreateEmpty();
  zMapFeatureAddStandardData(frame_feature, "_delete_me_", "_delete_me_", 
                             "_delete_me_", "sequence", ZMAPSTYLE_MODE_PEP_SEQUENCE, 
                             style, 1, 10, FALSE, 0.0, ZMAPSTRAND_NONE, ZMAPPHASE_NONE);
  zMapFeatureSetAddFeature(feature_set, frame_feature);

  zMapAssert(feature_set->parent);
  block = (ZMapFeatureBlock)(feature_set->parent);
  block_position = block->block_to_sequence.q1;

  for (i = ZMAPFRAME_0; seq && *seq && i <= ZMAPFRAME_2; i++, seq++, block_position++)
    {
      ZMapPeptide pep;
      ZMapFeature translation;
      char *feature_name; /* Remember to free this */
      GQuark feature_id;
      ZMapFrame curr_frame;
      
      frame_feature->x1 = block_position;

      curr_frame   = zMapFeatureFrame(frame_feature);
      feature_name = zMapFeature3FrameTranslationFeatureName(feature_set, curr_frame);
      feature_id   = g_quark_from_string(feature_name);
      
      pep = zMapPeptideCreateSafely(NULL, NULL, seq, NULL, FALSE);
      
      if((translation  = zMapFeatureSetGetFeatureByID(feature_set, feature_id)))
        g_free(translation->text);
      else
        {
          int x1, x2;

          x1 = frame_feature->x1;
          x2 = x1 + zMapPeptideFullSourceCodonLength(pep) - 1;

          translation = zMapFeatureCreateEmpty();
          
          zMapFeatureAddStandardData(translation, feature_name, feature_name,
                                     seq_name, "sequence",
                                     ZMAPSTYLE_MODE_PEP_SEQUENCE, style,
                                     x1, x2, FALSE, 0.0,
                                     ZMAPSTRAND_NONE, ZMAPPHASE_NONE);

          zMapFeatureSetAddFeature(feature_set, translation);
        }

      translation->text = g_strdup(zMapPeptideSequence(pep));
      
      zMapPeptideDestroy(pep) ;

      g_free(feature_name);
    }

  zMapFeatureSetRemoveFeature(feature_set, frame_feature);
  zMapFeatureDestroy(frame_feature);

  return ;
}

static void get_feature_list_extent(gpointer list_data, gpointer span_data)
{
  ZMapFeature feature = (ZMapFeature)list_data;
  ZMapSpan span = (ZMapSpan)span_data;

  if(feature->x1 < span->x1)
    span->x1 = feature->x1;
  if(feature->x2 > span->x2)
    span->x2 = feature->x2;

  return ;
}


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* GCompareFunc function, called for each member of a list of styles to see if the supplied 
 * style id matches the that in the style. */
static gint findStyle(gconstpointer list_data, gconstpointer user_data)
{
  gint result = -1 ;
  ZMapFeatureTypeStyle style = (ZMapFeatureTypeStyle)list_data ;
  GQuark style_quark =  GPOINTER_TO_UINT(user_data) ;

  if (style_quark == style->unique_id)
    result = 0 ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  printf("Looking for: %s   Found: %s\n",
	 g_quark_to_string(style_quark), g_quark_to_string(style->unique_id)) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  return result ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



/* GCompareFunc function, called for each member of a list of styles ids to see if the supplied 
 * style id matches one in the style list. */
static gint findStyleName(gconstpointer list_data, gconstpointer user_data)
{
  gint result = -1 ;
  GQuark style_list_id = GPOINTER_TO_UINT(list_data) ;
  GQuark style_quark =  GPOINTER_TO_UINT(user_data) ;

  if (style_quark == style_list_id)
    result = 0 ;

  return result ;
}


static void map_parent2child(gpointer exon_data, gpointer user_data)
{
  ZMapSpan exon_span = (ZMapSpan)exon_data;
  SimpleParent2ChildData p2c_data = (SimpleParent2ChildData)user_data;

  if(!(p2c_data->limit_start > exon_span->x2 ||
       p2c_data->limit_end   < exon_span->x1))
    {
      if(exon_span->x1 <= p2c_data->map->p1 &&
	 exon_span->x2 >= p2c_data->map->p1)
	{
	  /* update the c1 coord*/
	  p2c_data->map->c1  = (p2c_data->map->p1 - p2c_data->limit_start + 1);
	  p2c_data->map->c1 += p2c_data->counter;
	}
      
      if(exon_span->x1 <= p2c_data->map->p2 &&
	 exon_span->x2 >= p2c_data->map->p2)
	{
	  /* update the c2 coord */
	  p2c_data->map->c2  = (p2c_data->map->p2 - p2c_data->limit_start + 1);
	  p2c_data->map->c2 += p2c_data->counter;
	}
      
      p2c_data->counter += (exon_span->x2 - exon_span->x1 + 1);
    }

  return ;
}


/* *************************************************************
 * Not entirely sure of the wisdom of this (mainly performance
 * concerns), but everywhere else we have start < end!.  previously
 * loadGaps didn't even fill in the strand or apply the start < end 
 * idiom and gaps array required a test when iterating through 
 * it (the GArray). The GArray will now be ordered as it almost
 * certainly should be to fit with the start < end idiom.  RDS 
 */
static int sortGapsByTarget(gconstpointer a, gconstpointer b)
{
  ZMapAlignBlock alignA = (ZMapAlignBlock)a, 
    alignB = (ZMapAlignBlock)b;
  return (alignA->t1 == alignB->t1 ? 0 : (alignA->t1 > alignB->t1 ? 1 : -1));
}

