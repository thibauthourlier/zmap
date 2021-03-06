/*  File: zmapWindowCanvasSequence.c
 *  Author: Malcolm Hinsley (mh17@sanger.ac.uk)
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
 * implements callback functions for FeaturesetItem Sequence features
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>

#include <math.h>
#include <string.h>

#include <ZMap/zmapFeature.hpp>
#include <zmapWindowCanvasDraw.hpp>
#include <zmapWindowCanvasFeatureset_I.hpp>
#include <zmapWindowCanvasFeature_I.hpp>
#include <zmapWindowCanvasSequence_I.hpp>


static void zmapWindowCanvasSequencePaintFeature(ZMapWindowFeaturesetItem featureset, ZMapWindowCanvasFeature feature,
                                                 GdkDrawable *drawable, GdkEventExpose *expose) ;
static void zmapWindowCanvasSequencePreZoom(ZMapWindowFeaturesetItem featureset) ;
static void zmapWindowCanvasSequenceZoomSet(ZMapWindowFeaturesetItem featureset, GdkDrawable *drawable_unused) ;
static void zmapWindowCanvasSequenceFreeSet(ZMapWindowFeaturesetItem featureset) ;
static void zmapWindowCanvasSequenceSetColour(FooCanvasItem *foo,
                                              ZMapFeature feature, ZMapFeatureSubPart sub_feature,
                                              ZMapStyleColourType colour_type, int colour_flags,
                                              GdkColor *default_fill, GdkColor *default_border) ;
static ZMapFeatureSubPart zmapWindowCanvasSequenceGetSubPartSpan(FooCanvasItem *foo,
                                                                     ZMapFeature feature, double x, double y) ;




/*
 *                 External routines
 */


/* hack to test text width stuff.... */
void zMapWindowCanvasSequenceZoomSet(ZMapWindowFeaturesetItem featureset)
{
  zmapWindowCanvasSequenceZoomSet(featureset, NULL) ;

  return ;
}



/* it would have been better to make a SequenceSet struct to wrap the Pango in case we ant to add anything,
 * search for pango-> and ->opt to find all occurences
 */
void zMapWindowCanvasSequenceInit(void)
{
  gpointer funcs[FUNC_N_FUNC] = { NULL };
  gpointer feature_funcs[CANVAS_FEATURE_FUNC_N_FUNC] = { NULL };

  funcs[FUNC_PAINT]   = (void *)zmapWindowCanvasSequencePaintFeature;
  funcs[FUNC_PRE_ZOOM] = (void *)zmapWindowCanvasSequencePreZoom ;
  funcs[FUNC_ZOOM]    = (void *)zmapWindowCanvasSequenceZoomSet;
  funcs[FUNC_COLOUR]  = (void *)zmapWindowCanvasSequenceSetColour;
  funcs[FUNC_FREE]    = (void *)zmapWindowCanvasSequenceFreeSet;


  /* it would have been better to make a SequenceSet struct to wrap the Pango in case we ant to add anything,
   * search for pango-> and ->opt to find all occurences
   */
  zMapWindowCanvasFeatureSetSetFuncs(FEATURE_SEQUENCE, funcs, sizeof(zmapWindowCanvasPangoStruct)) ;


  feature_funcs[CANVAS_FEATURE_FUNC_SUBPART] = (void *)zmapWindowCanvasSequenceGetSubPartSpan ;

  zMapWindowCanvasFeatureSetSize(FEATURE_SEQUENCE, feature_funcs, sizeof(zmapWindowCanvasSequenceStruct)) ;
  

  return ;
}





/*
 *                     Internal routines
 */




static void zmapWindowCanvasSequenceGetPango(GdkDrawable *drawable, ZMapWindowFeaturesetItem featureset,
                                             ZMapWindowCanvasSequence seq)
{
  /* lazy evaluation of pango renderer */
  ZMapWindowCanvasPango pango = NULL ;

  zMapReturnIfFail(featureset) ;

  pango = (ZMapWindowCanvasPango) featureset->opt ;

  if ((pango = (ZMapWindowCanvasPango)(featureset->opt)))
    {
      if (drawable && pango->drawable && pango->drawable != drawable)
        zmapWindowCanvasFeaturesetFreePango(pango);

      if (!pango->renderer)
        {
          GdkColor *draw;

          zMapStyleGetColours(featureset->style, STYLE_PROP_COLOURS, ZMAPSTYLE_COLOURTYPE_NORMAL, NULL, &draw, NULL);

          zmapWindowCanvasFeaturesetInitPango(drawable, featureset,
                                              pango, ZMAP_ZOOM_FONT_FAMILY, ZMAP_ZOOM_FONT_SIZE, draw) ;

          seq->feature.width = zMapStyleGetWidth(featureset->style)  * pango->text_width;
        }
    }

  return ;
}


