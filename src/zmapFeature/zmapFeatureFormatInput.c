/*  File: zmapFeatureFormatInput.c
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
 * Last edited: Sep 16 14:53 2005 (rds)
 * Created: Thu Sep 15 12:01:30 2005 (rds)
 * CVS info:   $Id: zmapFeatureFormatInput.c,v 1.1 2005-09-20 17:24:23 rds Exp $
 *-------------------------------------------------------------------
 */

#include <zmapFeature_P.h>

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
gboolean zMapFeatureFormatType(gboolean SO_compliant, gboolean default_to_basic,
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
	       || g_ascii_strcasecmp(feature_type, "contig") == 0
	       || g_ascii_strcasecmp(feature_type, "supercontig") == 0
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
gboolean zMapFeatureFormatScore(char *score_str, gdouble *score_out)
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

      if ((result = zMapStr2Double(score_str, &score)))
	*score_out = score ;
    }

  return result ;
}
	

/* Strand must be one of the chars '+', '-' or '.'. */
gboolean zMapFeatureFormatStrand(char *strand_str, ZMapStrand *strand_out)
{
  gboolean result = FALSE ;

  if (strlen(strand_str) == 1
      && (*strand_str == '+' || *strand_str == '-' || *strand_str == '.'))
    {
      result = TRUE ;

      switch (*strand_str)
	{
	case '+':
	  *strand_out = ZMAPSTRAND_FORWARD ;
	  break ;
	case '-':
	  *strand_out = ZMAPSTRAND_REVERSE ;
	  break ;
	default:
	  *strand_out = ZMAPSTRAND_NONE ;
	  break ;
	}
    }

  return result ;
}
	

/* Phase must either be the char '.' or 0, 1 or 2. */
gboolean zMapFeatureFormatPhase(char *phase_str, ZMapPhase *phase_out)
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
