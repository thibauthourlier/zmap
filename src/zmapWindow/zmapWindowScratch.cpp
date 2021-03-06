/*  File: zmapWindowScratch.c
 *  Author: Gemma Barson (gb10@sanger.ac.uk)
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
 * Description: Implements the Annotation column in zmap. This is a
 *              column where the user can create and edit temporary
 *              gene model objects.
 *
 * Exported functions: See zmapWindow_P.h
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>

#include <ZMap/zmapUtils.hpp>
#include <ZMap/zmapFeature.hpp>
#include <zmapWindow_P.hpp>        /* ZMapWindow */
#include <zmapWindowScratch_P.hpp>



/*!
 * \brief Get the single featureset that resides in the scratch column
 *
 * \returns The ZMapFeatureSet, or NULL if the column doesn't exist
 */
ZMapFeatureSet zmapWindowScratchGetFeatureset(ZMapWindow window)
{
  ZMapFeatureSet feature_set = NULL;

  if (window && window->context_map)
    {
      GQuark column_id = zMapFeatureSetCreateID(ZMAP_FIXED_STYLE_SCRATCH_NAME);
      GList *fs_list = window->context_map->getColumnFeatureSets(column_id, TRUE);

      /* There should be one (and only one) featureset in the column */
      if (g_list_length(fs_list) > 0)
        {
          GQuark set_id = (GQuark)(GPOINTER_TO_INT(fs_list->data));
          feature_set = zmapFeatureContextGetFeaturesetFromId(window->feature_context, set_id);
        }
    }

  return feature_set;
}


/*!
 * \brief Get the single feature that resides in the scratch column
 *
 * \returns The ZMapFeature, or NULL if the column doesn't exist
 */
ZMapFeature zmapWindowScratchGetFeature(ZMapWindow window)
{
  ZMapFeature feature = NULL ;
  ZMapFeatureSet feature_set = zmapWindowScratchGetFeatureset(window) ;

  if (feature_set)
    {
      ZMapFeatureAny feature_any = (ZMapFeatureAny)feature_set;

      GHashTableIter iter;
      gpointer key, value;

      g_hash_table_iter_init (&iter, feature_any->children);

      /* Should only be one, so just get the first */
      while (g_hash_table_iter_next (&iter, &key, &value) && !feature)
        {
          feature = (ZMapFeature)(value);
        }
    }

  return feature;
}


static void scratchRecalcTranslation(ZMapWindow window)
{
  ZMapFeatureSet scratch_featureset = zmapWindowScratchGetFeatureset(window) ;

  if (window->show_translation_featureset_id &&
      window->show_translation_featureset_id == scratch_featureset->unique_id)
    {
      ZMapFeature scratch_feature = zmapWindowScratchGetFeature(window) ;
      zmapWindowFeatureShowTranslation(window, scratch_feature) ;
    }

}

/* Do the callback to the View level for a command on the scratch column */
static void doScratchCallbackCommand(ZMapWindowCommandType command_type,
                                     ZMapWindow window,
                                     ZMapFeature feature,
                                     FooCanvasItem *item,
                                     const double world_x,
                                     const double world_y,
                                     const gboolean use_subfeature)
{
  gboolean ok = TRUE ;
  ZMapWindowCallbackCommandScratch scratch_cmd = g_new0(ZMapWindowCallbackCommandScratchStruct, 1) ;

  /* Set up general command field for callback. */
  scratch_cmd->cmd = command_type ;
  scratch_cmd->seq_start = 0;
  scratch_cmd->seq_end = 0;
  scratch_cmd->subpart = NULL;
  scratch_cmd->use_subfeature = use_subfeature ;

  if (feature && (use_subfeature || !(window && window->focus)))
    {
      /* Just use the single feature that the user right-clicked on */
      scratch_cmd->features = g_list_append(scratch_cmd->features, feature) ;
    }
  else if (window && window->focus)
    {
      /* Add all selected features. Note that these may be different to the feature the user
       * actually right-clicked on! */
      GList *glist = zmapWindowFocusGetFeatureList(window->focus) ;
      scratch_cmd->features = g_list_concat(scratch_cmd->features, glist) ;
    }

  if (feature && feature->mode == ZMAPSTYLE_MODE_SEQUENCE)
    {
      /* For sequence features, get the sequence coord that was clicked */
      zMapWindowItemGetSeqCoord(item, TRUE, world_x, world_y, &scratch_cmd->seq_start, &scratch_cmd->seq_end) ;
    }

  /* If necessary, find the subfeature and set it in the data. What constitutes a
   * "subfeature" depends on the feature type: for transcripts, it's an intron/exon. For
   * alignments, it's a match block, which is actually the whole ZMapFeature (and the
   * "entire" feature therefore includes the linked match blocks from the same alignment).  */
  if (feature && use_subfeature && feature->mode == ZMAPSTYLE_MODE_TRANSCRIPT)
    {
      zMapWindowCanvasItemGetInterval((ZMapWindowCanvasItem)item, world_x, world_y, &scratch_cmd->subpart) ;

      if (!scratch_cmd->subpart)
        {
          ok = FALSE ;
          zMapWarning("Cannot find subfeature for feature '%s' at position %f,%f",
                      g_quark_to_string(feature->original_id), world_x, world_y) ;
        }
    }

  if (ok)
    {
      /* Call back to the View to update all windows */
      ZMapWindowCallbacks window_cbs_G = zmapWindowGetCBs() ;
      (*(window_cbs_G->command))(window, window->app_data, scratch_cmd) ;

      /* If the translation is being shown for the scratch feature, recalculate it */
      scratchRecalcTranslation(window) ;
    }
  else
    {
      g_list_free(scratch_cmd->features) ;
      g_free(scratch_cmd) ;
      scratch_cmd = NULL ;
    }
}


