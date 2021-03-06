/*  File: zmapGFFGeneral.c
 *  Author: Steve Miller (sm23@sanger.ac.uk)
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
 * Description: This file contains material that is used by both
 * the v2 and v3 parsers. This consists of some (but not all) of the
 * functions publically exported as listed in src/include/ZMap/zmapGFF.h.
 *-------------------------------------------------------------------
 */


#include <ZMap/zmapGFF.hpp>
#include <ZMap/zmapFeatureLoadDisplay.hpp>

#include <zmapGFF_P.hpp>
#include <zmapGFF2_P.hpp>
#include <zmapGFF3_P.hpp>


/*
 * Check whether the parser passed in is a valid version (i.e. one that we handle)
 * or not. Returns TRUE if this is the case and FALSE otherwise.
 */
gboolean zMapGFFIsValidVersion(ZMapGFFParser pParser)
{
  gboolean bResult = FALSE ;

  if (!pParser)
    {

    }
  else if (pParser->gff_version == ZMAPGFF_VERSION_2)
    {
      bResult = TRUE ;
    }
  else if (pParser->gff_version == ZMAPGFF_VERSION_3)
    {
      bResult = TRUE ;
    }

  return bResult ;
}



/*
 * Internal static function declarations.
 */
static void getFeatureArray(GQuark key_id, gpointer data, gpointer user_data) ;
static void normaliseFeatures_V2(GData **feature_sets) ;
static void normaliseFeatures_V3(GData **feature_sets) ;
static void checkFeatureSetCB_V2(GQuark key_id, gpointer data, gpointer user_data_unused) ;
static void checkFeatureSetCB_V3(GQuark key_id, gpointer data, gpointer user_data_unused) ;
static void checkFeatureCB_V2(GQuark key_id, gpointer data, gpointer user_data_unused) ;
static void checkFeatureCB_V3(gpointer key_id, gpointer data, gpointer user_data_unused) ;
static gboolean removeTranscriptFeature(gpointer key,gpointer value, gpointer user_data);


/*
 * Public interface parser creation function. Calls version specific functions. Returns
 * NULL if we are passed unsupported version.
 */
ZMapGFFParser zMapGFFCreateParser(int iGFFVersion, const char *sequence, int features_start, int features_end, ZMapConfigSource source)
{
  ZMapGFFParser pParser = NULL ;

  if (iGFFVersion == ZMAPGFF_VERSION_2 )
    {
      pParser = zMapGFFCreateParser_V2(sequence, features_start, features_end, source) ;
    }
  else if (iGFFVersion == ZMAPGFF_VERSION_3 )
    {
      pParser = zMapGFFCreateParser_V3(sequence, features_start, features_end, source) ;
    }
  else
    {
      /* unknown version */
    }

  return pParser ;
}



/*
 * Public interface function to parse headers. Calls version specific functions.
 */
gboolean zMapGFFParseHeader(ZMapGFFParser parser, char *line, gboolean *header_finished, ZMapGFFHeaderState *header_state)
{
  gboolean bResult = FALSE ;

  zMapReturnValIfFail(parser && zMapGFFIsValidVersion(parser), bResult );

  if (parser->gff_version == ZMAPGFF_VERSION_2 )
    {
      bResult = zMapGFFParseHeader_V2(parser, line, header_finished, header_state) ;
    }
  else if (parser->gff_version == ZMAPGFF_VERSION_3 )
    {
      bResult = zMapGFFParse_V3(parser, line) ;
      *header_finished = zMapGFFGetHeaderGotMinimal_V3(parser) ;
    }
  else
    {
      /* unknown version  */
    }

  return bResult ;
}


/*
 * Public interface function to parse a section of sequence. Calls version specific functions.
 */
gboolean zMapGFFParseSequence(ZMapGFFParser parser, char *line, gboolean *sequence_finished)
{
  gboolean bResult = FALSE ;

  zMapReturnValIfFail(parser && zMapGFFIsValidVersion(parser), bResult );

  if (parser->gff_version == ZMAPGFF_VERSION_2 )
    {
      bResult = zMapGFFParseSequence_V2(parser, line, sequence_finished) ;
    }
  else if (parser->gff_version == ZMAPGFF_VERSION_3 )
    {
      /*
       * This should not be required since it's not part of the v3 spec.
       */
      bResult = FALSE ;
    }
  else
    {
      /* unknown version */
    }

  return bResult ;
}