static void zmapWindowCanvasSequenceSetRow(ZMapWindowFeaturesetItem featureset, ZMapWindowCanvasSequence seq)
{
  ZMapWindowCanvasPango pango = NULL ;

  zMapReturnIfFail(featureset) ;
  pango = (ZMapWindowCanvasPango) featureset->opt;

  if(!seq->row_size)                        /* have just zoomed */
    {
      seq->start = featureset->start;
      seq->end = featureset->end;

      /* we need seq start and end to get the range, and the text height and the zoom level
       * calculate how many bases we need to display per row -> gives us row_size
       * if there are not enough for the last row then we only display what's there,
       * and these can dangle off the end if there's not enough pixels vertically
       * from max width of the column calculate if we can display all the bases and set a flag
       * set the featureset width
       */

      /* NOTE may need to move this into zoom and find the feature if we have to do a reposition
       * or set the feature on add
       */

      /* integer overflow? 20MB x 20pixels = 400M px which is ok in a long */
      long n_bases, pixels, n_rows, length;
      int width, n_disp;

      n_bases = seq->end - seq->start + 1;
      length = seq->feature.feature->feature.sequence.length;        /* is not n_bases for peptide */

      pixels = n_bases * featureset->zoom + pango->text_height - 1;        /* zoom is pixels per unit y; + fiddle factor for integer divide */
      width = (int) zMapStyleGetWidth(featureset->style);        /* characters not pixels */
      n_disp = width;                 // if pixels, divide by pango->text_width;

      seq->spacing = pango->text_height + ZMAP_WINDOW_TEXT_BORDER;        /* NB: BORDER is 0 */
      n_rows = pixels / seq->spacing;
      seq->row_size = length / n_rows;
      if(length % n_rows)        /* avoid dangling bases at the end */
        seq->row_size++;

      n_rows = length / seq->row_size;
      seq->spacing = pixels / n_rows;

      if(seq->row_size <= n_disp)
        {
          seq->row_disp = seq->row_size;
          seq->n_bases  = seq->row_size;
          seq->truncated = "";
        }
      else
        {
          seq->row_disp = n_disp;
          seq->n_bases  = n_disp - 3;
          seq->truncated = "...";
        }

      seq->factor = seq->feature.feature->feature.sequence.type == ZMAPSEQUENCE_PEPTIDE ? 3 : 1;



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      zMapDebugPrintf("%s n_bases: %ld %ld %.5f %ld, %d %d %ld = %ld %ld\n",
                      g_quark_to_string(featureset->id), n_bases, length, featureset->zoom,
                      pixels, width, n_disp, n_rows, seq->row_size, seq->spacing);
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



      /* allocate space for the text */
      if(seq->n_text <= seq->row_size)
        {
          if(seq->text)
            g_free(seq->text);
          seq->n_text = seq->row_size > MAX_SEQ_TEXT ? seq->row_size : MAX_SEQ_TEXT;        /* avoid reallocation, maybe we first do this zoomned in */
          seq->text = (char *)g_malloc(seq->n_text);
        }

      featureset->width = seq->row_disp * pango->text_width;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      /* disable for now and see what that does.... */

      /* have to do the whole canvas */
      zMapWindowRequestReposition((FooCanvasItem *) featureset);
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

    }

  return ;
}