/* If evidence features for the scratch column are highlighted, unhighlight
 * them. We need to do this before copying features into the scratch column because otherwise we
 * would re-copy all of the existing features. It's a pain for the user to have to hide evidence
 * before copying and then highlight it again afterwards, which is why we do this automatically
 */
static void scratchHideEvidence(ZMapWindow window)
{
  zMapReturnIfFail(window) ;

  ZMapWindowFocus focus = window->focus;

  if (focus && window->highlight_evidence_featureset_id == zMapStyleCreateID(ZMAP_FIXED_STYLE_SCRATCH_NAME))
    {
      zmapWindowFocusResetType(focus,WINDOW_FOCUS_GROUP_EVIDENCE);
    }

  return ;
}


/* If evidence was highlighted for the scratch column, re-highlight it after an edit. This makes
 * sure any newly copied features are included and is also necessary because we hide evidence
 * with scratchHideEvidence before a copy.  */
static void scratchHighlightEvidence(ZMapWindow window)
{
  zMapReturnIfFail(window) ;

  if (window->highlight_evidence_featureset_id == zMapStyleCreateID(ZMAP_FIXED_STYLE_SCRATCH_NAME))
    {
      ZMapFeature scratch_feature = zmapWindowScratchGetFeature(window) ;

      ZMapWindowHighlightData highlight_data = g_new0(ZMapWindowHighlightDataStruct, 1) ;
      highlight_data->window = window ;
      highlight_data->feature = (ZMapFeatureAny)scratch_feature ;

      zmapWindowScratchFeatureGetEvidence(window, scratch_feature, zmapWindowHighlightEvidenceCB, highlight_data) ;
    }

  return ;
}



/*!
 * \brief Copy the given feature to the scratch column
 *
 * \param window
 * \param feature The new feature to be copied in
 * \param item The foo canvas item for the new feature
 * \param world_x The clicked x coord
 * \param world_y The clicked y coord
 * \param merge_subfeature If true, just merge the clicked subfeature, otherwise merge the whole feature
 */
void zmapWindowScratchCopyFeature(ZMapWindow window,
                                  ZMapFeature feature,
                                  FooCanvasItem *item,
                                  const double world_x,
                                  const double world_y,
                                  const gboolean use_subfeature)
{
  if (window)
    {
      scratchHideEvidence(window) ;

      doScratchCallbackCommand(ZMAPWINDOW_CMD_COPYTOSCRATCH,
                               window,
                               feature,
                               item,
                               world_x,
                               world_y,
                               use_subfeature) ;

      scratchHighlightEvidence(window) ;
    }
}


/*!
 * \brief Copy the given feature to the scratch column
 *
 * \param window
 * \param feature The new feature to be copied in
 * \param item The foo canvas item for the new feature
 * \param world_x The clicked x coord
 * \param world_y The clicked y coord
 * \param merge_subfeature If true, just merge the clicked subfeature, otherwise merge the whole feature
 */
void zmapWindowScratchSetCDS(ZMapWindow window, 
                             ZMapFeature feature, 
                             FooCanvasItem *item, 
                             const double world_x,
                             const double world_y,
                             const gboolean use_subfeature,
                             const gboolean set_cds_start,
                             const gboolean set_cds_end)
{
  if (window)
    {
      scratchHideEvidence(window) ;

      ZMapWindowCommandType cmd = ZMAPWINDOW_CMD_INVALID ;

      if (set_cds_start && set_cds_end)
        cmd = ZMAPWINDOW_CMD_SETCDSRANGE ;
      else if (set_cds_start)
        cmd = ZMAPWINDOW_CMD_SETCDSSTART ;
      else if (set_cds_end)
        cmd = ZMAPWINDOW_CMD_SETCDSEND ;

      if (cmd != ZMAPWINDOW_CMD_INVALID)
        {
          doScratchCallbackCommand(cmd, 
                                   window,
                                   feature, 
                                   item, 
                                   world_x,
                                   world_y,
                                   use_subfeature) ;

          scratchHighlightEvidence(window) ;
        }
      else
        {
          zMapLogWarning("%s", "Called zmapWindowScratchSetCDS but did not specify start/end/range") ;
        }
    }
}


/*!
 * \brief Delete the subfeature at the given feature's coords from the scratch column
 *
 * \param window
 * \param feature The new feature to be copied in
 * \param item The foo canvas item for the new feature
 * \param world_x The clicked x coord
 * \param world_y The clicked y coord
 * \param merge_subfeature If true, just merge the clicked subfeature, otherwise merge the whole feature
 */