/*
 * Public interface function for the general body line parser. Calls version specific stuff.
 */
gboolean zMapGFFParseLine(ZMapGFFParser parser, char *line)
{
  gboolean bResult = FALSE ;

  zMapReturnValIfFail(parser && zMapGFFIsValidVersion(parser), bResult );

  if (parser->gff_version == ZMAPGFF_VERSION_2)
    {
      bResult = zMapGFFParseLineLength_V2(parser, line, 0) ;
    }
  else if (parser->gff_version == ZMAPGFF_VERSION_3 )
    {
      bResult = zMapGFFParse_V3(parser, line) ;
    }
  else
    {
      /* unknown version */
    }

  return bResult ;
}

/*
 * Public interface for general body line parser. Calls version specific stuff.
 */
gboolean zMapGFFParseLineLength(ZMapGFFParser parser, char *line, gsize line_length)
{
  gboolean bResult = FALSE ;

  zMapReturnValIfFail(parser && zMapGFFIsValidVersion(parser), bResult );

  if (parser->gff_version == ZMAPGFF_VERSION_2)
    {
      bResult = zMapGFFParseLineLength_V2(parser, line, 0) ;
    }
  else if (parser->gff_version == ZMAPGFF_VERSION_3 )
    {
      bResult = zMapGFFParse_V3(parser, line) ;
    }
  else
    {
      /* unknown version */
    }

  return bResult ;

}


/*
 * General code to destroy parser.
 */
void zMapGFFDestroyParser(ZMapGFFParser parser)
{
  zMapReturnIfFail(parser && zMapGFFIsValidVersion(parser));

  if (parser->gff_version == ZMAPGFF_VERSION_2)
    {
      zMapGFFDestroyParser_V2(parser) ;
    }
  else if (parser->gff_version == ZMAPGFF_VERSION_3)
    {
      zMapGFFDestroyParser_V3(parser) ;
    }
  else
    {
      /* unknown version */
    }

  return ;
}







/* the servers need styles to add DNA and 3FT
 * they used to create temp style and then destroy these but that's not very good
 * they don't have styles info directly but this is stored in the parser
 * during the protocol steps
 */
ZMapStyleTree *zMapGFFParserGetStyles(ZMapGFFParser parser)
{
  if(!parser)
    return NULL ;

  if (!zMapGFFIsValidVersion(parser))
    return NULL ;

  return parser->sources;
}




/*
 * Used for both versions.
 *
 * We should do this internally with a var in the parser struct....
 * This function must be called prior to parsing feature lines, it is not required
 * for either the header lines or sequences.
 */
gboolean zMapGFFParserInitForFeatures(ZMapGFFParser parser, ZMapStyleTree *sources, gboolean parse_only)
{
  gboolean result = FALSE ;
  GQuark locus_id ;

  zMapReturnValIfFail(parser && zMapGFFIsValidVersion(parser) && sources, result);

  parser->sources = sources ;
  parser->parse_only = parse_only ;

  /* Check for Locus as one of the sources as this needs to be constructed as we go along. */
  locus_id = zMapStyleCreateID(ZMAP_FIXED_STYLE_LOCUS_NAME) ;
  if (!(parser->locus_set_style = parser->sources->find_style(locus_id)))
    {
      zMapLogWarning("Locus set will not be created, "
                     "could not find style \"%s\" for feature set \"%s\".",
                     ZMAP_FIXED_STYLE_LOCUS_NAME, ZMAP_FIXED_STYLE_LOCUS_NAME) ;
    }
  else
    {
      parser->locus_set_id = locus_id ;
    }



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

  return result ;
}


/*
 * Used by both versions.
 *
 * Return the set of features read from a file for a block.
 */