/* paint highlights for one line of text */
static GList *zmapWindowCanvasSequencePaintHighlight(GdkDrawable *drawable, ZMapWindowFeaturesetItem featureset,
                                                     ZMapWindowCanvasSequence seq, GList *highlight,
                                                     long y, int cx, int cy)
{
  ZMapSequenceHighlight sh;
  GdkColor c;
  long start,end;
  int n_bases;
  gboolean join_left = FALSE;
  int hcx;
  gboolean done_middle = FALSE;
  int height,len;
  int n_show;                /* no of DNA bases not letters eg peptide */
  int last_paint = 0;
  ZMapWindowCanvasPango pango = NULL ;

  zMapReturnValIfFail(featureset, highlight) ;
  pango = (ZMapWindowCanvasPango) featureset->opt;

  while (highlight)
    {
      /* each highlight struct has start and end seq coords and a colour */
      /* there may be more than one on a line eg w/ split codon */

      sh = (ZMapSequenceHighlight) highlight->data;
      //zMapDebugPrintf("3FT highlight %ld (%ld) @ %x, %ld, %ld\n",y, seq->row_size, sh->type, sh->start, sh->end);

      if(sh->end < y)
        {
          highlight = highlight->next;        /* this highlight is upstream of this line */
          continue;
        }
      else
        {
          if(sh->start >= y + seq->row_size * seq->factor)        /* this highlight is downstream of this line */
            break;
        }

      /* this highlight is on this line */

      c.pixel = sh->colour;
      gdk_gc_set_foreground (featureset->gc, &c);

      join_left = FALSE;

      /* paint bases actually visible */
      start = sh->start;
      end = sh->end;
      if(start < y)
        start = y;
      n_bases = seq->n_bases * seq->factor;

      if(start < y + n_bases)
        {
          if(end >= y + n_bases)
            {
              end  = y + n_bases - 1;
              join_left = TRUE;
            }

          n_bases = end - start + 1;
          n_show = (n_bases  + seq->factor - 1) / seq->factor;
          /* now start and end are the DNA coords to show, n_bases is how many to interpret, n_show is how many to display */

          hcx = cx + ((start - y)  / seq->factor ) * pango->text_width;        /* we need integer truncation from this divide before the multiply */

          height = seq->spacing;
          len = n_bases;        // sh->end - sh->start + 1;

          if(seq->row_disp == 1 && seq->feature.feature->feature.sequence.type == ZMAPSEQUENCE_PEPTIDE && len < seq->factor)
            {
              /* peptide covering more than one base on display vertically, if split codon then limit size */
              /* there cannot be an overflow region in the next if { block} */
#if 0
              if(sh->type == ZMAPFEATURE_SUBPART_SPLIT_5_CODON)        /* start of exon */
                {
                  height *= len;
                  height /= seq->factor;
                  cy += seq->spacing - height;
                }
              else  if(sh->type == ZMAPFEATURE_SUBPART_SPLIT_3_CODON)        /* end of exon */
                {
                  height *= len;
                  height /= seq->factor;
                }
              else
#endif
                {
                  /* split codons need to be adjusted but non coding (& out of frame coding) also need to be */

                  if(start > y)
                    cy += (start - y) * height / seq->factor;
                  height = (height * len) / seq->factor;
                }
            }
          //zMapDebugPrintf("3FT paint sel %x %ld,%ld @ %ld %d %d,(%ld,%ld %d %d) %d\n", sh->type, sh->start, sh->end, start - y, cy, hcx, start, end, n_bases, n_show, len);

          /* NOTE
           * we paint ascending seq coords. If we have a 3' split codon (which is at the 5' end of an exon)
           * and there is more than 1 residue on a line then we could overwrite a split codon with coding
           * as these highlights could appear it the same character cell.
           * split codons are important, so we don't overwrite these
           * implementation is by detecting highlights of < 3 bases and same x coordinate
           * no state needs to be kept by the caller as this is contained within one line, which this is what function does.
           * 5' split cocdons could conceivably fail to overwrite a whole residue previous,
           * but we only trigger this if the highlight is less than 3 bases, which can't happen
           */

          if(hcx == last_paint)
            {
              /* shrink our rectangle to not overwrite */

              if(n_show > 1)        /* must be more than one base on the line, spacing = 1 char */
                {
                  n_show--;
                  hcx += pango->text_width;
                }
#if 0
              else if(seq->spacing > pamgo->text_height)
                {
                  /* this gets complicated as the split codons in an out of frame column puts us in mid codon for coding */
                  /* so, as we display these as for coding it does not matter if we overwrite a highlight */
                  /* yes, it's a bodge */
                }
#endif
            }
          last_paint = 0;
          if(len < seq->factor)
            last_paint = hcx;

          if(n_show > 0)
            gdk_draw_rectangle (drawable, featureset->gc, TRUE, hcx, cy, pango->text_width * n_show, height + 1);

        }

      if(*seq->truncated && sh->end > y + seq->n_bases * seq->factor)
        {
          /* paint bases in the ... */

          hcx = cx + seq->n_bases * pango->text_width;

          n_bases = strlen(seq->truncated);
          if(!join_left)
            {
              n_bases--;
              hcx += pango->text_width;
            }
          if(sh->end < y + seq->row_size)
            {
              n_bases--;
            }

          /* NOTE there are some combinations of highlight we will cannot show in the ..., but let's try */
          else if(!join_left && done_middle)        /* shrink 2 char blob to one on the right */
            {
              n_bases--;
              hcx += pango->text_width;
            }

          if(!join_left)
            done_middle = TRUE;        /* likely to be a split codon */

          //zMapDebugPrintf("3FT paint ... @ %ld %d, %d (%d)\n", y, cy, cx, n_bases);

          gdk_draw_rectangle (drawable, featureset->gc, TRUE, hcx, cy, pango->text_width * n_bases, seq->spacing + 1);
        }

      if(sh->end >= y + seq->row_size * seq->factor)        /* there can be no more on this line, return current position for retry on next line */
        return highlight;

      highlight = highlight->next;        /* try the next one */
    }


  return highlight;
}



