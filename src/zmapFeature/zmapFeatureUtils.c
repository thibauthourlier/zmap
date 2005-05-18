/*  File: zmapFeature.c
 *  Author: Rob Clack (rnc@sanger.ac.uk)
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
 * Description: Manipulates features.
 *              1
 * Exported functions: See zmapFeature.h
 * HISTORY:
 * Last edited: May 13 18:01 2005 (edgrif)
 * Created: Tue Nov 2 2004 (rnc)
 * CVS info:   $Id: zmapFeatureUtils.c,v 1.9 2005-05-18 10:53:37 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <stdio.h>
#include <unistd.h>
#include <ZMap/zmapFeature.h>
#include <ZMap/zmapUtils.h>


typedef struct
{
  gboolean status ;
  GIOChannel *channel ;
} DumpFeaturesStruct, *DumpFeatures ;


static void printFeatureContext(ZMapFeatureContext feature_context, DumpFeatures feature_dump) ;
static void printFeatureSet(GQuark key_id, gpointer data, gpointer user_data) ;
static void printFeature(GQuark key_id, gpointer data, gpointer user_data) ;
static gboolean printLine(GIOChannel *channel, gchar *line) ;


static void printFeatureSet(GQuark key_id, gpointer data, gpointer user_data) ;
static void printFeature(GQuark key_id, gpointer data, gpointer user_data) ;


/* This function creates a unique id for a feature. This is essential if we are to use the
 * g_datalist package to hold and reference features. Code should _ALWAYS_ use this function
 * to produce these IDs.
 * Caller must g_free() returned string when finished with. */
char *zMapFeatureCreateName(ZMapFeatureType feature_type, char *feature,
			    ZMapStrand strand, int start, int end, int query_start, int query_end)
{
  char *feature_name = NULL ;

  if (zMapFeatureSetCoords(strand, &start, &end, &query_start, &query_end))
    {
      if (feature_type == ZMAPFEATURE_HOMOL)
	feature_name = g_strdup_printf("%s.%d-%d-%d-%d", feature,
				       start, end, query_start, query_end) ;
      else
	feature_name = g_strdup_printf("%s.%d-%d", feature, start, end) ;
    }

  return feature_name ;
}