gboolean zMapGFFGetFeatures(ZMapGFFParser parser, ZMapFeatureBlock feature_block)
{
  int start,end;
  gboolean result = FALSE ;

  zMapReturnValIfFail(parser && zMapGFFIsValidVersion(parser), result) ;

  if (parser->state != ZMAPGFF_PARSER_ERR)
    {
      start = parser->features_start;
      end   = parser->features_end;

      if (parser->clip_mode)
        {
          if(start < parser->clip_start)
            start = parser->clip_start;
          if(end > parser->clip_end)
           end = parser->clip_end;
        }


      /* as request coordinates are often given as 1,0 we need to put real coordinates in */
      /* ideally chromosome coordinates would be better */

      /* NOTE we need to know the actual data returned as we
       *  mark empty featuresets as loaded over this range */
      feature_block->block_to_sequence.block.x1 = start;
      feature_block->block_to_sequence.block.x2 = end;

      if (!feature_block->block_to_sequence.parent.x2)
        {
          /* as request coordinates are often given as 1,0 we need to put real coordinates in */
          /* ideally chromosome coordinates would be better */
          feature_block->block_to_sequence.parent.x1 = start;
          feature_block->block_to_sequence.parent.x2 = end;
        }

      /* Actually we should only need to test feature_sets here really as there shouldn't be any
       * for parse_only.... */
      if (!parser->parse_only && parser->feature_sets)
      {
        g_datalist_foreach(&(parser->feature_sets), getFeatureArray, feature_block) ;
      }

     /* It is possible for features to be incomplete from GFF (e.g. transcripts without
      * an exon), we attempt to normalise them where possible...currently only
      * transcripts/exons. */
      if (parser->gff_version == ZMAPGFF_VERSION_2)
      {
        normaliseFeatures_V2(&(parser->feature_sets)) ;
        result = TRUE ;
      }
      else if (parser->gff_version == ZMAPGFF_VERSION_3)
      {
        normaliseFeatures_V3(&(parser->feature_sets)) ;
        result = TRUE ;
      }
      else
        result = FALSE ;

    }

  return result ;
}

/*
 * Used by both versions.
 *
 * This is a GDataForeachFunc() and is called for each element of a GData list as a result
 * of a call to zmapGFFGetFeatures(). The function adds the feature array returned
 * in the GData element to the GArray in user_data.
 */
static void getFeatureArray(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapGFFParserFeatureSet parser_feature_set = (ZMapGFFParserFeatureSet)data ;
  ZMapFeatureSet feature_set = parser_feature_set->feature_set ;
  ZMapFeatureBlock feature_block = (ZMapFeatureBlock)user_data ;

  zMapFeatureBlockAddFeatureSet(feature_block, feature_set);

  return ;
}


/*
 * Features may be created incomplete by 'faulty' GFF files, here we try
 * to make them valid.
 */
static void normaliseFeatures_V2(GData **feature_sets)
{
  /* Look through all datasets.... */
  g_datalist_foreach(feature_sets, checkFeatureSetCB_V2, NULL) ;

  return ;
}

/*
 * A GDataForeachFunc() which checks to see if the dataset passed in has
 * mulitline features and if they are then checks each feature to make sure
 * it is complete/valid. Currently this is only transcripts...see checkFeatureCB().
 */
static void checkFeatureSetCB_V2(GQuark key_id, gpointer data, gpointer user_data_unused)
{
  ZMapGFFParserFeatureSet parser_feature_set = (ZMapGFFParserFeatureSet)data ;

  if (parser_feature_set->multiline_features)
    {
      g_datalist_foreach(&(parser_feature_set->multiline_features), checkFeatureCB_V2, NULL) ;
    }

  return ;
}

/*
 * A GDataForeachFunc() which checks to see if the feature passed in is
 * complete/valid. Currently just checks transcripts to make sure they
 * have at least one exon.
 */
static void checkFeatureCB_V2(GQuark key_id, gpointer data, gpointer user_data_unused)
{
  ZMapFeature feature = (ZMapFeature)data ;

  switch (feature->mode)
    {
      case ZMAPSTYLE_MODE_TRANSCRIPT:
          zMapFeatureTranscriptNormalise(feature) ;
        break ;

      default:
        break ;
    }

  return ;
}


/*
 * This traverses all featuresets.
 */
static void normaliseFeatures_V3(GData **feature_sets)
{
  g_datalist_foreach(feature_sets, checkFeatureSetCB_V3, NULL) ;
  return ;
}

/*
 * Makes corrections to introns in transcript features.
 */