/* calculate strings on demand and run them through pango
 * for the example code this was derived from look at:
 * http://developer.gnome.org/gdk/stable/gdk-Pango-Interaction.html
 */
static void zmapWindowCanvasSequencePaintFeature(ZMapWindowFeaturesetItem featureset, ZMapWindowCanvasFeature feature,
                                                 GdkDrawable *drawable, GdkEventExpose *expose)
{
  FooCanvasItem *foo = (FooCanvasItem *) featureset;
  double y1, y2;
  ZMapSequence sequence = NULL ;
  ZMapWindowCanvasSequence seq = (ZMapWindowCanvasSequence) feature;
  int cx, cy ;
  long seq_y1, seq_y2, y;
  long y_base, y_paint;                /* index of base or peptide to draw */
  int nb;
  GList *hl;        /* iterator through highlight */
  ZMapFrame frame ;
  ZMapWindowCanvasPango pango = NULL ;

  zMapReturnIfFail(featureset && feature && feature->feature) ;

  pango = (ZMapWindowCanvasPango) featureset->opt ;
  sequence = &feature->feature->feature.sequence;
  frame = zMapFeatureFrame(feature->feature);

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  GtkAdjustment *adjust ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



  zmapWindowCanvasSequenceGetPango(drawable, featureset, seq);

  zmapWindowCanvasSequenceSetRow(featureset, seq);

  //zMapDebugPrintf("Seq paint %s index= %p\n", g_quark_to_string(featureset->id), featureset->display_index);

  /* restrict to actual expose area, any expose will fetch the whole feature */

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* restrict to actual expose area, any expose will fetch the whole feature */
  /* OK, expose area may not be whole window if we have scrolled to part of the canvas
   * that is outside the existing window.....so get size of window and use that if
   * expose is less..... */

  adjust = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(featureset->canvas_scrolled_window)) ;
  if (expose->area.height < adjust->page_size)
    expose->area.height = adjust->page_size ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



  cx = expose->area.y - 1;
  cy = expose->area.y + expose->area.height + 1 ;
  if(cx < featureset->clip_y1)
    cx = featureset->clip_y1 - seq->spacing + 1 ;
  if(cy > featureset->clip_y2)
    cy = featureset->clip_y2 + seq->spacing - 1 ;



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  zMapDebugPrintf("\n\nExpose: %d %d (%d %d) -> %d %d\n",
                  expose->area.y - 1, expose->area.y + expose->area.height + 1,
                  featureset->clip_y1, featureset->clip_y2,
                  cx, cy) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



  /* get the expose area: copied from calling code, we have one item here and it's normally bigger
   * than the expose area */
  foo_canvas_c2w(foo->canvas, 0, floor(cx), NULL, &y1) ;
  foo_canvas_c2w(foo->canvas, 0, ceil(cy), NULL, &y2) ;




