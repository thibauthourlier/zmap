/*  File: zmapGFF2parser.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) Sanger Institute, 2004
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
 * Description: parser for GFF version 2 format.
 *              
 * Exported functions: See ZMap/zmapGFF.h
 * HISTORY:
 * Last edited: Jul 19 10:23 2004 (edgrif)
 * Created: Fri May 28 14:25:12 2004 (edgrif)
 * CVS info:   $Id: zmapGFF2parser.c,v 1.10 2004-07-19 09:24:42 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <glib.h>
#include <ZMap/zmapFeature.h>
#include <zmapGFF_P.h>


static gboolean parseHeaderLine(ZMapGFFParser parser, char *line) ;
static gboolean parseBodyLine(ZMapGFFParser parser, char *line) ;

static gboolean makeNewFeature(ZMapGFFParser parser, char *sequence, char *source,
			       ZMapFeatureType feature_type,
			       int start, int end, double score, ZMapStrand strand,
			       ZMapPhase phase, char *attributes) ;
static gboolean addDataToFeature(ZMapGFFParser parser, ZMapFeature feature,
				 char *name,
				 char *sequence, char *source, ZMapFeatureType feature_type,
				 int start, int end, double score, ZMapStrand strand,
				 ZMapPhase phase, char *attributes) ;

static char *getFeatureName(char *attributes) ;
static gboolean getHomolAttrs(char *attributes, ZMapHomolType *homol_type_out,
			      int *start_out, int *end_out) ;

static gboolean formatType(gboolean SO_compliant, gboolean default_to_basic,
			   char *feature_type, ZMapFeatureType *type_out) ;
static gboolean formatScore(char *score_str, gdouble *score_out) ;
static gboolean formatStrand(char *strand_str, ZMapStrand *strand_out) ;
static gboolean formatPhase(char *phase_str, ZMapPhase *phase_out) ;


static void getFeatureArray(GQuark key_id, gpointer data, gpointer user_data) ;
void destroyFeatureArray(gpointer data) ;


/* If parse_only is TRUE the parser will parse the GFF and report errors but will
 * _not_ create any features. This means the parser can be tested/used on huge datasets
 * without having to have huge amounts of memory to hold the feature structs.
 * You can only set parse_only when you create the parser, it cannot be set later. */
ZMapGFFParser zMapGFFCreateParser(gboolean parse_only)
{
  ZMapGFFParser parser ;

  parser = g_new0(ZMapGFFParserStruct, 1) ;

  parser->state = ZMAPGFF_PARSE_HEADER ;
  parser->error = NULL ;
  parser->error_domain = g_quark_from_string(ZMAP_GFF_ERROR) ;
  parser->stop_on_error = FALSE ;
  parser->parse_only = parse_only ;

  parser->line_count = 0 ;
  parser->SO_compliant = FALSE ;
  parser->default_to_basic = FALSE ;


  parser->done_version = FALSE ;
  parser->gff_version = -1 ;

  parser->done_source = FALSE ;
  parser->source_name = parser->source_version = NULL ;

  parser->done_sequence_region = FALSE ;
  parser->sequence_name = NULL ;
  parser->features_start = parser->features_end = 0 ;

  if (!parser->parse_only)
    {
      g_datalist_init(&(parser->feature_sets)) ;
      parser->free_on_destroy  = TRUE ;
    }
  else
    {
      parser->feature_sets =  NULL ;
      parser->free_on_destroy  = FALSE ;
    }

  return parser ;
}




/* This function expects a null-terminated C string that contains a complete GFF line
 * (comment or non-comment line), the function expects the caller to already have removed the
 * newline char from the end of the GFF line.
 */

/* ISSUE: need to decide on rules for comments, can they be embedded within other gff lines, are
 * the header comments compulsory ? etc. etc. 
 * 
 * Current code assumes that the header block will be a contiguous set of header lines
 * at the top of the file and that the first non-header line marks the beginning
 * of the GFF data. If this is not true then its an error.
 * 
 * Currently once there has been an error then no more parsing will take place, could change
 * this behaviour so that this is not true for body lines and perhaps for some header lines
 * but note that we do need to know the GFF version....
 *  */

gboolean zMapGFFParseLine(ZMapGFFParser parser, char *line)
{
  gboolean result = FALSE ;

  parser->line_count++ ;

  /* Look for the header information. */
  if (parser->state == ZMAPGFF_PARSE_HEADER)
    {
      if (!(result = parseHeaderLine(parser, line)))
	{
	  /* returns FALSE for two reasons: there was a parse error, or the header section has
	   * finished, for the latter we need to cancel the error. */
	  if (parser->error && parser->stop_on_error)
	    {
	      result = FALSE ;
	      parser->state = ZMAPGFF_PARSE_ERROR ;
	    }
	  else
	    {
	      result = TRUE ;

	      /* If we found all the header parts move on to the body. */
	      if (parser->done_version && parser->done_source && parser->done_sequence_region)
		parser->state = ZMAPGFF_PARSE_BODY ;
	    }
	}
    }

  /* Note can only be in parse body state if header correctly parsed. */
  if (parser->state == ZMAPGFF_PARSE_BODY)
    {
      /* Skip over comment lines, this is a CRUDE test, probably need something more subtle. */
      if (*(line) == '#')
	result = TRUE ;
      else
	{
	  /* THIS NEEDS WORK, ONCE I'VE SORTED OUT ALL THE PARSING STUFF...... */
	  if (!(result = parseBodyLine(parser, line)))
	    {
	      if (parser->error && parser->stop_on_error)
		{
		  result = FALSE ;
		  parser->state = ZMAPGFF_PARSE_ERROR ;
		}
	    }
	}
    }

  return result ;
}



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* this is a version that returns a GArray of features arrays, it may be useful in the
 * future but having thought about it I think its better to return the GData of
 * feature arrays. */