static void checkFeatureSetCB_V3(GQuark key_id, gpointer data, gpointer user_data_unused)
{
  ZMapGFFParserFeatureSet parser_feature_set = (ZMapGFFParserFeatureSet)data ;
  guint iRemoved = 0 ;

  /*
   * This traversal is to remove transcript features with no exons.
   */
  if ((iRemoved = g_hash_table_foreach_remove(parser_feature_set->feature_set->features, removeTranscriptFeature, NULL)))
    zMapLogWarning("%d transcripts removed because they have no exons.", iRemoved) ;

  /*
   * This traversal is to normalize introns in each transcript feature
   */
  if (parser_feature_set->feature_set->features)
  {
    g_hash_table_foreach(parser_feature_set->feature_set->features, checkFeatureCB_V3, NULL) ;
  }

  return ;
}

/*
 * Calls function to make correction to introns in transcript features.
 */
static void checkFeatureCB_V3(gpointer key_id, gpointer data, gpointer user_data_unused)
{
  ZMapFeature feature = (ZMapFeature)data ;

  switch (feature->mode)
    {
      case ZMAPSTYLE_MODE_TRANSCRIPT:
          zMapFeatureRemoveIntrons(feature) ;
          zMapFeatureTranscriptRecreateIntrons(feature) ;
        break ;

      default:
        break ;
    }

  return ;
}

/*
 * Return true if the feature is a transcript with no exons; return false otherwise.
 * This function simply signals whether or not the pointer is to be removed from the
 * hash table, but does not delete what it points to. This latter operation is performed
 * by the appropriate desroy function that has already been attached to the featureset.
 */
static gboolean removeTranscriptFeature(gpointer key, gpointer value, gpointer user_data_unused)
{
  gboolean bEmptyTranscript = FALSE ;
  ZMapFeature pFeature = NULL ;
  pFeature = (ZMapFeature) value ;

  zMapReturnValIfFail(pFeature, FALSE ) ;

  if (pFeature->mode == ZMAPSTYLE_MODE_TRANSCRIPT)
    {
      if (!pFeature->feature.transcript.exons->len)
        {
          bEmptyTranscript = TRUE ;
        }
    }

  return bEmptyTranscript ;
}



/*
 * Used by both versions.
 *
 * Optionally set mappings that are keys from the GFF source to feature set and style names.
 */
void zMapGFFParseSetSourceHash(ZMapGFFParser parser,
       GHashTable *source_2_feature_set, GHashTable *source_2_sourcedata)
{
  zMapReturnIfFail(parser && zMapGFFIsValidVersion(parser));

  parser->source_2_feature_set = source_2_feature_set ;
  parser->source_2_sourcedata = source_2_sourcedata ;

  /* Locus is an odd one out just now, we need to handle this differently..... */
  if (parser->locus_set_id)
    {
      ZMapFeatureSetDesc set_data ;
      ZMapFeatureSource source_data ;


      set_data = g_new0(ZMapFeatureSetDescStruct, 1) ;
      set_data->column_id = parser->locus_set_id ;
      set_data->feature_set_text = g_strdup_printf("Locus IDs") ;

      g_hash_table_insert(parser->source_2_feature_set,
                          GINT_TO_POINTER(parser->locus_set_id),
                          set_data) ;


      source_data = g_new0(ZMapFeatureSourceStruct, 1) ;
      source_data->source_id = parser->locus_set_id ;
      source_data->style_id = parser->locus_set_id ;
      source_data->source_text = g_quark_from_string("Locus IDs") ;


      g_hash_table_insert(parser->source_2_sourcedata,
                          GINT_TO_POINTER(parser->locus_set_id),
                          source_data) ;

    }


  return ;
}



/*
 * Used by both versions.
 *
 *servers have a list of columns in/out as provided by ACEDB and later used by pipes
 * here we privide a list of (single) featuresets as put into the context
 */
GList *zMapGFFGetFeaturesets(ZMapGFFParser parser)
{
  zMapReturnValIfFail(parser && zMapGFFIsValidVersion(parser), NULL);

  return (parser->src_feature_sets) ;
}



/*
 * Used by both versions.
 *
 *
 * If stop_on_error is TRUE the parser will not parse any further lines after it encounters
 * the first error in the GFF file.
 */