#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  zMapDebugPrintf("Paint from %.1f to %.1f\n", y1, y2) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */




  //        NOTE need to sort highlight list here if it's not added in ascending coord order */

  /* find and draw all the sequence data within */
  /* we act dumb and just display each row complete, let X do the clipping */

  /* bias coordinates to stable ones , find the first seq coord in this row */
  seq_y1 = (long) y1  - featureset->dy + 1;
  seq_y2 = (long) y2  - featureset->dy + 1;

  if(seq->row_disp == 1 && seq->factor > 1)
    {
      /* bias backwards as we bias fwds later */
      seq_y1-= frame - ZMAPFRAME_0;
      seq_y2 -= frame - ZMAPFRAME_0;
    }

  if(seq_y1 < 1)                        /* sequence coords are 1 based */
    seq_y1 = 1;

  y = seq_y1;
  y -= 1;
  y -=  y % (seq->row_size  * seq->factor);                /* sequence offset from start, 0 based, biased to row start */

  /*  zMapDebugPrintf("Seq %s frame = %d y1, = %.1f
      (%ld)\n",g_quark_to_string(featureset->id),sequence->frame, y1, y); */

  seq->offset = (seq->spacing - pango->text_height) / 2;                /* vertical padding if relevant */

  for(hl = seq->highlight; y < seq_y2; y += seq->row_size * seq->factor)
    {
      char *p,*q;
      int i;

      frame = zMapFeatureFrameAtCoord(feature->feature, y + feature->y1);

      y_base = y / seq->factor;
      if(y_base >= sequence->length)
        break;

      y_paint = y_base * seq->factor;

      if(frame)
        y_paint += frame - ZMAPFRAME_0;


      //if(sequence->frame == ZMAPFRAME_2) zMapDebugPrintf("3FT y, seq: %ld (%ld %ld) start,end %ld %ld, ybase %ld\n",y, seq_y1,seq_y2,seq->start, seq->end, y_base);

      p = sequence->sequence + y_base;
      q = seq->text;

      nb = seq->n_bases;
      if(sequence->length - y_base < nb)
        nb = sequence->length - y_base;

      for(i = 0;i < nb; i++)
        *q++ = *p++;        // & 0x5f; original code did this lower cased, peptides are upper

      strcpy(q,seq->truncated);        /* may just be a null */
      while (*q)
        q++;

      pango_layout_set_text(pango->layout, seq->text, q - seq->text);        /* be careful of last short row */

      /* need to get pixel coordinates for pango */
      foo_canvas_w2c (foo->canvas, featureset->dx, y_paint + featureset->dy, &cx, &cy);

      /* NOTE y is 0 based so we have to add 1 to do the highlight properly */
      hl = zmapWindowCanvasSequencePaintHighlight(drawable, featureset, seq, hl, y_paint + featureset->start, cx, cy);

      //if(sequence->frame == ZMAPFRAME_2) zMapDebugPrintf("3FT paint dna %s @ %ld = %d (%ld)\n",seq->text, y_paint, cy, seq->offset);

      pango_renderer_draw_layout(pango->renderer, pango->layout,
                                 cx * PANGO_SCALE, (cy + seq->offset) * PANGO_SCALE) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      zMapDebugPrintf("text %s at %ld, canvas %.1f, %.1f = %d, %d \n",
                      seq->text, y, featureset->dx, y + featureset->dy, cx, cy) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

    }

  return ;
}

static void zmapWindowCanvasSequencePreZoom(ZMapWindowFeaturesetItem featureset)
{

  /* Need to call routine to calculate zoom for text here... */
  zMapWindowCanvasSequenceZoomSet(featureset) ;

  return ;
}