GArray *zmapGFFGetFeatures(ZMapGFFParser parser)
{
  GArray *features = NULL ;

  if (!parser->parse_only)
    {
      features = g_array_sized_new(FALSE, FALSE, sizeof(ZMapFeatureSetStruct), 30) ;

      g_datalist_foreach(&(parser->feature_sets), getFeatureArray, features) ;
    }

  return features ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


ZMapFeatureContext zmapGFFGetFeatures(ZMapGFFParser parser)
{
  ZMapFeatureContext feature_context = NULL ;

  if (!parser->parse_only)
    {
      feature_context = g_new0(ZMapFeatureContextStruct, 1) ;

      feature_context->sequence = g_strdup(parser->sequence_name) ;

      /* This is a COMPLETE HACK......we just set all the mapping stuff to have fictitious
       * coords for now....in the future it must all be filled in with calls to get the mapping from
       * the requested sequence to the parent etc. etc....
       * We assume that we only ask for complete clones....
       *  */
      {
	int tmp_pad = 100000 ;
	int clone_length = parser->features_end - parser->features_start + 1 ;

	feature_context->sequence_to_parent.c1 = parser->features_start ;
	feature_context->sequence_to_parent.c2 = parser->features_end ;
	feature_context->sequence_to_parent.p1 = tmp_pad ;
	feature_context->sequence_to_parent.p2
	  = feature_context->sequence_to_parent.p1 + clone_length ;

	feature_context->features_to_sequence.p1 = parser->features_start ;
	feature_context->features_to_sequence.p2 = parser->features_end ;
	feature_context->features_to_sequence.c1 = parser->features_start ;
	feature_context->features_to_sequence.c2 = parser->features_end ;

	feature_context->parent_length = feature_context->sequence_to_parent.p2 + tmp_pad ;
	feature_context->parent = "chromsome_99" ;
      }

      g_datalist_init(&(feature_context->feature_sets)) ;

      g_datalist_foreach(&(parser->feature_sets), getFeatureArray, &(feature_context->feature_sets)) ;
    }

  return feature_context ;
}


/* If stop_on_error is TRUE the parser will not parse any further lines after it encounters
 * the first error in the GFF file. */
void zMapGFFSetStopOnError(ZMapGFFParser parser, gboolean stop_on_error)
{
  parser->stop_on_error = stop_on_error ;

  return ;
}


/* If SO_compliant is TRUE the parser will only accept SO terms for feature types. */
void zMapGFFSetSOCompliance(ZMapGFFParser parser, gboolean SO_compliant)
{
  parser->SO_compliant = SO_compliant ;

  return ;
}


/* If default_to_basic is TRUE the parser will create basic features for any unrecognised
 * feature type. */
void zMapGFFSetDefaultToBasic(ZMapGFFParser parser, gboolean default_to_basic)
{
  parser->default_to_basic = default_to_basic ;

  return ;
}


/* Return the GFF version which the parser is using. This is determined from the GFF
 * input stream from the header comments. */
int zMapGFFGetVersion(ZMapGFFParser parser)
{
  return parser->gff_version ;
}


/* Return line number of last line processed (this is the same as the number of lines processed. */
int zMapGFFGetLineNumber(ZMapGFFParser parser)
{
  return parser->line_count ;
}


/* If a zMapGFFNNN function has failed then this function returns a description of the error
 * in the glib GError format. If there has been no error then NULL is returned. */
GError *zMapGFFGetError(ZMapGFFParser parser)
{
  return parser->error ;
}


/* If free_on_destroy == TRUE then all the feature arrays will be freed when
 * the GFF parser is destroyed, otherwise they are left intact. Important
 * because caller may still want access to them after the destroy. */
void zMapGFFSetFreeOnDestroy(ZMapGFFParser parser, gboolean free_on_destroy)
{
  parser->free_on_destroy = free_on_destroy ;

  return ;
}


void zMapGFFDestroyParser(ZMapGFFParser parser)
{
  if (parser->error)
    g_error_free(parser->error) ;

  if (parser->source_name)
    g_free(parser->source_name) ;
  if (parser->source_version)
    g_free(parser->source_version) ;

  if (parser->sequence_name)
    g_free(parser->sequence_name) ;

  /* The datalist uses a destroy routine that only destroys the feature arrays if the caller wants
   * us to, see destroyFeatureArray() */
  if (!parser->parse_only && parser->feature_sets)
    g_datalist_clear(&(parser->feature_sets)) ;

  g_free(parser) ;

  return ;
}






/* 
 * --------------------- Internal functions ---------------------------
 */



/* This function expects a null-terminated C string that contains a GFF header line
 * which is a special form of comment line starting with a "##".
 *
 * GFF version 2 format for header lines is:
 *
 * ##gff-version <version_number>
 * ##source-version <program_identifier> <program_version>
 * ##date YYYY-MM-DD
 * ##sequence-region <sequence_name> <sequence_start> <sequence_end>
 *
 * Currently we require the gff-version, source-version and sequence-region
 * to have been set and ignore any other header comments.
 * 
 * Returns FALSE if passed a line that is not a header comment OR if there
 * was a parse error. In the latter case parser->error will have been set.
 */
static gboolean parseHeaderLine(ZMapGFFParser parser, char *line)
{
  gboolean result = FALSE ;
  enum {FIELD_BUFFER_LEN = 1001} ;			    /* If you change this, change the
							       scanf's below... */


  if (g_str_has_prefix(line, "##"))
    {
      int fields = 0 ;
      char *format_str = NULL ;

      /* There may be other header comments that we are not interested in so we just return TRUE. */
      result = TRUE ;

      /* Note that number of fields returned by sscanf calls does not include the initial ##<word>
       * as this is not assigned to a variable. */
      /* this could be turned into a table driven piece of code but not worth the effort currently. */
      if (g_str_has_prefix(line, "##gff-version")
	  && !parser->done_version)
	{
	  int version ;

	  fields = 1 ;
	  format_str = "%*13s%d" ;
	  
	  if ((fields = sscanf(line, format_str, &version)) != 1)
	    {
	      parser->error = g_error_new(parser->error_domain, ZMAP_GFF_ERROR_HEADER,
					  "Bad ##gff-version line %d: \"%s\"",
					  parser->line_count, line) ;
	      result = FALSE ;
	    }
	  else
	    {
	      parser->gff_version = version ;
	      parser->done_version = TRUE ;
	    }
	}
      else if (g_str_has_prefix(line, "##source-version")
	       && !parser->done_source)
	{
	  char program[FIELD_BUFFER_LEN] = {'\0'}, version[FIELD_BUFFER_LEN] = {'\0'} ;

	  fields = 3 ;
	  format_str = "%*s%1000s%1000s" ;
	  
	  if ((fields = sscanf(line, format_str, &program[0], &version[0])) != 2)
	    {
	      parser->error = g_error_new(parser->error_domain, ZMAP_GFF_ERROR_HEADER,
					  "Bad ##source-version line %d: \"%s\"",
					  parser->line_count, line) ;
	      result = FALSE ;
	    }
	  else
	    {
	      parser->source_name = g_strdup(&program[0]) ;
	      parser->source_version = g_strdup(&version[0]) ;
	      parser->done_source = TRUE ;
	    }
     	}
      else if (g_str_has_prefix(line, "##sequence-region")
	       && !parser->done_sequence_region)
	{
	  char sequence_name[FIELD_BUFFER_LEN] = {'\0'} ;
	  int start = 0, end = 0 ;

	  fields = 4 ;
	  format_str = "%*s%1000s%d%d" ;
	  
	  if ((fields = sscanf(line, format_str, &sequence_name[0], &start, &end)) != 3)
	    {
	      parser->error = g_error_new(parser->error_domain, ZMAP_GFF_ERROR_HEADER,
					  "Bad ##sequence-region line %d: \"%s\"",
					  parser->line_count, line) ;
	      result = FALSE ;
	    }
	  else
	    {
	      parser->sequence_name = g_strdup(&sequence_name[0]) ;
	      parser->features_start = start ;
	      parser->features_end = end ;
	      parser->done_sequence_region = TRUE ;
	    }
     
	}
    }

  return result ;
}



/* This function expects a null-terminated C string that contains a complete GFF record
 * (i.e. a non-comment line), the function expects the caller to already have removed the
 * newline char from the end of the GFF record.
 *
 * GFF version 2 format for a line is:
 *
 * <sequence> <source> <feature> <start> <end> <score> <strand> <phase> [attributes] [#comments]
 * 
 * The format_str matches the above splitting everything into its own strings, note that
 * some fields, although numerical, default to the char "." so cannot be simply scanned into
 * a number. The only tricky bit is to get at the attributes and comments which have
 * white space in them, this scanf format string seems to do it:
 * 
 *  format_str = "%50s%50s%50s%d%d%50s%50s%50s %999[^#] %999c"
 *  
 *  " %999[^#]" Jumps white space after the last mandatory field and then gets everything up to
 *              the next "#", so this will fail if people put a "#" in their attributes !
 * 
 *  " %999c"    Reads everything from (and including) the "#" found by the previous pattern.
 * 
 * If it turns out that people do have "#" chars in their attributes we will have do our own
 * parsing of this section of the line.
 * 
 * 
 *  */
static gboolean parseBodyLine(ZMapGFFParser parser, char *line)
{
  gboolean result = FALSE ;
  char sequence[GFF_MAX_FIELD_CHARS + 1] = {'\0'},
    source[GFF_MAX_FIELD_CHARS + 1] = {'\0'}, feature_type[GFF_MAX_FIELD_CHARS + 1] = {'\0'},
    score_str[GFF_MAX_FIELD_CHARS + 1] = {'\0'}, strand_str[GFF_MAX_FIELD_CHARS + 1] = {'\0'},
    phase_str[GFF_MAX_FIELD_CHARS + 1] = {'\0'},
    attributes[GFF_MAX_FREETEXT_CHARS + 1] = {'\0'}, comments[GFF_MAX_FREETEXT_CHARS + 1] = {'\0'} ;
  int start = 0, end = 0 ;
  double score = 0 ;
  char *format_str = "%50s%50s%50s%d%d%50s%50s%50s %1000[^#] %1000c" ; 
  int fields ;


  if ((fields = sscanf(line, format_str,
		       &sequence[0], &source[0], &feature_type[0],
		       &start, &end, &score_str[0], &strand_str[0], &phase_str[0],
		       &attributes[0], &comments[0]))
      < GFF_MANDATORY_FIELDS)
    {
      parser->error = g_error_new(parser->error_domain, ZMAP_GFF_ERROR_BODY,
				  "GFF line %d - Mandatory fields missing in: \"%s\"",
				  parser->line_count, line) ;
      result = FALSE ;
    }
  else
    {
      ZMapFeatureType type ;
      ZMapStrand strand ;
      ZMapPhase phase ;
      char *err_text = NULL ;

      /* I'm afraid I'm not doing assembly stuff at the moment, its not worth it....if I need
       * to change this decision I can just this section.....
       * Code just silently drops these lines.
       *  */
      if (g_ascii_strcasecmp(source, "assembly_tag") == 0)
	{
	  return TRUE ;
	}

      /* Verbose but worth it for debugging.... */
      if (strlen(sequence) == GFF_MAX_FREETEXT_CHARS)
	err_text = g_strdup_printf("sequence name too long: %s", sequence) ;
      else if (strlen(source) == GFF_MAX_FREETEXT_CHARS)
	err_text = g_strdup_printf("source name too long: %s", source) ;
      else if (strlen(feature_type) == GFF_MAX_FREETEXT_CHARS)
	err_text = g_strdup_printf("feature_type name too long: %s", feature_type) ;
      else if (!formatType(parser->SO_compliant, parser->default_to_basic, feature_type, &type))
	err_text = g_strdup_printf("feature_type not recognised: %s", feature_type) ;
      else if (!formatScore(score_str, &score))
	err_text = g_strdup_printf("score format not recognised: %s", score_str) ;
      else if (!formatStrand(strand_str, &strand))
	err_text = g_strdup_printf("strand format not recognised: %s", strand_str) ;
      else if (!formatPhase(phase_str, &phase))
	err_text = g_strdup_printf("phase format not recognised: %s", phase_str) ;

      if (err_text)
	{
	  parser->error = g_error_new(parser->error_domain, ZMAP_GFF_ERROR_BODY,
				      "GFF line %d - %s (\"%s\")",
				      parser->line_count, err_text, line) ;
	  g_free(err_text) ;
	  result = FALSE ;
 	}
      else
	{
	  result = makeNewFeature(parser, sequence, source, type,
				  start, end, score, strand, phase,
				  attributes) ;
	}
    }


  return result ;
}



/* SOME WORK STILL TO DO ON THE PARSE_ONLY VERSION AND MY NEW VERSION THAT USES GDATA NOT AN
 * ARRAY.............. */

static gboolean makeNewFeature(ZMapGFFParser parser, char *sequence, char *source,
			       ZMapFeatureType feature_type,
			       int start, int end, double score, ZMapStrand strand,
			       ZMapPhase phase, char *attributes)
{
  gboolean result = FALSE ;
  char *feature_name = NULL ;
  ZMapFeature feature = NULL ;
  char *first_attr = NULL ;
  ZMapGFFParserFeatureSet feature_set = NULL ; ;
  gboolean has_name = TRUE ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  ZMapFeatureStruct new_feature ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
  ZMapFeature new_feature ;

  
  /* Look for an explicit feature name for the GFF record, if none exists use the sequence
   * name itself. */
  if (!(feature_name = getFeatureName(attributes)))
    {
      feature_name = sequence ;
      has_name = FALSE ;
    }

  /* Check if the "source" for this feature is already known, if it is then check if there
   * is already a multiline feature with the same name as we will need to augment it with this data. */
  if (!parser->parse_only &&
      (feature_set = (ZMapGFFParserFeatureSet)g_datalist_get_data(&(parser->feature_sets), source)))
    {
      feature = (ZMapFeature)g_datalist_get_data(&(feature_set->multiline_features), feature_name) ;
    }


  if (parser->parse_only || !feature)
    {

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      memset(&new_feature, 0, sizeof(ZMapFeatureStruct)) ;
      new_feature.id = ZMAPFEATUREID_NULL ;
      new_feature.type = ZMAPFEATURE_INVALID ;		    /* hack to detect empty feature.... */
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

      new_feature = g_new0(ZMapFeatureStruct, 1) ;
      new_feature->id = ZMAPFEATUREID_NULL ;
      new_feature->type = ZMAPFEATURE_INVALID ;
    }

  /* FOR PARSE ONLY WE WOULD LIKE TO COTINUE TO USE THE LOCAL VARIABLE new_feature....SORT THIS
   * OUT............. */


  if (parser->parse_only)
    {

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      feature = &new_feature ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
      feature = new_feature ;
    }
  else if (!feature)
    {
      /* If we haven't got a feature then create one, then add the feature to its source if that exists,
       * otherwise we have to create a list for the source and then add that to the list of sources
       *...ugh.  */

      /* If we don't have this feature_set yet, then make one. */
      if (!feature_set)
	{
	  feature_set = g_new0(ZMapGFFParserFeatureSetStruct, 1) ;

	  g_datalist_set_data_full(&(parser->feature_sets), source, feature_set, destroyFeatureArray) ;
	  
	  feature_set->source = g_strdup(source) ;

	  feature_set->multiline_features = NULL ;
	  g_datalist_init(&(feature_set->multiline_features)) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  feature_set->features = g_array_sized_new(FALSE, FALSE, sizeof(ZMapFeatureStruct), 30) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
	  g_datalist_init(&(feature_set->features)) ;

	  feature_set->parser = parser ;		    /* We need parser flags in the destroy
							       function for the feature_set list. */
	}


      /* Always add every new feature to the final array.... */

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      feature_set->features = g_array_append_val(feature_set->features, new_feature) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
      g_datalist_set_data(&(feature_set->features), feature_name, new_feature) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      /* Now set feature pointer to be the feature in the array...tacky.... */
      feature = &g_array_index(feature_set->features, ZMapFeatureStruct,
			       (feature_set->features->len - 1)) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
      feature = new_feature ;


      /* THIS PIECE OF CODE WILL NEED TO BE CHANGED AS I DO MORE TYPES..... */
      /* If the feature is one that must be built up from several GFF lines then add it to
       * our set of such features. There are arcane/adhoc rules in action here, any features
       * that do not have their own feature_name  _cannot_  be multiline features as such features
       * can _only_ be identified if they do have their own name. */

      if (has_name
	  && (feature_type == ZMAPFEATURE_SEQUENCE || feature_type == ZMAPFEATURE_TRANSCRIPT
	      || feature_type == ZMAPFEATURE_EXON || feature_type == ZMAPFEATURE_INTRON))
	{
	  g_datalist_set_data(&(feature_set->multiline_features), feature_name, feature) ;
	}

    }


  /* Phew, now fill in the feature.... */
  result = addDataToFeature(parser, feature, feature_name, sequence, source, feature_type,
			    start, end, score, strand,
			    phase, attributes) ;


  /* If we are only parsing then free any stuff allocated by addDataToFeature() */
  if (parser->parse_only)
    {
      g_free(feature->name) ;
      g_free(feature->method_name) ;

      if (feature->type == ZMAPFEATURE_TRANSCRIPT)
	{
	  if (feature->feature.transcript.exons)
	    g_array_free(feature->feature.transcript.exons, TRUE) ;
	  if (feature->feature.transcript.introns)
	    g_array_free(feature->feature.transcript.introns, TRUE) ;
	}

      g_free(feature) ;
    }

  return result ;
}




#ifdef ED_G_NEVER_INCLUDE_THIS_CODE

/* no parse_only */

static gboolean makeNewFeature(ZMapGFFParser parser, char *sequence, char *source,
			       ZMapFeatureType feature_type,
			       int start, int end, double score, ZMapStrand strand,
			       ZMapPhase phase, char *attributes)
{
  gboolean result = FALSE ;
  char *feature_name = NULL ;
  ZMapFeature feature = NULL ;
  char *first_attr = NULL ;
  ZMapGFFParserFeatureSet feature_set = NULL ; ;
  gboolean has_name = TRUE ;


  /* Look for an explicit feature name for the GFF record, if none exists use the sequence
   * name itself. */
  if (!(feature_name = getFeatureName(attributes)))
    {
      feature_name = sequence ;
      has_name = FALSE ;
    }

  /* Check if the "source" for this feature is already known, if it is then check if there
   * is already a multiline feature with the same name as we will need to augment it with this data. */
  if ((feature_set = (ZMapGFFParserFeatureSet)g_datalist_get_data(&(parser->feature_sets), source)))
    {
      feature = (ZMapFeature)g_datalist_get_data(&(feature_set->multiline_features), feature_name) ;
    }


  /* If we haven't got a feature then create one, then add the feature to its source if that exists,
   * otherwise we have to create a list for the source and then add that to the list of sources
   *...ugh.  */

  if (!feature)
    {
      ZMapFeatureStruct new_feature ;

      memset(&new_feature, 0, sizeof(ZMapFeatureStruct)) ;
      new_feature.id = ZMAPFEATUREID_NULL ;
      new_feature.type = ZMAPFEATURE_INVALID ;		    /* hack to detect empty feature.... */


      /* If we don't have this feature_set yet, then make one. */
      if (!feature_set)
	{
	  feature_set = g_new0(ZMapGFFParserFeatureSetStruct, 1) ;

	  g_datalist_set_data_full(&(parser->feature_sets), source, feature_set, destroyFeatureArray) ;
	  
	  feature_set->source = g_strdup(source) ;

	  feature_set->multiline_features = NULL ;
	  g_datalist_init(&(feature_set->multiline_features)) ;

	  feature_set->features = g_array_sized_new(FALSE, FALSE, sizeof(ZMapFeatureStruct), 30) ;

	  feature_set->parser = parser ;		    /* We need parser flags in the destroy
							       function for the feature_set list. */
	}


      /* Always add every new feature to the final array.... */
      feature_set->features = g_array_append_val(feature_set->features, new_feature) ;


      /* Now set feature pointer to be the feature in the array...tacky.... */
      feature = &g_array_index(feature_set->features, ZMapFeatureStruct,
			       (feature_set->features->len - 1)) ;


      /* THIS PIECE OF CODE WILL NEED TO BE CHANGED AS I DO MORE TYPES..... */
      /* If the feature is one that must be built up from several GFF lines then add it to
       * our set of such features. There are arcane/adhoc rules in action here, any features
       * that do not have their own feature_name  _cannot_  be multiline features as such features
       * can _only_ be identified if they do have their own name. */

      if (has_name
	  && (feature_type == ZMAPFEATURE_SEQUENCE || feature_type == ZMAPFEATURE_TRANSCRIPT
	      || feature_type == ZMAPFEATURE_EXON || feature_type == ZMAPFEATURE_INTRON))
	{
	  g_datalist_set_data(&(feature_set->multiline_features), feature_name, feature) ;
	}

    }

  /* Phew, now fill in the feature.... */
  result = addDataToFeature(parser, feature, feature_name, sequence, source, feature_type,
			    start, end, score, strand,
			    phase, attributes) ;

  return result ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */




static gboolean addDataToFeature(ZMapGFFParser parser, ZMapFeature feature,
				 char *name,
				 char *sequence, char *source, ZMapFeatureType feature_type,
				 int start, int end, double score, ZMapStrand strand,
				 ZMapPhase phase, char *attributes)
{
  gboolean result = FALSE ;

  /* Note we have hacked this up for now...in the end we should have a unique id for each feature
   * but for now we will look at the type to determine if a feature is empty or not..... */
  /* If its an empty feature then initialise... */
  if (feature->type == ZMAPFEATURE_INVALID)
    {
      feature->name = g_strdup(name) ;
      feature->type = feature_type ;
      feature->x1 = start ;
      feature->x2 = end ;

      feature->method_name = g_strdup(source) ;

      feature->strand = strand ;
      feature->phase = phase ;
      feature->score = score ;

      result = TRUE ;
    }


  /* There is going to have to be some hacky code here to decide when something goes from being
   * a single exon to being part of a transcript..... */

  if (feature_type == ZMAPFEATURE_EXON)
    {
      ZMapSpanStruct exon ;

      exon.x1 = start ;
      exon.x2 = end ;

      /* This is still not correct I think ??? we shouldn't be using the transcript field but
       * instead the lone exon field. */

      if (!feature->feature.transcript.exons)
	feature->feature.transcript.exons = g_array_sized_new(FALSE, TRUE,
							      sizeof(ZMapSpanStruct), 30) ;

      g_array_append_val(feature->feature.transcript.exons, exon) ;

      /* If this is _not_ single exon we make it into a transcript.
       * This is a bit adhoc but so is GFF v2. */
      if (feature->type != ZMAPFEATURE_EXON
	  || feature->feature.transcript.exons->len > 1)
	feature->type = ZMAPFEATURE_TRANSCRIPT ;

      result = TRUE ;
    }
  else if (feature_type == ZMAPFEATURE_INTRON)
    {
      ZMapSpanStruct intron ;

      intron.x1 = start ;
      intron.x2 = end ;

      if (!feature->feature.transcript.introns)
	feature->feature.transcript.introns = g_array_sized_new(FALSE, TRUE,
							      sizeof(ZMapSpanStruct), 30) ;

      g_array_append_val(feature->feature.transcript.introns, intron) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      /* I DON'T THINK WE WANT TO DO THIS BECAUSE WE MAY JUST HAVE A SET OF CONFIRMED INTRONS
       * THAT DO NOT ACTUALLY REPRESENT A TRANSCRIPT YET..... */


      /* If we have more than one intron then we are going to force the type to be
       * transcript. */
      if (feature->type != ZMAPFEATURE_TRANSCRIPT
	  && feature->feature.transcript.introns->len > 1)
	feature->type = ZMAPFEATURE_TRANSCRIPT ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

      result = TRUE ;
    }
  else if (feature_type == ZMAPFEATURE_HOMOL)
    {
      ZMapHomolType homol_type ;
      int query_start = 0, query_end = 0 ;

      /* Note that we do not put the extra "align" information for gapped alignments into the
       * GFF files from acedb so no need to worry about it just now...... */


      /* if this fails, what do we do...should just log the error I think..... */
      if ((result = getHomolAttrs(attributes, &homol_type, &query_start, &query_end)))
	{
	  feature->feature.homol.type = homol_type ;
	  feature->feature.homol.y1 = query_start ;
	  feature->feature.homol.y2 = query_end ;
	  feature->feature.homol.score = score ;
	  feature->feature.homol.align = NULL ;		    /* not supported currently.... */
	}
    }

  return result ;
}



/* This routine attempts to find the features name from its attributes field, in output from
 * acedb the attributes start with:
 * 
 *          class "object_name"    e.g.   Sequence "B0250.1"
 * 
 * So this routine looks for and extracts the object_name, if it can't find one it returns
 * NULL.
 * 
 * For other dumpers this may be different, for GFF v2 this is all ad hoc for GFF v3 its
 * formalised and so easier.
 * 
 * 
 *  */
static char *getFeatureName(char *attributes)
{
  char *feature_name = NULL ;
  int attr_fields ;
  char *attr_format_str = "%50s %*[\"]%50[^\"]%*[\"]%*s" ;
  char class[GFF_MAX_FIELD_CHARS + 1] = {'\0'}, name[GFF_MAX_FIELD_CHARS + 1] = {'\0'} ;


  if ((attr_fields = sscanf(attributes, attr_format_str, &class[0], &name[0])) == 2)
    {
      feature_name = g_strdup(name) ;
    }

  return feature_name ;
}

/* 
 * 
 * Format of similarity/homol attribute section is:
 * 
 *          Target "class:obj_name" start end
 * 
 * Format string extracts  class:obj_name  and  start and end.
 * 
 *  */
static gboolean getHomolAttrs(char *attributes, ZMapHomolType *homol_type_out,
			      int *start_out, int *end_out)
{
  gboolean result = FALSE ;
  int attr_fields ;
  char *attr_format_str = "%*s %*[\"]%50[^\"]%*s%d%d" ;
  char homol_type[GFF_MAX_FIELD_CHARS + 1] = {'\0'} ;
  int start = 0, end = 0 ;

  if ((attr_fields = sscanf(attributes, attr_format_str, &homol_type[0], &start, &end)) == 3)
    {
      if (g_ascii_strncasecmp(homol_type, "Sequence:", 9) == 0)
	*homol_type_out = ZMAPHOMOL_N_HOMOL ;
      else if (g_ascii_strncasecmp(homol_type, "Protein:", 8) == 0)
	*homol_type_out = ZMAPHOMOL_X_HOMOL ;
      /* or what.....these seem to the only possibilities for acedb gff output. */

      *start_out = start ;
      *end_out = end ;

      result = TRUE ;
    }

  return result ;
}





/* Type must equal something that the code understands. In GFF v1 this is unregulated and
 * could be anything. By default unrecognised terms are not converted.
 * 
 * 
 * SO_compliant == TRUE   only recognised SO terms will be accepted for feature types.
 * SO_compliant == FALSE  both SO and the earlier more adhoc names will be accepted (source for
 *                        these terms is wormbase GFF dumps).
 * 
 * This option is only valid when SO_compliant == FALSE.
 * misc_default == TRUE   unrecognised terms will be returned as "misc_feature" features types.
 * misc_default == FALSE  unrecognised terms will not be converted.
 * 
 * 
 *  */
gboolean formatType(gboolean SO_compliant, gboolean default_to_basic,
		    char *feature_type, ZMapFeatureType *type_out)
{
  gboolean result = FALSE ;
  ZMapFeatureType type = ZMAPFEATURE_INVALID ;


  /* Is feature_type a SO term, note that I do case-independent compares, hope this is correct. */
  if (g_ascii_strcasecmp(feature_type, "trans_splice_acceptor_site") == 0)
    {
      type = ZMAPFEATURE_BOUNDARY ;
    }
  else if (g_ascii_strcasecmp(feature_type, "transposable_element_insertion_site") == 0
	   || g_ascii_strcasecmp(feature_type, "deletion") == 0)
    {
      type = ZMAPFEATURE_VARIATION ;
    }
  if (g_ascii_strcasecmp(feature_type, "region") == 0)
    {
      type = ZMAPFEATURE_BASIC ;
    }
  else if (g_ascii_strcasecmp(feature_type, "virtual_sequence") == 0)
    {
      type = ZMAPFEATURE_SEQUENCE ;
    }
  else if (g_ascii_strcasecmp(feature_type, "reagent") == 0
	   || g_ascii_strcasecmp(feature_type, "oligo") == 0
	   || g_ascii_strcasecmp(feature_type, "PCR_product") == 0
	   || g_ascii_strcasecmp(feature_type, "RNAi_reagent") == 0
	   || g_ascii_strcasecmp(feature_type, "clone") == 0
	   || g_ascii_strcasecmp(feature_type, "clone_end") == 0
	   || g_ascii_strcasecmp(feature_type, "clone_end") == 0)
    {
      type = ZMAPFEATURE_BASIC ;
    }
  else if (g_ascii_strcasecmp(feature_type, "UTR") == 0
	   || g_ascii_strcasecmp(feature_type, "polyA_signal_sequence") == 0
	   || g_ascii_strcasecmp(feature_type, "polyA_site") == 0)
    {
      /* these should in the end be part of a gene structure..... */
      type = ZMAPFEATURE_BASIC ;
    }
  else if (g_ascii_strcasecmp(feature_type, "operon") == 0)
    {
      /* this should in the end be part of a gene group, not sure how we display that..... */
      type = ZMAPFEATURE_BASIC ;
    }
  else if (g_ascii_strcasecmp(feature_type, "pseudogene") == 0)
    {
      /* In SO terms this is a region but we don't have a basic "region" type that includes
       * exons like structure...suggests we need to remodel our feature struct.... */
      type = ZMAPFEATURE_TRANSCRIPT ;
    }
  else if (g_ascii_strcasecmp(feature_type, "experimental_result_region") == 0
	   || g_ascii_strcasecmp(feature_type, "chromosomal_structural_element") == 0)
    {
      type = ZMAPFEATURE_BASIC ;
    }
  else if (g_ascii_strcasecmp(feature_type, "transcript") == 0
	   || g_ascii_strcasecmp(feature_type, "protein_coding_primary_transcript") == 0
	   || g_ascii_strcasecmp(feature_type, "CDS") == 0
	   || g_ascii_strcasecmp(feature_type, "mRNA") == 0
	   || g_ascii_strcasecmp(feature_type, "nc_primary_transcript") == 0)
    {
      type = ZMAPFEATURE_TRANSCRIPT ;
    }
  else if (g_ascii_strcasecmp(feature_type, "exon") == 0)
    {
      type = ZMAPFEATURE_EXON ;
    }
  else if (g_ascii_strcasecmp(feature_type, "intron") == 0)
    {
      type = ZMAPFEATURE_INTRON ;
    }
  else if (g_ascii_strcasecmp(feature_type, "nucleotide_match") == 0
	   || g_ascii_strcasecmp(feature_type, "expressed_sequence_match") == 0
	   || g_ascii_strcasecmp(feature_type, "EST_match") == 0
	   || g_ascii_strcasecmp(feature_type, "cDNA_match") == 0
	   || g_ascii_strcasecmp(feature_type, "translated_nucleotide_match") == 0
	   || g_ascii_strcasecmp(feature_type, "protein_match") == 0)
    {
      type = ZMAPFEATURE_HOMOL ;
    }
  else if (g_ascii_strcasecmp(feature_type, "repeat_region") == 0
	   || g_ascii_strcasecmp(feature_type, "inverted_repeat") == 0
	   || g_ascii_strcasecmp(feature_type, "tandem_repeat") == 0
	   || g_ascii_strcasecmp(feature_type, "transposable_element") == 0)
    {
      type = ZMAPFEATURE_BASIC ;
    }
  else if (g_ascii_strcasecmp(feature_type, "SNP") == 0
	   || g_ascii_strcasecmp(feature_type, "sequence_variant") == 0
	   || g_ascii_strcasecmp(feature_type, "substitution") == 0)
    {
      type = ZMAPFEATURE_VARIATION ;
    }


  if (!SO_compliant)
    {
      if (g_ascii_strcasecmp(feature_type, "SL1_acceptor_site") == 0
	  || g_ascii_strcasecmp(feature_type, "SL2_acceptor_site") == 0)
	{
	  type = ZMAPFEATURE_BOUNDARY ;
	}
      else if (g_ascii_strcasecmp(feature_type, "Clone_right_end") == 0)
	{
	  type = ZMAPFEATURE_BASIC ;
	}
      else if (g_ascii_strcasecmp(feature_type, "Clone") == 0
	       || g_ascii_strcasecmp(feature_type, "Clone_left_end") == 0
	       || g_ascii_strcasecmp(feature_type, "utr") == 0
	       || g_ascii_strcasecmp(feature_type, "experimental") == 0
	       || g_ascii_strcasecmp(feature_type, "reagent") == 0
	       || g_ascii_strcasecmp(feature_type, "repeat") == 0
	       || g_ascii_strcasecmp(feature_type, "structural") == 0
	       || g_ascii_strcasecmp(feature_type, "misc_feature") == 0)
	{
	  type = ZMAPFEATURE_BASIC ;
	}
      else if (g_ascii_strcasecmp(feature_type, "Pseudogene") == 0)
	{
	  /* REALLY NOT SURE ABOUT THIS CLASSIFICATION......SHOULD IT BE A TRANSCRIPT ? */
	  type = ZMAPFEATURE_TRANSCRIPT ;
	}
      else if (g_ascii_strcasecmp(feature_type, "SNP") == 0
	       || g_ascii_strcasecmp(feature_type, "complex_change_in_nucleotide_sequence") == 0)
	{
	  type = ZMAPFEATURE_VARIATION ;
	}
      else if (g_ascii_strcasecmp(feature_type, "Sequence") == 0)
	{
	  type = ZMAPFEATURE_SEQUENCE ;
	}
      else if (g_ascii_strcasecmp(feature_type, "transcript") == 0
	       || g_ascii_strcasecmp(feature_type, "protein-coding_primary_transcript") == 0
	       || g_ascii_strcasecmp(feature_type, "miRNA_primary_transcript") == 0
	       || g_ascii_strcasecmp(feature_type, "snRNA_primary_transcript") == 0
	       || g_ascii_strcasecmp(feature_type, "snoRNA_primary_transcript") == 0
	       || g_ascii_strcasecmp(feature_type, "tRNA_primary_transcript") == 0
	       || g_ascii_strcasecmp(feature_type, "rRNA_primary_transcript") == 0)
	{
	  /* N.B. "protein-coding_primary_transcript" is a typo in wormDB currently, should
	   * be all underscores. */
	  type = ZMAPFEATURE_TRANSCRIPT ;
	}
      else if (g_ascii_strcasecmp(feature_type, "similarity") == 0
	       || g_ascii_strcasecmp(feature_type, "transcription") == 0)
	{
	  type = ZMAPFEATURE_HOMOL ;
	}
      else if (g_ascii_strcasecmp(feature_type, "trans-splice_acceptor") == 0)
	{
	  type = ZMAPFEATURE_BOUNDARY ;
	}
      else if (g_ascii_strcasecmp(feature_type, "coding_exon") == 0
	       || g_ascii_strcasecmp(feature_type, "exon") == 0)
	{
	  type = ZMAPFEATURE_EXON ;
	}
      else if (g_ascii_strcasecmp(feature_type, "intron") == 0)
	{
	  type = ZMAPFEATURE_INTRON ;
	}
      else if (default_to_basic)
	{
	  /* If we allow defaulting of unrecognised features, the default is a "basic" feature. */
	  type = ZMAPFEATURE_BASIC ;
	}
    }


  if (type != ZMAPFEATURE_INVALID)
    {
      result = TRUE ;
      *type_out = type ;
    }

  return result ;
}


/* Score must either be the char '.' or a valid floating point number, currently
 * if the score is '.' we return 0, this may not be correct. */
gboolean formatScore(char *score_str, gdouble *score_out)
{
  gboolean result = FALSE ;

  if (strlen(score_str) == 1 && strcmp(score_str, ".") == 0)
    {
      result = TRUE ;
      *score_out = 0 ;
    }
  else
    {
      gdouble score ;
      char *last_char ;

      score = g_ascii_strtod(score_str, &last_char) ; /* resets errno for us. */
      if (*last_char != '\0' || errno != 0)
	result = FALSE ;
      else
	{
	  result = TRUE ;
	  *score_out = score ;
	}
    }

  return result ;
}
	

/* Strand must be one of the chars '+', '-' or '.'. */
gboolean formatStrand(char *strand_str, ZMapStrand *strand_out)
{
  gboolean result = FALSE ;

  if (strlen(strand_str) == 1
      && (*strand_str == '+' || *strand_str == '-' || *strand_str == '.'))
    {
      result = TRUE ;

      switch (*strand_str)
	{
	case '+':
	  *strand_out = ZMAPSTRAND_DOWN ;
	  break ;
	case '-':
	  *strand_out = ZMAPSTRAND_UP ;
	  break ;
	default:
	  *strand_out = ZMAPSTRAND_NONE ;
	  break ;
	}
    }

  return result ;
}
	

/* Phase must either be the char '.' or 0, 1 or 2. */
gboolean formatPhase(char *phase_str, ZMapPhase *phase_out)
{
  gboolean result = FALSE ;

  if (strlen(phase_str) == 1
      && (*phase_str == '.' == 0
	  || *phase_str == '0' == 0 || *phase_str == '1' == 0 || *phase_str == '2' == 0))
    {
      result = TRUE ;

      switch (*phase_str)
	{
	case '0':
	  *phase_out = ZMAPPHASE_0 ;
	  break ;
	case '1':
	  *phase_out = ZMAPPHASE_1 ;
	  break ;
	case '2':
	  *phase_out = ZMAPPHASE_2 ;
	  break ;
	default:
	  *phase_out = ZMAPPHASE_NONE ;
	  break ;
	}
    }

  return result ;
}




#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* This is a GDataForeachFunc() and is called for each element of a GData list as a result
 * of a call to zmapGFFGetFeatures(). The function adds the feature array returned 
 * in the GData element to the GArray in user_data. */
static void getFeatureArray(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapGFFParserFeatureSet feature_set = (ZMapGFFParserFeatureSet)data ;
  GData **features = (GData **)user_data ;
  ZMapFeatureSet new_features ;

  new_features = g_new0(ZMapFeatureSetStruct, 1) ;

  new_features->source = g_strdup(feature_set->source) ;
  new_features->features = feature_set->features ;

  g_datalist_set_data(features, new_features->source, new_features) ;

  return ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


/* This is a GDataForeachFunc() and is called for each element of a GData list as a result
 * of a call to zmapGFFGetFeatures(). The function adds the feature array returned 
 * in the GData element to the GArray in user_data. */
static void getFeatureArray(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapGFFParserFeatureSet feature_set = (ZMapGFFParserFeatureSet)data ;
  GData **features = (GData **)user_data ;
  ZMapFeatureSet new_features ;

  new_features = g_new0(ZMapFeatureSetStruct, 1) ;

  new_features->source = g_strdup(feature_set->source) ;
  new_features->features = feature_set->features ;

  g_datalist_set_data(features, new_features->source, new_features) ;

  return ;
}




/* This is a GDestroyNotify() and is called for each element in a GData list when
 * the list is cleared with g_datalist_clear (), the function must free the GArray
 * and GData lists. */
void destroyFeatureArray(gpointer data)
{
  ZMapGFFParserFeatureSet feature_set = (ZMapGFFParserFeatureSet)data ;

  g_free(feature_set->source) ;



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE

  /* THIS IS NOT AN ARRAY IN FACT..... */

  /* When I try to get rid of the array but _not_ the data it seems to free the data as well ! */

  /* Only free actual array data if caller wants it freed. */
  if (feature_set->parser->free_on_destroy)
    g_array_free(feature_set->features, TRUE) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */




  /* No data to free in this list, just clear it. */
  g_datalist_clear(&(feature_set->multiline_features)) ;

  g_free(feature_set) ;

  return ;
}