void zMapGFFSetStopOnError(ZMapGFFParser parser, gboolean stop_on_error)
{
  zMapReturnIfFail(parser && zMapGFFIsValidVersion(parser));

  if (parser->state != ZMAPGFF_PARSER_ERR)
    parser->stop_on_error = stop_on_error ;

  return ;
}




/*
 * Used by both versions.
 *
 * If SO_compliant is TRUE the parser will only accept SO terms for feature types.
 */
void zMapGFFSetSOCompliance(ZMapGFFParser parser, gboolean SO_compliant)
{
  zMapReturnIfFail(parser && zMapGFFIsValidVersion(parser));

  if (parser->state != ZMAPGFF_PARSER_ERR)
    parser->SO_compliant = SO_compliant ;

  return ;
}


/*
 * Used by both versions.
 * Sets the clipping mode for handling features that are either partly or wholly outside
 * the requested start/end for the target sequence.
 */
void zMapGFFSetFeatureClip(ZMapGFFParser parser, ZMapGFFClipMode clip_mode)
{
  zMapReturnIfFail(parser && zMapGFFIsValidVersion(parser));

  if (parser->state != ZMAPGFF_PARSER_ERR)
    {
      parser->clip_mode = clip_mode ;
    }

  return ;
}


/*
 * Used by both versions.
 *
 * Sets the start/end coords for clipping features.
 */
void zMapGFFSetFeatureClipCoords(ZMapGFFParser parser, int start, int end)
{
  zMapReturnIfFail(parser && zMapGFFIsValidVersion(parser));

  if (start <= 0 || end <= 0 || end < start )
    return ;

  if (parser->state != ZMAPGFF_PARSER_ERR)
    {
      parser->clip_start = start ;
      parser->clip_end = end ;
    }

  return ;
}




/*
 * Used by both versions.
 *
 * If default_to_basic is TRUE the parser will create basic features for any unrecognised
 * feature type.
 */
void zMapGFFSetDefaultToBasic(ZMapGFFParser parser, gboolean default_to_basic)
{
  zMapReturnIfFail(parser && zMapGFFIsValidVersion(parser)) ;

  if (parser->state != ZMAPGFF_PARSER_ERR)
    parser->default_to_basic = default_to_basic ;

  return ;
}


/*
 * Used by both versions.
 *
 * Return the GFF version which the parser is using. This is determined from the GFF
 * input stream from the header comments.
 */
int zMapGFFGetVersion(ZMapGFFParser parser)
{
  int version = ZMAPGFF_VERSION_UNKNOWN ;

  if (parser && (parser->state != ZMAPGFF_PARSER_ERR) )
    version = parser->gff_version ;

  return version ;
}



/*
 * Used by both versions.
 *
 * Return line number of last line processed (this is the same as the number of lines processed.
 * N.B. we always return this as it may be required for error diagnostics.
 */
int zMapGFFGetLineNumber(ZMapGFFParser parser)
{
  zMapReturnValIfFail(parser && zMapGFFIsValidVersion(parser), 0 );

  return parser->line_count ;
}

int zMapGFFGetLineBod(ZMapGFFParser parser)
{
  zMapReturnValIfFail(parser && zMapGFFIsValidVersion(parser), 0 );

  return parser->line_count_bod ;
}

int zMapGFFGetLineDir(ZMapGFFParser parser)
{
  zMapReturnValIfFail(parser && zMapGFFIsValidVersion(parser), 0 );

  return parser->line_count_dir ;
}

int zMapGFFGetLineSeq(ZMapGFFParser parser)
{
  zMapReturnValIfFail(parser && zMapGFFIsValidVersion(parser), 0 );

  return parser->line_count_seq ;
}

int zMapGFFGetLineFas(ZMapGFFParser parser)
{
  zMapReturnValIfFail(parser && zMapGFFIsValidVersion(parser), 0 );

  return parser->line_count_fas ;
}


/*
 * Increment the general line counter.
 */
void zMapGFFIncrementLineNumber(ZMapGFFParser parser)
{
  zMapReturnIfFail(parser && zMapGFFIsValidVersion(parser)) ;

  ++parser->line_count ;
}