/* I don't know why drawable is passed in here....check other canvas types to see how it's used.... */
static void zmapWindowCanvasSequenceZoomSet(ZMapWindowFeaturesetItem featureset, GdkDrawable *drawable)
{
  ZMapWindowCanvasSequence seq ;
  ZMapSkipList sl ;

  ZMapWindowCanvasPango pango = (ZMapWindowCanvasPango) featureset->opt;

  if (!pango)
    return;

  /* calculate bases per line */
  //        zMapDebugPrintf("sequence zoom (%.5f)\n", featureset->zoom);        /* is pixels per unit y */

  /* ummmm..... previous implemmentation is quite complex, let-s just do it from scratch */

  /* ummmm.........this is the place where we need to calculate
   * bases per line etc etc....you broke it malcolm...... */


  /* I'M NOT SURE WHY THERE'S A LOOP HERE....IS IT FOR PEPTIDES....??? */
  for (sl = zMapSkipListFirst(featureset->display_index) ; sl ; sl = sl->next)
    {
      seq = (ZMapWindowCanvasSequence)(sl->data) ;

      zmapWindowCanvasSequenceGetPango(drawable, featureset, seq);



      seq->row_size = 0 ;                                    /* trigger recalc, can't do it here as
                                                               we don't have the seq feature, or a drawable  */

      /* try calc here.....in fact this is too late..... */
      zmapWindowCanvasSequenceSetRow(featureset, seq) ;
    }

  //zMapDebugPrintf("3FT zoom %s index= %p\n",g_quark_to_string(featureset->id), featureset->display_index);

  return ;
}


static void zmapWindowCanvasSequenceFreeHighlight(ZMapWindowCanvasSequence seq)
{
  GList *l = NULL ;

  zMapReturnIfFail(seq) ;

  /* free any highlight list */

  for(l = seq->highlight; l ; l = l->next)
    {
      ZMapSequenceHighlight sh;

      sh = (ZMapSequenceHighlight) l->data;
      /* expose */

      g_free(sh);
    }
  g_list_free(seq->highlight);
  seq->highlight = NULL;

  return ;
}



static void zmapWindowCanvasSequenceFreeSet(ZMapWindowFeaturesetItem featureset)
{
  ZMapWindowCanvasSequence seq ;
  ZMapSkipList sl = NULL ;
  ZMapWindowCanvasPango pango = NULL ;

  zMapReturnIfFail(featureset) ;
  pango = (ZMapWindowCanvasPango) featureset->opt;

  /* nominally there is only one feature in a seq column, but we could have 3 franes staggered.
   * besides, this data is in the feature not the featureset so we have to iterate regardless
   * NOTE the stagger is done in the FeaturesetItem, so we can have only one seq in this featureset
   * so we expect only one iteration in this loop
   */
  for(sl = zMapSkipListFirst(featureset->display_index); sl; sl = sl->next)
    {
      seq = (ZMapWindowCanvasSequence) sl->data;

      zmapWindowCanvasSequenceFreeHighlight(seq);
    }

  if(pango)
    zmapWindowCanvasFeaturesetFreePango(pango);

  return ;
}