void zmapWindowScratchDeleteFeature(ZMapWindow window,
                                    ZMapFeature feature,
                                    FooCanvasItem *item,
                                    const double world_x,
                                    const double world_y,
                                    const gboolean use_subfeature)
{
  if (window)
    {
      doScratchCallbackCommand(ZMAPWINDOW_CMD_DELETEFROMSCRATCH,
                               window,
                               feature,
                               item,
                               world_x,
                               world_y,
                               use_subfeature) ;
    }
}


/*!
 * \brief Clear the scratch column
 */
void zmapWindowScratchClear(ZMapWindow window)
{
  if (!window)
    return;

  scratchHideEvidence(window) ;

  zmapWindowScratchResetAttributes(window) ;

  /* Call the callback to the view to redraw everything */
  ZMapWindowCallbacks window_cbs_G = zmapWindowGetCBs() ;
  ZMapWindowCallbackCommandScratch scratch_cmd = g_new0(ZMapWindowCallbackCommandScratchStruct, 1) ;

  /* Set up general command field for callback. */
  scratch_cmd->cmd = ZMAPWINDOW_CMD_CLEARSCRATCH ;

  (*(window_cbs_G->command))(window, window->app_data, scratch_cmd) ;
}


/*!
 * \brief Undo the last copy into the scratch column
 */
void zmapWindowScratchUndo(ZMapWindow window)
{
  if (!window)
    return;

  scratchHideEvidence(window) ;

  /* Call the callback to the view to redraw everything */
  ZMapWindowCallbacks window_cbs_G = zmapWindowGetCBs() ;
  ZMapWindowCallbackCommandScratch scratch_cmd = g_new0(ZMapWindowCallbackCommandScratchStruct, 1) ;

  /* Set up general command field for callback. */
  scratch_cmd->cmd = ZMAPWINDOW_CMD_UNDOSCRATCH ;

  (*(window_cbs_G->command))(window, window->app_data, scratch_cmd) ;

  scratchHighlightEvidence(window) ;

  /* If the translation is being shown for the scratch feature, recalculate it */
  scratchRecalcTranslation(window) ;
}


/*!
 * \brief Redo the last operation
 */
void zmapWindowScratchRedo(ZMapWindow window)
{
  if (!window)
    return;

  scratchHideEvidence(window) ;

  /* Call the callback to the view to redraw everything */
  ZMapWindowCallbacks window_cbs_G = zmapWindowGetCBs() ;
  ZMapWindowCallbackCommandScratch scratch_cmd = g_new0(ZMapWindowCallbackCommandScratchStruct, 1) ;

  /* Set up general command field for callback. */
  scratch_cmd->cmd = ZMAPWINDOW_CMD_REDOSCRATCH ;

  (*(window_cbs_G->command))(window, window->app_data, scratch_cmd) ;

  scratchHighlightEvidence(window) ;

  /* If the translation is being shown for the scratch feature, recalculate it */
  scratchRecalcTranslation(window) ;
}


/*!
 * \brief Get evidence feature names from the scratch column and call the given callback with them.
 */
void zmapWindowScratchFeatureGetEvidence(ZMapWindow window, ZMapFeature feature,
                                         ZMapWindowGetEvidenceCB evidence_cb, gpointer evidence_cb_data)
{
  zMapReturnIfFail(window && feature && evidence_cb) ;

  /* Call the callback to the view */
  ZMapWindowCallbacks window_cbs_G = zmapWindowGetCBs() ;
  ZMapWindowCallbackCommandScratch scratch_cmd = g_new0(ZMapWindowCallbackCommandScratchStruct, 1) ;

  /* Set up general command field for callback. */
  scratch_cmd->cmd = ZMAPWINDOW_CMD_GETEVIDENCE ;
  scratch_cmd->evidence_cb = evidence_cb ;
  scratch_cmd->evidence_cb_data = evidence_cb_data ;

  (*(window_cbs_G->command))(window, window->app_data, scratch_cmd) ;
}


/*
 * \brief Save the feature name to be used when we create the real feature from the scratch feature.
 */
void zmapWindowScratchSaveFeature(ZMapWindow window, GQuark feature_id)
{
  window->int_values[ZMAPINT_SCRATCH_ATTRIBUTE_FEATURE] = feature_id ;
}


/*
 * \brief Save the featureset to be used when we create the real feature from the scratch feature.
 */
void zmapWindowScratchSaveFeatureSet(ZMapWindow window, GQuark feature_set_id)
{
  window->int_values[ZMAPINT_SCRATCH_ATTRIBUTE_FEATURESET] = feature_set_id ;
}


/*
 * \brief Reset the saved attributes
 */
void zmapWindowScratchResetAttributes(ZMapWindow window)
{
  window->int_values[ZMAPINT_SCRATCH_ATTRIBUTE_FEATURE] = 0 ;
  window->int_values[ZMAPINT_SCRATCH_ATTRIBUTE_FEATURESET] = 0 ;
}