/*
 * Body/data line counter
 */
void zMapGFFIncrementLineBod(ZMapGFFParser parser)
{
  zMapReturnIfFail(parser && zMapGFFIsValidVersion(parser)) ;

  ++parser->line_count_bod ;
}

/*
 * Directive line counter
 */
void zMapGFFIncrementLineDir(ZMapGFFParser parser)
{
  zMapReturnIfFail(parser && zMapGFFIsValidVersion(parser)) ;

  ++parser->line_count_dir ;
}

/*
 * Sequence line counter
 */
void zMapGFFIncrementLineSeq(ZMapGFFParser parser)
{
  zMapReturnIfFail(parser && zMapGFFIsValidVersion(parser)) ;

  ++parser->line_count_seq ;
}

/*
 * Fasta line counter
 */
void zMapGFFIncrementLineFas(ZMapGFFParser parser)
{
  zMapReturnIfFail(parser && zMapGFFIsValidVersion(parser)) ;

  ++parser->line_count_fas ;
}




/*
 * Used by both versions.
 *
 * Return the features_start coord
 */
int zMapGFFGetFeaturesStart(ZMapGFFParser parser)
{
  if (!parser || !zMapGFFIsValidVersion(parser))
    return 0 ;

  return parser->features_start ;
}


/*
 * Used by both versions.
 *
 * Return the features_end coord
 */
int zMapGFFGetFeaturesEnd(ZMapGFFParser parser)
{
  if (!parser || !zMapGFFIsValidVersion(parser))
    return 0 ;

  return parser->features_end ;
}


/*
 * Used by both versions.
 *
 * Return the sequence name stored in the parser
 */
char* zMapGFFGetSequenceName(ZMapGFFParser parser)
{
  if (!parser || !zMapGFFIsValidVersion(parser))
    return NULL ;

  return parser->sequence_name ;
}


/* 
 * Only applicable to GFF3.
 *
 * Return true if the sequence-region was specified in the header
 */
gboolean zMapGFFGotSequenceRegion(ZMapGFFParser parser)
{
  gboolean bResult = FALSE;

  if (parser && parser->gff_version == ZMAPGFF_VERSION_3)
    {
      bResult = zMapGFFGetHeaderGotMinimal_V3(parser) ;
    }

  return bResult;
}


/*
 * Used by both versions.
 *
 * If a zMapGFFNNN function has failed then this function returns a description of the error
 * in the glib GError format. If there has been no error then NULL is returned.
 */
GError *zMapGFFGetError(ZMapGFFParser parser)
{
  zMapReturnValIfFail(parser && zMapGFFIsValidVersion(parser), NULL);

  return parser->error ;
}


/*
 * Return the current number of features that have been created.
 */
int zMapGFFParserGetNumFeatures(ZMapGFFParser parser)
{
  zMapReturnValIfFail(parser && zMapGFFIsValidVersion(parser), 0) ;
  return(parser->num_features) ;
}

/*
 * Increment the number of features that have been created.
 */
void zMapGFFParserIncrementNumFeatures(ZMapGFFParser parser)
{
  zMapReturnIfFail(parser && zMapGFFIsValidVersion(parser)) ;
  ++parser->num_features ;
}



/*
 * Used by both versions.
 *
 * Returns TRUE if the parser has encountered an error from which it cannot recover and hence will
 * not process any more lines, FALSE otherwise.
 */
gboolean zMapGFFTerminated(ZMapGFFParser parser)
{
  gboolean result = TRUE ;
  zMapReturnValIfFail( parser && zMapGFFIsValidVersion(parser), result) ;

  if (parser->state != ZMAPGFF_PARSER_ERR)
    result = FALSE ;

  return result ;
}



/*
 * Used by both versions.
 *
 *
 * If free_on_destroy == TRUE then all the feature arrays will be freed when
 * the GFF parser is destroyed, otherwise they are left intact. Important
 * because caller may still want access to them after the destroy.
 */
void zMapGFFSetFreeOnDestroy(ZMapGFFParser parser, gboolean free_on_destroy)
{
  zMapReturnIfFail(parser && zMapGFFIsValidVersion(parser));

  if (parser->state != ZMAPGFF_PARSER_ERR)
    parser->free_on_destroy = free_on_destroy ;

  return ;
}