static void zmapWindowCanvasSequenceSetColour(FooCanvasItem *foo,
                                              ZMapFeature feature, ZMapFeatureSubPart sub_feature,
                                              ZMapStyleColourType colour_type, int colour_flags,
                                              GdkColor *default_fill, GdkColor *default_border)
{
  ZMapWindowCanvasSequence seq;
  ZMapWindowFeaturesetItem fi = (ZMapWindowFeaturesetItem) foo;

  zMapReturnIfFail(foo) ;

  /* based on colour_type:
   * set highlight region from span if SELECTED
   * set background if NORMAL
   * remove highlight if INVALID
   */

  /* NOTE this function always adds a highlight. To replace one, deselect the highlight first.
   * We don't ever expect to want to change a highlight slightly.
   * NOTE must add these in seq coord order, if not must sort before paint
   */

  ZMapSequenceHighlight h;
  gulong colour_pixel;

  ZMapSkipList sl;

  //zMapDebugPrintf("3FT set colour %s index= %p\n",g_quark_to_string(fi->id), fi->display_index);

  if((colour_type != ZMAPSTYLE_COLOURTYPE_INVALID) && !default_fill)                /* eg if not configured in the style */
    return;
  // if(!default_border) return;        not actually used

  /* nominally there is only one feature in a seq column, but we could have 3 franes staggered.
   * besides, this data is in the feature not the featureset so we have to iterate regardless
   */
  for(sl = zMapSkipListFirst(fi->display_index); sl; sl = sl->next)
    {
      seq = (ZMapWindowCanvasSequence) sl->data;

      switch(colour_type)
        {
        case ZMAPSTYLE_COLOURTYPE_INVALID:
          zmapWindowCanvasSequenceFreeHighlight(seq);
          break;

        case ZMAPSTYLE_COLOURTYPE_NORMAL:
          colour_pixel = zMap_gdk_color_to_rgba(default_fill);
          colour_pixel = foo_canvas_get_color_pixel(foo->canvas, colour_pixel);

          seq->background = colour_pixel;
          break;

        case ZMAPSTYLE_COLOURTYPE_SELECTED:

          /* zmapStyleSetColour() according to sub_part->subpart */
          colour_pixel = zMap_gdk_color_to_rgba(default_fill);
          colour_pixel = foo_canvas_get_color_pixel(foo->canvas, colour_pixel);

          h = g_new0(zmapSequenceHighlightStruct, 1);
          h->colour = colour_pixel;
          h->type = sub_feature->subpart;

          /* this must match the sequence coords calculated for painting text */
          /* which are zero based relative to seq start coord and featureset offset (dy) */
          h->start = sub_feature->start;        // - fi->dy + 1;
          h->end = sub_feature->end;                // - fi->dy + 1;
          //zMapDebugPrintf("3FT set highlight for %d,%d @ %ld, %ld\n",sub_feature->start, sub_feature->end, h->start,h->end);
          seq->highlight = g_list_append(seq->highlight, h);
          break;
        }
    }

  return ;
}



static ZMapFeatureSubPart zmapWindowCanvasSequenceGetSubPartSpan(FooCanvasItem *foo,
                                                                 ZMapFeature feature, double x, double y)
{
  ZMapFeatureSubPart sub_part = NULL ;

  /*! \todo #warning revisit this when canvas items are simplified */

  sub_part = g_new0(ZMapFeatureSubPartStruct, 1) ;
  sub_part->start = y;
  sub_part->end = y;
  sub_part->index = 1;
  sub_part->subpart = ZMAPFEATURE_SUBPART_INVALID;

  return sub_part;
}


/* Wrapper for zMapWindowCanvasFeaturesetGetSeqCoord, taking a generic feature. Returns the same as
* that function if feature is a ZMapWindowFeaturesetItem, else does nothing and returns FALSE. */
gboolean zMapWindowItemGetSeqCoord(FooCanvasItem *item, gboolean set, double x, double y, long *seq_start, long *seq_end)
{
  gboolean result = FALSE;

  if (ZMAP_IS_WINDOW_FEATURESET_ITEM(item))
    {
      result = zMapWindowCanvasFeaturesetGetSeqCoord((ZMapWindowFeaturesetItem)(item), set, x, y, seq_start, seq_end);
    }

  return result;
}


/* return sequence coordinate corresponding to a mouse cursor */
/* NOTE unusually, we call the function directly rather than going through an array of functions
 * it's only relevant for sequence features, but could be added as a virtual function/wrapper
 * in ZWCFS.c if we care about code purity
 */
/* if it's a peptide column then we bias coordinates to cover whole residues,
 * taking into account start and end wobble */