/* Like zMapFeatureCreateName() but returns a quark representing the feature name. */
GQuark zMapFeatureCreateID(ZMapFeatureType feature_type, char *feature,
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


/* In zmap we hold coords in the forward orientation always and get strand from the strand
 * member of the feature struct. This function looks at the supplied strand and sets the
 * coords accordingly. */
/* ACTUALLY I REALISE I'M NOT QUITE SURE WHAT I WANT THIS FUNCTION TO DO.... */
gboolean zMapFeatureSetCoords(ZMapStrand strand, int *start, int *end, int *query_start, int *query_end)
{
  gboolean result = FALSE ;

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



ZMapFeature zMapFeatureFindFeatureInContext(ZMapFeatureContext feature_context,
					    GQuark type_id, GQuark feature_id)
{
  ZMapFeature feature = NULL ;
  ZMapFeatureSet feature_set ;

  if ((feature_set = (ZMapFeatureSet)g_datalist_id_get_data(&(feature_context->feature_sets), type_id)))
    {
      feature = (ZMapFeature)g_datalist_id_get_data(&(feature_set->features), feature_id) ;
    }

  return feature ;
}



ZMapFeature zMapFeatureFindFeatureInSet(ZMapFeatureSet feature_set, GQuark feature_id)
{
  ZMapFeature feature ;

  feature = (ZMapFeature)g_datalist_id_get_data(&(feature_set->features), feature_id) ;

  return feature ;
}


GData *zMapFeatureFindSetInContext(ZMapFeatureContext feature_context, GQuark set_id)
{
  GData *features = NULL ;
  ZMapFeatureSet feature_set ;

  if ((feature_set = g_datalist_id_get_data(&(feature_context->feature_sets), set_id)))
    features = feature_set->features ;

  return features ;
}






/* Dump out a feature context, if file is NULL then goes to stdout. */
void zMapFeatureDump(ZMapFeatureContext feature_context, char *file)
{
  DumpFeaturesStruct dump_features ;
  GIOChannel *channel ;
  GError *channel_error = NULL ;
  char *filepath = NULL ;


  zMapAssert(feature_context) ;

  
  /* open output file */
  if (!file)
    {
      filepath = g_strdup("stdout") ;

      channel = g_io_channel_unix_new(STDOUT_FILENO) ;
    }
  else
    {
      if (g_path_is_absolute(file))
	filepath = g_strdup(file) ;
      else
	{
	  gchar *curr_dir = g_get_current_dir() ;

	  filepath = g_build_path("/", curr_dir, file, NULL) ;

	  g_free(curr_dir) ;
	}

      channel = g_io_channel_new_file(filepath, "w", &channel_error) ;
    }

  if (!channel)
    {
      zMapShowMsg(ZMAP_MSG_WARNING, "Can't open output file \"%s\": %s",
		  filepath, channel_error->message) ;
      g_error_free(channel_error) ;
    }
  else
    {
      dump_features.status = TRUE ;
      dump_features.channel = channel ;

      /* dump out the data */
      printFeatureContext(feature_context, &dump_features) ;

      /* close output file */
      if (g_io_channel_shutdown(channel, TRUE, &channel_error) != G_IO_STATUS_NORMAL)
	{
	  zMapShowMsg(ZMAP_MSG_WARNING, "Error closing output file \"%s\": %s",
		      filepath, channel_error->message) ;
	  g_error_free(channel_error) ;
	}
      else 
	zMapShowMsg(ZMAP_MSG_INFORMATION, "Feature Context dumped to \"%s\"", filepath) ;
    }

  g_free(filepath) ;

  return ;
}



char *zmapFeatureLookUpEnum(int id, int enumType)
{
  /* These arrays must correspond 1:1 with the enums declared in zmapFeature.h */
  static char *types[]   = {"BASIC", "HOMOL", "EXON", "INTRON", "TRANSCRIPT",
			    "VARIATION", "BOUNDARY", "SEQUENCE"} ;
  static char *strands[] = {".", "+", "-" } ;
  static char *phases[]  = {"ZMAPPHASE_NONE", "ZMAPPHASE_0", "ZMAPPHASE_1", "ZMAPPHASE_2" } ;
  char *enum_str = NULL ;

  zMapAssert(enumType == TYPE_ENUM || enumType == STRAND_ENUM || enumType == PHASE_ENUM) ;

  switch (enumType)
    {
    case TYPE_ENUM:
      enum_str = types[id];
      break;
      
    case STRAND_ENUM:
      enum_str = strands[id];
      break;
      
    case PHASE_ENUM:
      enum_str = phases[id];
      break;
    }
  
  return enum_str ;
}





/* 
 *              Internal routines.
 */


static void printFeatureContext(ZMapFeatureContext feature_context, DumpFeatures dump_features)
{
  char *line ;

  line = g_strdup_printf("Feature Context:\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", 
			 feature_context->sequence_name, 
			 feature_context->parent_name,
			 feature_context->parent_span.x1,
			 feature_context->parent_span.x2,
			 feature_context->sequence_to_parent.p1,
			 feature_context->sequence_to_parent.p2,
			 feature_context->sequence_to_parent.c1,
			 feature_context->sequence_to_parent.c2,
			 feature_context->features_to_sequence.p1,
			 feature_context->features_to_sequence.p2,
			 feature_context->features_to_sequence.c1,
			 feature_context->features_to_sequence.c2) ;

  /* Only proceed if there's no problem printing the line */
  if ((dump_features->status = printLine(dump_features->channel, line)))
    g_datalist_foreach(&(feature_context->feature_sets), printFeatureSet, dump_features) ;

  g_free(line) ;

  return ;
}


static void printFeatureSet(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeatureSet feature_set = (ZMapFeatureSet)data ;
  DumpFeatures dump_features = (DumpFeatures)user_data ;
  char *line ;

  /* Once we have failed then we stop printing, note that there is no way to stop this routine
   * being called which is a shame.... */
  if (!dump_features->status)
    return ;

  line = g_strdup_printf("\tFeature Set:\t%s\n", g_quark_to_string(feature_set->unique_id)) ;

  /* Only proceed if there's no problem printing the line */
  if ((dump_features->status = printLine(dump_features->channel, line)))
    g_datalist_foreach(&(feature_set->features), printFeature, dump_features) ;
    
  g_free(line) ;

  return ;
}


static void printFeature(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeature feature = (ZMapFeature)data ;
  DumpFeatures dump_features = (DumpFeatures)user_data ;
  char *type, *strand, *phase ;
  GString *line ;
  gboolean unused ;


  /* Once we have failed then we stop printing, note that there is no way to stop this routine
   * being called which is a shame.... */
  if (!dump_features->status)
    return ;


  line = g_string_sized_new(150);
  type   = zmapFeatureLookUpEnum(feature->type, TYPE_ENUM);
  strand = zmapFeatureLookUpEnum(feature->strand, STRAND_ENUM);
  phase  = zmapFeatureLookUpEnum(feature->phase, PHASE_ENUM);
  
  g_string_printf(line, "\t\t%s\t%d\t%s\t%s\t%d\t%d\t%s\t%s\t%s\t%f", 
		  (char *)g_quark_to_string(key_id),
		  feature->db_id,
		  (char *)g_quark_to_string(feature->original_id),
		  type,
		  feature->x1,
		  feature->x2,
		  (char *)g_quark_to_string(feature->style),
		  strand,
		  phase,
		  feature->score) ;
  if (feature->text)
    {
      g_string_append_c(line, '\t');
      g_string_append(line, feature->text);
    }

  /* all these extra fields not required for now.
   *if (feature->type == ZMAPFEATURE_HOMOL)
   *{
   * line = g_strdup_printf("%s\t%d\t%d\t%d\t%d\t%d\t%f\t", line,
   *			 feature->feature.homol.type,
   *			 feature->feature.homol.y1,
   *			 feature->feature.homol.y2,
   *			 feature->feature.homol.target_strand,
   *			 feature->feature.homol.target_phase,
   *			 feature->feature.homol.score
   *			 );   
   * for (i = 0; i < feature->feature.homol.align->len; i++)
   *   {
   *     align = &g_array_index(feature->feature.homol.align, ZMapAlignBlockStruct, i);
   *     line = g_strdup_printf("%s\t%d\t%d\t%d\t%d", line,
   *			     align->q1,
   *			     align->q2,
   *		     align->t1,
   *			     align->t2
   *			     );
   *   }
   *
   *else if (feature->type == ZMAPFEATURE_TRANSCRIPT)
   *{
   *  line = g_strdup_printf("%s\t%d\t%d\t%d\t%d\t%d", line,
   *			 feature->feature.transcript.cdsStart,
   *			 feature->feature.transcript.cdsEnd,
   *			 feature->feature.transcript.start_not_found,
   *			 feature->feature.transcript.cds_phase,
   *			 feature->feature.transcript.endNotFound
   *			 );   
   *  for (i = 0; i < feature->feature.transcript.exons->len; i++)
   *    {
   *      exon = &g_array_index(feature->feature.transcript.exons, ZMapSpanStruct, i);
   *      line = g_strdup_printf("%s\t%d\t%d", line,
   *			     exon->x1,
   *			     exon->x2
   *			     );
   *    }
   *}
   */

  g_string_append_c(line, '\n');

  dump_features->status = printLine(dump_features->channel, line->str) ;

  g_string_free(line, TRUE) ;

  return ;
}


/* Should have a prefix level ?? */
static gboolean printLine(GIOChannel *channel, gchar *line)
{
  gboolean status = TRUE ;
  gsize bytes_written ;
  GError *channel_error = NULL ;

  if (g_io_channel_write_chars(channel, line, -1, &bytes_written, &channel_error) != G_IO_STATUS_NORMAL)
    {
      zMapShowMsg(ZMAP_MSG_WARNING, "Error writing to output file: %50s... : %s",
		  line, channel_error->message) ;
      g_error_free(channel_error) ;
      status = FALSE ;
    }

  return status ;
}