/*
 * Used by both versions.
 *
 *
 * Return true if the parser status is parsing the header/directives.
 * Also returns true if parsing hasn't started yet, because we need
 * to parser the header in that case.
 */
gboolean zMapGFFParsingHeader(ZMapGFFParser parser)
{
  gboolean result = FALSE ;

  if (parser->state == ZMAPGFF_PARSER_DIR || parser->state == ZMAPGFF_PARSER_NON)
    result = TRUE ;

  return result ;
}


gboolean zMapGFFSequenceDestroy(ZMapSequence sequence)
{
  gboolean bReturn = FALSE ;

  zMapReturnValIfFail(sequence, bReturn) ;

  if (sequence->sequence)
    g_free(sequence->sequence) ;
  /* g_free(sequence) ; */

  return bReturn ;
}

/*
 * Return the number of sequence records that the parser has stored.
 *
 * Note that: for v2 this can only be 0 or 1, but for v3 can be 0,1,...
 * and corresponds to the number of fasta records found in the GFF stream.
 */
int zMapGFFGetSequenceNum(ZMapGFFParser parser_base)
{
  int result = 0 ;

  if (parser_base->gff_version == ZMAPGFF_VERSION_2)
    {
      ZMapGFF2Parser parser = (ZMapGFF2Parser) parser_base ;
      if (parser->header_flags.done_header && g_quark_from_string(parser->sequence_name))
        {
          if(parser->seq_data.type != ZMAPSEQUENCE_NONE && parser->seq_data.sequence != NULL)
            {
              result = 1 ;
            }
        }
    }
  else if (parser_base->gff_version == ZMAPGFF_VERSION_3)
    {
      ZMapGFF3Parser parser = (ZMapGFF3Parser) parser_base ;
      result = parser->nSequenceRecords ;
    }

  return result ;
}

/*
 * Return a sequence item by name, or rather, GQuark representing the name.
 */
ZMapSequence zMapGFFGetSequence(ZMapGFFParser parser_base, GQuark sequence_name)
{
  /*
   * Pointer to ZMapSequenceStruct
   */
  ZMapSequence sequence = NULL;

  zMapReturnValIfFail(parser_base && sequence_name, sequence) ;

  /*
   * We do different things here depending on the GFF version.
   */
  if (parser_base->gff_version == ZMAPGFF_VERSION_2)
    {
      ZMapGFF2Parser parser = (ZMapGFF2Parser) parser_base ;

      /* There is only one sequence in the GFF2 parser and that's the genomic sequence. If the
         given name is the genomic sequence name, return it. */
      if (parser->header_flags.done_header && g_quark_from_string(parser->sequence_name) == sequence_name)
        {
          if(parser->seq_data.type != ZMAPSEQUENCE_NONE && (parser->seq_data.sequence != NULL && parser->raw_line_data == NULL))
            {
              sequence = g_new0(ZMapSequenceStruct, 1);
              *sequence = parser->seq_data;
              sequence->name = g_quark_from_string(parser->sequence_name);

              /* So we don't copy empty data */
              parser->seq_data.type     = ZMAPSEQUENCE_NONE;
              parser->seq_data.sequence = NULL; /* So it doesn't get free'd */
            }
        }
    }
  else if (parser_base->gff_version == ZMAPGFF_VERSION_3)
    {
      /* Loop through all of the parsed sequences looking for the one with the given name */
      ZMapGFF3Parser parser = (ZMapGFF3Parser) parser_base ;
      int sequence_record = 0 ;

      for ( ; sequence_record < parser->nSequenceRecords; ++sequence_record)
        {
          if (parser->pSeqData[sequence_record].name == sequence_name)
            {
              /*
               * Note that the v3 parser controls the lifetime of its pSeqData objects.
               */
              sequence = g_new0(ZMapSequenceStruct, 1) ;
              sequence->name = parser->pSeqData[sequence_record].name ;
              sequence->type = parser->pSeqData[sequence_record].type ;
              sequence->length = parser->pSeqData[sequence_record].length ;
              sequence->sequence = g_strdup(parser->pSeqData[sequence_record].sequence) ;

              break ;
            }
        }
    }

  return sequence;
}