gboolean zMapWindowCanvasFeaturesetGetSeqCoord(ZMapWindowFeaturesetItem featureset, gboolean set,
                                               double x, double y, long *seq_start, long *seq_end)
{

  /* get y coordinate as left hand base/ residue of current row */
  static ZMapWindowCanvasSequence seq = NULL;
  static long seq_origin;                /* is fixed once set */
  ZMapSkipList sl = NULL ;
  long seq_y, seq_x;
  ZMapWindowCanvasPango pango = NULL;

  zMapReturnValIfFail(featureset, FALSE) ;
  pango = (ZMapWindowCanvasPango) featureset->opt;

  if(!ZMAP_IS_WINDOW_FEATURESET_ITEM(featureset))
    return FALSE;
  if(zMapStyleGetMode(featureset->style) != ZMAPSTYLE_MODE_SEQUENCE)
    return FALSE;

  /* first button press: need to find the seq canvas feature to base our coords on */
  /* could conceivably have 3 features in a column offset by index
   * NOTE the stagger is done in the FeaturesetItem, so we can have only one seq in this featureset
   */
  if(set) for(sl = zMapSkipListFirst(featureset->display_index); sl; sl = sl->next)
    {
      seq = (ZMapWindowCanvasSequence) sl->data;
      //                if(x >= featureset->x_off + featureset->dx && x <= featureset->x_off + featureset->dx + featureset->width)
      break;
    }

  if(!seq)
    {
      zMapWarnIfReached() ;
      return FALSE;        /* should not happen */
    }

  /* get y coordinate as left hand base/ residue of current row */

  /* bias coordinates to stable ones , find the first seq coord in this row */
  seq_y = (long) (y - featureset->dy) + 1;

  //zMapDebugPrintf("seq coord %.1f, %.1f, %.1f -> %ld\n",x,y,featureset->dy, seq_y);

  ZMapFrame frame = featureset->frame;
  ZMapFeature feature = zmapWindowItemGetFeature((FooCanvasItem*)featureset) ;

  if (frame == ZMAPFRAME_NONE)
    {
      /* We get here for the Show Translation column, where all 3 frames are in 
       * the same column. We need to know which exon we're in to find the frame 
       * for this particular section, but that's difficult because we can't work
       * out exactly which coord we're at until we know the frame! For now just find 
       * the closest exon and hope that's good enough. */
      frame = zMapFeatureFrameAtCoord(feature, seq_y + seq->start - 1) ;

      if (frame == ZMAPFRAME_NONE)
        {
          zMapDebugPrintf("Could not find frame for Translation at coordinate %d", seq_y);
          frame = ZMAPFRAME_0;
        }
    }


  seq_y -= 1;                                                    /* zero base */
  if(seq->factor > 1)                                            /* bias to first base of residue */
    {
      seq_y -= frame - ZMAPFRAME_0;                    /* remove frame offset */
      seq_y -= seq_y % seq->factor;                            /* bias to start of residue */
    }

  if(seq_y < 0)                        /* sequence coords are 1 based */
    return FALSE;

  //zMapDebugPrintf("seq_y (framed) = %ld\n",seq_y);

  seq_y -=  seq_y % (seq->row_size * seq->factor);            /* sequence offset from start, 0
                                                               based, biased to row start */

  seq_y += 1;                                                    /* start of this row in seq coords,
                                                               as we have 1-based coordinates */
  if(seq->factor > 1)
    seq_y += frame - ZMAPFRAME_0;                    /* back to frame offset coords */

  //zMapDebugPrintf("row coord %ld\n", seq_y);


  /* adjust by x coordinate */
  seq_x = (long) (x - featureset->x_off - featureset->dx);

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  zMapDebugPrintf("x,wid = %.1f,%d\n",x,pango->text_width);
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  seq_x /= pango->text_width;

  if(seq_x < 0)
    seq_x = 0;
  if(seq_x >= seq->row_disp)
    seq_x = seq->row_disp - 1;

  seq_x *= seq->factor;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  zMapDebugPrintf("x,row_disp,size: %ld %ld %ld\n",seq_x,seq->row_disp,seq->row_size);
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



  if(seq_x < seq->row_size * seq->factor - 1)        /* first 2 dots are just bases */
    seq_y += seq_x;
  else
    seq_y += seq->row_size * seq->factor - 1;        /* select whole row */


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  zMapDebugPrintf("seq_y = %ld %d\n",seq_y,frame);
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  /* now we have seq coords, adjust the cursor to the start of the selected residue */
  seq_y += featureset->start - 1;

  int phase_offset = zMapFeatureSplitCodonOffset(feature, seq_y) ;
  seq_y -= phase_offset ;

  if(set)
    {
      *seq_start = seq_origin = seq_y;
      *seq_end = seq_y + seq->factor - 1;
    }
  else
    {
      //zMapDebugPrintf("seq,start,end: %ld, %ld, %ld\n", seq_y, *seq_start, *seq_end);

      if(seq_y < seq_origin)
        {
          *seq_end = seq_origin + seq->factor - 1;
          *seq_start = seq_y;
        }
      else
        {
          *seq_start = seq_origin;
          *seq_end = seq_y + seq->factor - 1;
        }
    }

  return TRUE;
}



