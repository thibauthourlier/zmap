/*  File: zmapView.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
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
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *
 * Description: Handles the getting of the feature context from sources
 *              and their subsequent processing.
 *              
 * Exported functions: See ZMap/zmapView.h
 * HISTORY:
 * Last edited: Feb  5 10:53 2009 (edgrif)
 * Created: Thu May 13 15:28:26 2004 (edgrif)
 * CVS info:   $Id: zmapView.c,v 1.146 2009-02-05 12:03:38 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <string.h>
#include <sys/types.h>
#include <signal.h>             /* kill() */
#include <gtk/gtk.h>
#include <ZMap/zmapUtils.h>
#include <ZMap/zmapGLibUtils.h>
#include <ZMap/zmapUtilsXRemote.h>
#include <ZMap/zmapXRemote.h>
#include <ZMap/zmapConfig.h>
#include <ZMap/zmapConfigStanzaStructs.h>
#include <ZMap/zmapCmdLineArgs.h>
#include <ZMap/zmapConfigDir.h>
#include <ZMap/zmapConfigIni.h>
#include <ZMap/zmapConfigLoader.h>
#include <ZMap/zmapServerProtocol.h>
#include <zmapView_P.h>


typedef struct
{
  /* Processing options. */

  gboolean dynamic_loading ;


  /* Context data. */

  char *database_path ;

  GList *feature_sets ;

  GList *required_styles ;
  gboolean server_styles_have_mode ;

  GData *curr_styles ;					    /* Styles for this context. */
  ZMapFeatureContext curr_context ;

  ZMapServerReqType last_request ;
} ConnectionDataStruct, *ConnectionData ;


typedef struct
{
  gboolean found_style ;
  GString *missing_styles ;
} DrawableDataStruct, *DrawableData ;


typedef struct
{
  GData *styles ;
  gboolean error ;
} UnsetDeferredLoadStylesStruct, *UnsetDeferredLoadStyles ;




static ZMapView createZMapView(GtkWidget *xremote_widget, char *view_name,
			       GList *sequences, void *app_data) ;
static void destroyZMapView(ZMapView *zmap) ;

static gint zmapIdleCB(gpointer cb_data) ;
static void enterCB(ZMapWindow window, void *caller_data, void *window_data) ;
static void leaveCB(ZMapWindow window, void *caller_data, void *window_data) ;
static void scrollCB(ZMapWindow window, void *caller_data, void *window_data) ;
static void focusCB(ZMapWindow window, void *caller_data, void *window_data) ;
static void viewSelectCB(ZMapWindow window, void *caller_data, void *window_data) ;
static void viewVisibilityChangeCB(ZMapWindow window, void *caller_data, void *window_data) ;
static void setZoomStatusCB(ZMapWindow window, void *caller_data, void *window_data) ;
static void commandCB(ZMapWindow window, void *caller_data, void *window_data) ;
static void loaded_dataCB(ZMapWindow window, void *caller_data, void *window_data) ;
static void viewSplitToPatternCB(ZMapWindow window, void *caller_data, void *window_data);

static void setZoomStatus(gpointer data, gpointer user_data);
static void splitMagic(gpointer data, gpointer user_data);

static void loadFeatures(ZMapView view, ZMapWindowCallbackGetFeatures get_data) ;
static void unsetDeferredLoadStylesCB(GQuark key_id, gpointer data, gpointer user_data) ;

static void startStateConnectionChecking(ZMapView zmap_view) ;
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static void stopStateConnectionChecking(ZMapView zmap_view) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
static gboolean checkStateConnections(ZMapView zmap_view) ;

static gboolean dispatchContextRequests(ZMapViewConnection view_con, ZMapServerReqAny req_any) ;
static gboolean processDataRequests(ZMapViewConnection view_con, ZMapServerReqAny req_any) ;
static void freeDataRequest(ZMapViewConnection view_con, ZMapServerReqAny req_any) ;

static gboolean processGetSeqRequests(ZMapViewConnection view_con, ZMapServerReqAny req_any) ;

static ZMapViewConnection createConnection(ZMapView zmap_view,
					   ZMapURL url, char *format,
					   int timeout, char *version,
					   char *styles, char *styles_file,
					   char *feature_sets, char *navigator_set_names,
					   gboolean sequence_server, gboolean writeback_server,
					   char *sequence, int start, int end) ;
static void destroyConnection(ZMapViewConnection *view_conn) ;
static void killGUI(ZMapView zmap_view) ;
static void killConnections(ZMapView zmap_view) ;

static void resetWindows(ZMapView zmap_view) ;
static void displayDataWindows(ZMapView zmap_view,
			       ZMapFeatureContext all_features, 
                               ZMapFeatureContext new_features, GData *new_styles,
                               gboolean undisplay) ;
static void killAllWindows(ZMapView zmap_view) ;

static ZMapViewWindow createWindow(ZMapView zmap_view, ZMapWindow window) ;
static void destroyWindow(ZMapView zmap_view, ZMapViewWindow view_window) ;

static void getFeatures(ZMapView zmap_view, ZMapServerReqGetFeatures feature_req, GData *styles) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static GList *string2StyleQuarks(char *feature_sets) ;
static gboolean nextIsQuoted(char **text) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

static gboolean justMergeContext(ZMapView view, ZMapFeatureContext *context_inout, GData *styles);
static void justDrawContext(ZMapView view, ZMapFeatureContext diff_context, GData *styles);

static ZMapFeatureContext createContext(char *sequence, int start, int end, GList *feature_set_names) ;

static ZMapViewWindow addWindow(ZMapView zmap_view, GtkWidget *parent_widget) ;

static void addAlignments(ZMapFeatureContext context) ;

static gboolean mergeAndDrawContext(ZMapView view, ZMapFeatureContext context_inout, GData *styles);
static void eraseAndUndrawContext(ZMapView view, ZMapFeatureContext context_inout);

#ifdef NOT_REQUIRED_ATM
static gboolean getSequenceServers(ZMapView zmap_view, char *config_str) ;
static void destroySeq2ServerCB(gpointer data, gpointer user_data_unused) ;
static ZMapViewSequence2Server createSeq2Server(char *sequence, char *server) ;
static void destroySeq2Server(ZMapViewSequence2Server seq_2_server) ;
static gboolean checkSequenceToServerMatch(GList *seq_2_server, ZMapViewSequence2Server target_seq_server) ;
static gint findSequence(gconstpointer a, gconstpointer b) ;
#endif /* NOT_REQUIRED_ATM */

static void threadDebugMsg(ZMapThread thread, char *format_str, char *msg) ;

static void killAllSpawned(ZMapView zmap_view);

static gboolean connections(ZMapView zmap_view, gboolean threads_have_died) ;


static gboolean makeStylesDrawable(GData *styles, char **missing_styles_out) ;
static void drawableCB(GQuark key_id, gpointer data, gpointer user_data) ;



/* These callback routines are global because they are set just once for the lifetime of the
 * process. */

/* Callbacks we make back to the level above us. */
static ZMapViewCallbacks view_cbs_G = NULL ;


/* Callbacks back we set in the level below us, i.e. zMapWindow. */
ZMapWindowCallbacksStruct window_cbs_G = 
{
  enterCB, leaveCB,
  scrollCB,
  focusCB, 
  viewSelectCB, 
  viewSplitToPatternCB,
  setZoomStatusCB,
  viewVisibilityChangeCB,
  commandCB,
  loaded_dataCB
} ;




/*
 *  ------------------- External functions -------------------
 */



/*! @defgroup zmapview   zMapView: feature context display/processing
 * @{
 * 
 * \brief  Feature Context View Handling.
 * 
 * zMapView routines receive requests to load, display and process
 * feature contexts. Each ZMapView corresponds to a single feature context.
 * 
 *
 *  */



/*!
 * This routine must be called just once before any other views routine,
 * the caller must supply all of the callback routines.
 * 
 * @param callbacks   Caller registers callback functions that view will call
 *                    from the appropriate actions.
 * @return <nothing>
 *  */
void zMapViewInit(ZMapViewCallbacks callbacks)
{
  zMapAssert(!view_cbs_G) ;
  /* We have to assert alot here... */
  zMapAssert(callbacks);
  zMapAssert(callbacks->enter);
  zMapAssert(callbacks->leave);
  zMapAssert(callbacks->load_data);
  zMapAssert(callbacks->focus);
  zMapAssert(callbacks->select);
  zMapAssert(callbacks->visibility_change);
  zMapAssert(callbacks->state_change && callbacks->destroy) ;

  /* Now we can get on. */
  view_cbs_G = g_new0(ZMapViewCallbacksStruct, 1) ;

  view_cbs_G->enter = callbacks->enter ;
  view_cbs_G->leave = callbacks->leave ;
  view_cbs_G->load_data = callbacks->load_data ;
  view_cbs_G->focus = callbacks->focus ;
  view_cbs_G->select = callbacks->select ;
  view_cbs_G->split_to_pattern = callbacks->split_to_pattern;
  view_cbs_G->visibility_change = callbacks->visibility_change ;
  view_cbs_G->state_change = callbacks->state_change ;
  view_cbs_G->destroy = callbacks->destroy ;


  /* Init windows.... */
  zMapWindowInit(&window_cbs_G) ;

  return ;
}



/*!
 * Create a "view", this is the holder for a single feature context. The view may use
 * several threads to get this context and may display it in several windows.
 * A view _always_ has at least one window, this window may be blank but as long as
 * there is a view, there is a window. This makes the coding somewhat simpler and is
 * intuitively sensible.
 * 
 * @param xremote_widget   Widget that xremote commands for this view will be delivered to.
 * @param view_container   Parent widget of the view window(s)
 * @param sequence         Name of virtual sequence of context to be created.
 * @param start            Start coord of virtual sequence.
 * @param end              End coord of virtual sequence.
 * @param app_data         data that will be passed to the callers callback routines.
 * @return a new ZMapViewWindow (= view + a window)
 *  */
ZMapViewWindow zMapViewCreate(GtkWidget *xremote_widget, GtkWidget *view_container,
			      char *sequence, int start, int end,
			      void *app_data)
{
  ZMapViewWindow view_window = NULL ;
  ZMapView zmap_view = NULL ;
  gboolean debug ;
  ZMapViewSequenceMap sequence_fetch ;
  GList *sequences_list = NULL ;
  char *view_name ;

  /* No callbacks, then no view creation. */
  zMapAssert(view_cbs_G);
  zMapAssert(GTK_IS_WIDGET(view_container));
  zMapAssert(sequence);
  zMapAssert(start > 0);
  zMapAssert((end == 0 || end >= start)) ;

  /* Set up debugging for threads, we do it here so that user can change setting in config file
   * and next time they create a view the debugging will go on/off. */
  if (zMapUtilsConfigDebug(ZMAPSTANZA_DEBUG_APP_THREADS, &debug))
    zmap_thread_debug_G = debug ;

  /* Set up sequence to be fetched, in this case server defaults to whatever is set in config. file. */
  sequence_fetch = g_new0(ZMapViewSequenceMapStruct, 1) ;
  sequence_fetch->sequence = g_strdup(sequence) ;
  sequence_fetch->start = start ;
  sequence_fetch->end = end ;
  sequences_list = g_list_append(sequences_list, sequence_fetch) ;

  view_name = sequence ;

  zmap_view = createZMapView(xremote_widget, view_name, sequences_list, app_data) ; /* N.B. this step can't fail. */

  zmap_view->state = ZMAPVIEW_INIT ;

  view_window = addWindow(zmap_view, view_container) ;

  zmap_view->cwh_hash = zmapViewCWHHashCreate();

  return view_window ;
}



void zMapViewSetupNavigator(ZMapViewWindow view_window, GtkWidget *canvas_widget)
{
  ZMapView zmap_view ;

  zMapAssert(view_window) ;

  zmap_view = view_window->parent_view ;

  if (zmap_view->state != ZMAPVIEW_DYING)
    {
      zmap_view->navigator_window = zMapWindowNavigatorCreate(canvas_widget);
      zMapWindowNavigatorSetCurrentWindow(zmap_view->navigator_window, view_window->window);
    }

  return ;
}



/* Connect a View to its databases via threads, at this point the View is blank and waiting
 * to be called to load some data. */
gboolean zMapViewConnect(ZMapView zmap_view, char *config_str)
{
  gboolean result = TRUE ;


  if (zmap_view->state != ZMAPVIEW_INIT)
    {
      /* Probably we should indicate to caller what the problem was here....
       * e.g. if we are resetting then say we are resetting etc.....again we need g_error here. */
      zMapLogCritical("GUI: %s", "cannot connect because not in init state.") ;

      result = FALSE ;
    }
  else if (zmap_view->step_list)
    {
      zMapLogCritical("GUI: %s", "View is already executing connection requests so cannot connect.") ;

      result = FALSE ;
    }
  else
    {
      ZMapConfigIniContext context ;
      GList *settings_list = NULL, *free_this_list = NULL;


      zMapStartTimer(ZMAP_GLOBAL_TIMER) ;
      zMapPrintTimer(NULL, "Open connection") ;

      zmapViewBusy(zmap_view, TRUE) ;

      /* There is some redundancy of state here as the below code actually does a connect
       * and load in one call but we will almost certainly need the extra states later... */
      zmap_view->state = ZMAPVIEW_CONNECTING ;

      if ((context = zMapConfigIniContextProvide()))
	{
	  zMapConfigIniContextIncludeBuffer(context, config_str);

	  settings_list = zMapConfigIniContextGetSources(context) ;
	  
	  zMapConfigIniContextDestroy(context);

	  context = NULL ;
	}

      /* Set up connections to the named servers. */
      if (settings_list)
	{
	  ZMapConfigSource current_server = NULL;
	  int connections = 0 ;

	  free_this_list = settings_list ;

	  current_server = (ZMapConfigSource)settings_list->data ;

	  /* Create the step list that will be used to control obtaining the feature
	   * context from the multiple sources. */
	  zmap_view->on_fail = REQUEST_ONFAIL_CONTINUE ;
	  zmap_view->step_list = zmapViewStepListCreate(dispatchContextRequests,
							processDataRequests,
							freeDataRequest) ;
	  zmapViewStepListAddStep(zmap_view->step_list, ZMAP_SERVERREQ_CREATE) ;
	  zmapViewStepListAddStep(zmap_view->step_list, ZMAP_SERVERREQ_OPEN) ;
	  zmapViewStepListAddStep(zmap_view->step_list, ZMAP_SERVERREQ_GETSERVERINFO) ;
	  zmapViewStepListAddStep(zmap_view->step_list, ZMAP_SERVERREQ_FEATURESETS) ;
	  zmapViewStepListAddStep(zmap_view->step_list, ZMAP_SERVERREQ_STYLES) ;
	  zmapViewStepListAddStep(zmap_view->step_list, ZMAP_SERVERREQ_NEWCONTEXT) ;
	  zmapViewStepListAddStep(zmap_view->step_list, ZMAP_SERVERREQ_FEATURES) ;

	  /* Should test for dna col here as well....should unify dna and other features.... */
	  if (current_server->sequence)
	    zmapViewStepListAddStep(zmap_view->step_list, ZMAP_SERVERREQ_SEQUENCE) ;


	  /* Current error handling policy is to connect to servers that we can and
	   * report errors for those where we fail but to carry on and set up the ZMap
	   * as long as at least one connection succeeds. */
	  do
	    {
	      int url_parse_error ;
              ZMapURL urlObj;
	      ZMapViewConnection view_con ;
	      
	      current_server = (ZMapConfigSource)settings_list->data ;
	      
              if (!current_server->url)
		{
		  /* url is absolutely required. Go on to next stanza if there isn't one.
		   * Done before anything else so as not to set seq/write servers to invalid locations  */
		  zMapLogWarning("GUI: %s", "No url specified in config source group.") ;

		  continue ;
		}
#ifdef NOT_REQUIRED_ATM
	      /* This will become redundant with step stuff..... */

	      else if (!checkSequenceToServerMatch(zmap_view->sequence_2_server, &tmp_seq))
		{
		  /* If certain sequences must only be fetched from certain servers then make sure
		   * we only make those connections. */
		  
		  continue ;
		}
#endif /* NOT_REQUIRED_ATM */


              /* Parse the url and create connection. */
              if (!(urlObj = url_parse(current_server->url, &url_parse_error)))
                {
                  zMapLogWarning("GUI: url %s did not parse. Parse error < %s >\n",
                                 current_server->url,
                                 url_error(url_parse_error)) ;
                }
	      else
		{
		  if ((view_con = createConnection(zmap_view, urlObj,
						   (char *)current_server->format,
						   current_server->timeout,
						   (char *)current_server->version,
						   (char *)current_server->styles_list,
						   (char *)current_server->stylesfile,
						   (char *)current_server->featuresets,
						   (char *)current_server->navigatorsets,
						   current_server->sequence,
						   current_server->writeback,
						   zmap_view->sequence,
						   zmap_view->start,
						   zmap_view->end)))
		    {
		      /* Everything went well... replace current call */
		      zmap_view->connection_list = g_list_append(zmap_view->connection_list, view_con) ;
		      connections++ ;


		      /* THESE NEED TO GO WHEN STEP LIST STUFF IS DONE PROPERLY.... */
		      if (current_server->sequence)
			zmap_view->sequence_server  = view_con ;
		      if (current_server->writeback)
			zmap_view->writeback_server = view_con ;
		  
		    }
		  else
		    {
		      zMapLogWarning("GUI: url %s looks ok (host: %s\tport: %d)"
				     "but could not connect to server.",
				     urlObj->url, 
				     urlObj->host, 
				     urlObj->port) ; 
		    }
		}


	    }
	  while ((settings_list = g_list_next(settings_list)));
	      

	  /* Ought to return a gerror here........ */
	  if (!connections)
	    result = FALSE ;

	  zMapConfigSourcesFreeList(free_this_list);
	}
      else
	{
	  result = FALSE;
	}


      /* If at least one connection succeeded then we are up and running, if not then the zmap
       * returns to the init state. */
      if (result)
	{
	  zmap_view->state = ZMAPVIEW_LOADING ;

	  /* Start the connections to the sources. */
	  zmapViewStepListIter(zmap_view->step_list) ;
	}
      else
	{
	  zmap_view->state = ZMAPVIEW_INIT ;
	}


      /* Start polling function that checks state of this view and its connections,
       * this will wait until the connections reply, process their replies and call
       * zmapViewStepListIter() again.
       */
      startStateConnectionChecking(zmap_view) ;
    }

  return result ;
}



/* Copies an existing window in a view.
 * Returns the window on success, NULL on failure. */
ZMapViewWindow zMapViewCopyWindow(ZMapView zmap_view, GtkWidget *parent_widget,
				  ZMapWindow copy_window, ZMapWindowLockType window_locking)
{
  ZMapViewWindow view_window = NULL ;


  if (zmap_view->state != ZMAPVIEW_DYING)
    {
      /* the view _must_ already have a window _and_ data. */
      zMapAssert(zmap_view);
      zMapAssert(parent_widget);
      zMapAssert(zmap_view->window_list);
      zMapAssert(zmap_view->state == ZMAPVIEW_LOADED) ;

      view_window = createWindow(zmap_view, NULL) ;

      zmapViewBusy(zmap_view, TRUE) ;

      if (!(view_window->window = zMapWindowCopy(parent_widget, zmap_view->sequence,
						 view_window, copy_window,
						 zmap_view->features,
						 window_locking)))
	{
	  /* should glog and/or gerror at this stage....really need g_errors.... */
	  /* should free view_window.... */

	  view_window = NULL ;
	}
      else
	{
	  /* add to list of windows.... */
	  zmap_view->window_list = g_list_append(zmap_view->window_list, view_window) ;

	}

      zmapViewBusy(zmap_view, FALSE) ;
    }

  return view_window ;
}


/* Returns number of windows for current view. */
int zMapViewNumWindows(ZMapViewWindow view_window)
{
  int num_windows = 0 ;
  ZMapView zmap_view ;

  zMapAssert(view_window) ;

  zmap_view = view_window->parent_view ;

  if (zmap_view->state != ZMAPVIEW_DYING && zmap_view->window_list)
    {
      num_windows = g_list_length(zmap_view->window_list) ;
    }

  return num_windows ;
}


/* Removes a window from a view, the last window cannot be removed, the view must
 * always have at least one window. The function does nothing if there is only one
 * window. */
void zMapViewRemoveWindow(ZMapViewWindow view_window)
{
  ZMapView zmap_view ;

  zMapAssert(view_window) ;

  zmap_view = view_window->parent_view ;

  if (zmap_view->state != ZMAPVIEW_DYING)
    {
      if (g_list_length(zmap_view->window_list) > 1)
	{
	  /* We should check the window is in the list of windows for that view and abort if
	   * its not........ */
	  zMapWindowBusy(view_window->window, TRUE) ;

	  destroyWindow(zmap_view, view_window) ;

	  /* no need to reset cursor here...the window is gone... */
	}
    }

  return ;
}



/*!
 * Get the views "xremote" widget, returns NULL if view is dying.
 * 
 * @param                The ZMap View
 * @return               NULL or views xremote widget.
 *  */
GtkWidget *zMapViewGetXremote(ZMapView view)
{
  GtkWidget *xremote_widget = NULL ;

  if (view->state != ZMAPVIEW_DYING)
    xremote_widget = view->xremote_widget ;

  return xremote_widget ;
}


/*!
 * Get the views X window id for the "xremote" widget, this is the label
 * by which the view is known to the client program. Function will
 * always return the value <b>even</b> the actual widget has been destroyed.
 * 
 * @param                The ZMap View
 * @return               The X Window id of the views xremote widget.
 *  */
unsigned long zMapViewGetXremoteXWID(ZMapView view)
{
  return view->xwid ;
}


/*!
 * Erases the supplied context from the view's context and instructs
 * the window to delete the old features.
 *
 * @param                The ZMap View
 * @param                The Context to erase.  Those features which
 *                       match will be removed from this context and
 *                       the view's own context. They will also be 
 *                       removed from the display windows. Those that
 *                       don't match will be left in this context.
 * @return               void
 *************************************************** */
void zmapViewEraseFromContext(ZMapView replace_me, ZMapFeatureContext context_inout)
{
  if (replace_me->state != ZMAPVIEW_DYING)
    /* should replace_me be a view or a view_window???? */
    eraseAndUndrawContext(replace_me, context_inout);
    
  return;
}

/*!
 * Merges the supplied context with the view's context.
 *
 * @param                The ZMap View
 * @param                The Context to merge in.  This will be emptied
 *                       but needs destroying...
 * @return               The diff context.  This needs destroying.
 *************************************************** */
ZMapFeatureContext zmapViewMergeInContext(ZMapView view, ZMapFeatureContext context)
{

  /* called from zmapViewRemoteReceive.c, no styles are available. */

  if (view->state != ZMAPVIEW_DYING)
    justMergeContext(view, &context, NULL);

  return context;
}

/*!
 * Instructs the view to draw the diff context. The drawing will
 * happen sometime in the future, so we NULL the diff_context!
 *
 * @param               The ZMap View
 * @param               The Context to draw...
 *
 * @return              Boolean to notify whether the context was 
 *                      free'd and now == NULL, FALSE only if
 *                      diff_context is the same context as view->features
 *************************************************** */
gboolean zmapViewDrawDiffContext(ZMapView view, ZMapFeatureContext *diff_context)
{
  gboolean context_freed = TRUE;

  /* called from zmapViewRemoteReceive.c, no styles are available. */

  if (view->state != ZMAPVIEW_DYING)
    {
      if(view->features == *diff_context)
        context_freed = FALSE;

      justDrawContext(view, *diff_context, NULL);
    }
  else
    {
      context_freed = TRUE;
      zMapFeatureContextDestroy(*diff_context, context_freed);
    }

  if(context_freed)
    *diff_context = NULL;

  return context_freed;
}

/* Force a redraw of all the windows in a view, may be reuqired if it looks like
 * drawing has got out of whack due to an overloaded network etc etc. */
void zMapViewRedraw(ZMapViewWindow view_window)
{
  ZMapView view ;
  GList* list_item ;

  view = zMapViewGetView(view_window) ;
  zMapAssert(view) ;

  if (view->state == ZMAPVIEW_LOADED)
    {
      list_item = g_list_first(view->window_list) ;
      do
	{
	  ZMapViewWindow view_window ;

	  view_window = list_item->data ;

	  zMapWindowRedraw(view_window->window) ;
	}
      while ((list_item = g_list_next(list_item))) ;
    }

  return ;
}


/* Show stats for this view. */
void zMapViewStats(ZMapViewWindow view_window)
{
  ZMapView view ;
  GList* list_item ;

  view = zMapViewGetView(view_window) ;
  zMapAssert(view) ;

  if (view->state == ZMAPVIEW_LOADED)
    {
      ZMapViewWindow view_window ;

      list_item = g_list_first(view->window_list) ;
      view_window = list_item->data ;

      zMapWindowStats(view_window->window) ;
    }

  return ;
}


/* Reverse complement a view, this call will:
 * 
 *    - leave the View window(s) displayed and hold onto user information such as machine/port
 *      sequence etc so user does not have to add the data again.
 *    - reverse complement the sequence context.
 *    - display the reversed context.
 * 
 *  */
gboolean zMapViewReverseComplement(ZMapView zmap_view)
{
  gboolean result = TRUE ;

  if (zmap_view->state != ZMAPVIEW_LOADED)
    result = FALSE ;
  else
    {
      GList* list_item ;

      zmapViewBusy(zmap_view, TRUE) ;

      /* Call the feature code that will do the revcomp. */
      zMapFeatureReverseComplement(zmap_view->features, zmap_view->orig_styles) ;

      /* Set our record of reverse complementing. */
      zmap_view->revcomped_features = !(zmap_view->revcomped_features) ;

      zMapWindowNavigatorSetStrand(zmap_view->navigator_window, zmap_view->revcomped_features);
      zMapWindowNavigatorReset(zmap_view->navigator_window);
      zMapWindowNavigatorDrawFeatures(zmap_view->navigator_window, zmap_view->features, zmap_view->orig_styles);

      list_item = g_list_first(zmap_view->window_list) ;
      do
	{
	  ZMapViewWindow view_window ;

	  view_window = list_item->data ;

	  zMapWindowFeatureRedraw(view_window->window, zmap_view->features, TRUE) ;
	}
      while ((list_item = g_list_next(list_item))) ;

      /* Not sure if we need to do this or not.... */
      /* signal our caller that we have data. */
      (*(view_cbs_G->load_data))(zmap_view, zmap_view->app_data, NULL) ;
      
      zmapViewBusy(zmap_view, FALSE);
    }

  return TRUE ;
}

/* Return which strand we are showing viz-a-viz reverse complementing. */
gboolean zMapViewGetRevCompStatus(ZMapView zmap_view)
{
  return zmap_view->revcomped_features ;
}



/* Reset an existing view, this call will:
 * 
 *    - leave the View window(s) displayed and hold onto user information such as machine/port
 *      sequence etc so user does not have to add the data again.
 *    - Free all ZMap window data that was specific to the view being loaded.
 *    - Kill the existing server thread(s).
 * 
 * After this call the ZMap will be ready for the user to try again with the existing
 * machine/port/sequence or to enter data for a new sequence etc.
 * 
 *  */
gboolean zMapViewReset(ZMapView zmap_view)
{
  gboolean result = FALSE ;

  if (zmap_view->state == ZMAPVIEW_CONNECTING || zmap_view->state == ZMAPVIEW_CONNECTED
      || zmap_view->state == ZMAPVIEW_LOADING || zmap_view->state == ZMAPVIEW_LOADED)
    {
      zmapViewBusy(zmap_view, TRUE) ;

      zmap_view->state = ZMAPVIEW_RESETTING ;

      /* Reset all the windows to blank. */
      resetWindows(zmap_view) ;

      /* We need to destroy the existing thread connection and wait until user loads a new
	 sequence. */
      killConnections(zmap_view) ;

      zmap_view->connections_loaded = 0 ;

      result = TRUE ;
    }

  return result ;
}


/* 
 * If view_window is NULL all windows are zoomed.
 * 
 * 
 *  */
void zMapViewZoom(ZMapView zmap_view, ZMapViewWindow view_window, double zoom)
{
  if (zmap_view->state == ZMAPVIEW_LOADED)
    {
      if (view_window)
	zMapWindowZoom(zMapViewGetWindow(view_window), zoom) ;
      else
	{
	  GList* list_item ;

	  list_item = g_list_first(zmap_view->window_list) ;

	  do
	    {
	      ZMapViewWindow view_window ;

	      view_window = list_item->data ;

	      zMapWindowZoom(view_window->window, zoom) ;
	    }
	  while ((list_item = g_list_next(list_item))) ;
	}
    }

  return ;
}

void zMapViewHighlightFeatures(ZMapView view, ZMapViewWindow view_window, ZMapFeatureContext context, gboolean multiple)
{
  GList *list;

  if (view->state == ZMAPVIEW_LOADED)
    {
      if (view_window)
	{
	  zMapLogWarning("%s", "What were you thinking");
	}
      else
	{
	  list = g_list_first(view->window_list);
	  do
	    {
	      view_window = list->data;
	      zMapWindowHighlightObjects(view_window->window, context, multiple);
	    }
	  while((list = g_list_next(list)));
	}
    }

  return ;
}


/*
 *    A set of accessor functions.
 */
char *zMapViewGetSequence(ZMapView zmap_view)
{
  char *sequence = NULL ;

  if (zmap_view->state != ZMAPVIEW_DYING)
    sequence = zmap_view->sequence ;

  return sequence ;
}

ZMapFeatureContext zMapViewGetFeatures(ZMapView zmap_view)
{
  ZMapFeatureContext features = NULL ;

  if (zmap_view->state != ZMAPVIEW_DYING && zmap_view->features)
    features = zmap_view->features ;

  return features ;
}

GData *zMapViewGetStyles(ZMapViewWindow view_window)
{
  GData *styles = NULL ;
  ZMapView view = zMapViewGetView(view_window);
  
  if (view->state != ZMAPVIEW_DYING)
    styles = view->orig_styles ;

  return styles;
}

gboolean zMapViewGetFeaturesSpan(ZMapView zmap_view, int *start, int *end)
{
  gboolean result = FALSE ;

  if (zmap_view->state != ZMAPVIEW_DYING && zmap_view->features)
    {
      *start = zmap_view->features->sequence_to_parent.c1 ;
      *end = zmap_view->features->sequence_to_parent.c2 ;

      result = TRUE ;
    }

  return result ;
}


/* N.B. we don't exclude ZMAPVIEW_DYING because caller may want to know that ! */
ZMapViewState zMapViewGetStatus(ZMapView zmap_view)
{
  return zmap_view->state ;
}

char *zMapViewGetStatusStr(ZMapViewState state)
{
  /* Array must be kept in synch with ZmapState enum in zmapView.h */
  static char *zmapStates[] = {"",
			       "Connecting", "Connected",
			       "Data loading", "Data loaded",
			       "Resetting", "Dying"} ;
  char *state_str ;

  zMapAssert(state >= ZMAPVIEW_INIT);
  zMapAssert(state <= ZMAPVIEW_DYING) ;

  state_str = zmapStates[state] ;

  return state_str ;
}


ZMapWindow zMapViewGetWindow(ZMapViewWindow view_window)
{
  ZMapWindow window = NULL ;

  zMapAssert(view_window) ;

  if (view_window->parent_view->state != ZMAPVIEW_DYING)
    window = view_window->window ;

  return window ;
}

ZMapWindowNavigator zMapViewGetNavigator(ZMapView view)
{
  ZMapWindowNavigator navigator = NULL ;

  zMapAssert(view) ;

  if (view->state != ZMAPVIEW_DYING)
    navigator = view->navigator_window ;

  return navigator ;
}


GList *zMapViewGetWindowList(ZMapViewWindow view_window)
{
  zMapAssert(view_window);

  return view_window->parent_view->window_list;
}


void zMapViewSetWindowList(ZMapViewWindow view_window, GList *list)
{
  zMapAssert(view_window);
  zMapAssert(list);

  view_window->parent_view->window_list = list;

  return;
}


ZMapView zMapViewGetView(ZMapViewWindow view_window)
{
  ZMapView view = NULL ;

  zMapAssert(view_window) ;

  if (view_window->parent_view->state != ZMAPVIEW_DYING)
    view = view_window->parent_view ;

  return view ;
}

void zMapViewReadConfigBuffer(ZMapView zmap_view, char *buffer)
{
  /* designed to add extra config bits to an already created view... */
  /* This is probably a bit of a hack, why can't we make the zMapViewConnect do it?  */
#ifdef NOT_REQUIRED_ATM
  getSequenceServers(zmap_view, buffer);
#endif /* NOT_REQUIRED_ATM */
  return ;
}

void zmapViewFeatureDump(ZMapViewWindow view_window, char *file)
{

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  zMapFeatureDump(view_window->parent_view->features, file) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  printf("reimplement.......\n") ;

  return;
}


/* Called to kill a view and get the associated threads killed, note that
 * this call just signals everything to die, its the checkConnections() routine
 * that really clears up and when everything has died signals the caller via the
 * callback routine that they supplied when the view was created.
 *
 * NOTE: if the function returns FALSE it means the view has signalled its threads
 * and is waiting for them to die, the caller should thus wait until view signals
 * via the killedcallback that the view has really died before doing final clear up.
 * 
 * If the function returns TRUE it means that the view has been killed immediately
 * because it had no threads so the caller can clear up immediately.
 */
void zMapViewDestroy(ZMapView zmap_view)
{

  if (zmap_view->state != ZMAPVIEW_DYING)
    {
      zmapViewBusy(zmap_view, TRUE) ;

      /* All states have GUI components which need to be destroyed. */
      killGUI(zmap_view) ;

      if (zmap_view->state == ZMAPVIEW_INIT)
	{
	  /* For init we simply need to signal to our parent layer that we have died,
	   * we will then be cleaned up immediately. */

	  zmap_view->state = ZMAPVIEW_DYING ;
	}
      else
	{
	  /* for other states there are threads to kill so they must be cleaned
	   * up asynchronously. */

	  if (zmap_view->state != ZMAPVIEW_RESETTING)
	    {
	      /* If we are resetting then the connections have already being killed. */
	      killConnections(zmap_view) ;
	    }

	  /* Must set this as this will prevent any further interaction with the ZMap as
	   * a result of both the ZMap window and the threads dying asynchronously.  */
	  zmap_view->state = ZMAPVIEW_DYING ;
	}
    }

  return ;
}


/*! @} end of zmapview docs. */




/*
 *             Functions internal to zmapView package.
 */



char *zmapViewGetStatusAsStr(ZMapViewState state)
{
  /* Array must be kept in synch with ZmapState enum in ZMap.h */
  static char *zmapStates[] = {"ZMAPVIEW_INIT",
			       "ZMAPVIEW_NOT_CONNECTED", "ZMAPVIEW_NO_WINDOW",
			       "ZMAPVIEW_CONNECTING", "ZMAPVIEW_CONNECTED",
			       "ZMAPVIEW_LOADING", "ZMAPVIEW_LOADED",
			       "ZMAPVIEW_RESETTING", "ZMAPVIEW_DYING"} ;
  char *state_str ;

  zMapAssert(state >= ZMAPVIEW_INIT);
  zMapAssert(state <= ZMAPVIEW_DYING) ;

  state_str = zmapStates[state] ;

  return state_str ;
}






/*
 *  ------------------- Internal functions -------------------
 */



/* We could provide an "executive" kill call which just left the threads dangling, a kind
 * of "kill -9" style thing, it would actually kill/delete stuff without waiting for threads
 * to die....or we could allow a "force" flag to zmapViewKill/Destroy  */


/* This is really the guts of the code to check what a connection thread is up
 * to. Every time the GUI thread has stopped doing things GTK calls this routine
 * which then checks our connections for responses from the threads...... */
static gint zmapIdleCB(gpointer cb_data)
{
  gint call_again = 0 ;
  ZMapView zmap_view = (ZMapView)cb_data ;

 
  /* Returning a value > 0 tells gtk to call zmapIdleCB again, so if checkConnections() returns
   * TRUE we ask to be called again. */
  if (checkStateConnections(zmap_view))
    call_again = 1 ;
  else
    call_again = 0 ;

  return call_again ;
}



static void enterCB(ZMapWindow window, void *caller_data, void *window_data)
{

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  ZMapViewWindow view_window = (ZMapViewWindow)caller_data ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* Not currently used because we are doing "click to focus" at the moment. */

  /* Pass back a ZMapViewWindow as it has both the View and the window. */
  (*(view_cbs_G->enter))(view_window, view_window->parent_view->app_data, NULL) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return ;
}


static void leaveCB(ZMapWindow window, void *caller_data, void *window_data)
{

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  ZMapViewWindow view_window = (ZMapViewWindow)caller_data ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* Not currently used because we are doing "click to focus" at the moment. */

  /* Pass back a ZMapViewWindow as it has both the View and the window. */
  (*(view_cbs_G->leave))(view_window, view_window->parent_view->app_data, NULL) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  return ;
}


static void scrollCB(ZMapWindow window, void *caller_data, void *window_data)
{

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  ZMapViewWindow view_window = (ZMapViewWindow)caller_data ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  printf("In View, in window scroll callback\n") ;


  return ;
}


/* Called when a sequence window has been focussed, usually by user actions. */
static void focusCB(ZMapWindow window, void *caller_data, void *window_data)
{
  ZMapViewWindow view_window = (ZMapViewWindow)caller_data ;

  /* Pass back a ZMapViewWindow as it has both the View and the window. */
  (*(view_cbs_G->focus))(view_window, view_window->parent_view->app_data, NULL) ;

  {
    double x1, x2, y1, y2;
    x1 = x2 = y1 = y2 = 0.0;
    /* zero the input so that nothing changes */
    zMapWindowNavigatorFocus(view_window->parent_view->navigator_window, TRUE,
                             &x1, &y1, &x2, &y2);
  }

  return ;
}


/* Called when some sequence window feature (e.g. column, actual feature etc.)
 * has been selected. */
static void viewSelectCB(ZMapWindow window, void *caller_data, void *window_data)
{
  ZMapViewWindow view_window = (ZMapViewWindow)caller_data ;
  ZMapWindowSelect window_select = (ZMapWindowSelect)window_data ;
  ZMapViewSelectStruct view_select = {0} ;

  /* Check we've got a window_select! */
  if(!window_select)
    return ;                    /* !!! RETURN !!! */

  if((view_select.type = window_select->type) == ZMAPWINDOW_SELECT_SINGLE)
    {
      if (window_select->highlight_item)
	{
	  GList* list_item ;

	  /* Highlight the feature in all windows, BUT note that we may not find it in some
	   * windows, e.g. if a window has been reverse complemented the item may be hidden. */
	  list_item = g_list_first(view_window->parent_view->window_list) ;

	  do
	    {
	      ZMapViewWindow view_window ;
	      FooCanvasItem *item ;

	      view_window = list_item->data ;

	      if ((item = zMapWindowFindFeatureItemByItem(view_window->window, window_select->highlight_item)))
		zMapWindowHighlightObject(view_window->window, item,
					  window_select->replace_highlight_item,
					  window_select->highlight_same_names) ;
	    }
	  while ((list_item = g_list_next(list_item))) ;
	}


      view_select.feature_desc   = window_select->feature_desc ;
      view_select.secondary_text = window_select->secondary_text ;

    }


  view_select.xml_handler = window_select->xml_handler ;    /* n.b. struct copy. */
  
  if(window_select->xml_handler.zmap_action)
    {
      view_select.xml_handler.handled =
        window_select->xml_handler.handled = zmapViewRemoteSendCommand(view_window->parent_view,
                                                                       window_select->xml_handler.zmap_action,
                                                                       window_select->xml_handler.xml_events,
                                                                       window_select->xml_handler.start_handlers,
                                                                       window_select->xml_handler.end_handlers,
                                                                       window_select->xml_handler.handler_data);        
    }


  /* Pass back a ZMapViewWindow as it has both the View and the window to our caller. */
  (*(view_cbs_G->select))(view_window, view_window->parent_view->app_data, &view_select) ;

  
  window_select->xml_handler.handled = view_select.xml_handler.handled;
  
  return ;
}



static void viewSplitToPatternCB(ZMapWindow window, void *caller_data, void *window_data)
{
  ZMapViewWindow view_window = (ZMapViewWindow)caller_data;
  ZMapWindowSplitting split  = (ZMapWindowSplitting)window_data;
  ZMapViewSplittingStruct view_split = {0};

  view_split.split_patterns      = split->split_patterns;
  view_split.touched_window_list = NULL;

  view_split.touched_window_list = g_list_append(view_split.touched_window_list, view_window);

  (*(view_cbs_G->split_to_pattern))(view_window, view_window->parent_view->app_data, &view_split);

  /* foreach window find feature and Do something according to pattern */
  split->window_index = 0;
  g_list_foreach(view_split.touched_window_list, splitMagic, window_data);
  
  /* clean up the list */
  g_list_free(view_split.touched_window_list);

  return ;
}

static void splitMagic(gpointer data, gpointer user_data)
{
  ZMapViewWindow view_window = (ZMapViewWindow)data;
  ZMapWindowSplitting  split = (ZMapWindowSplitting)user_data;
  ZMapSplitPattern   pattern = NULL;

  if(view_window->window == split->original_window)
    {
      printf("Ignoring original window for now."
             "  I may well revisit this, but I"
             " think we should leave it alone ATM.\n");
      return ;                    /* really return from this */
    }

  if((pattern = &(g_array_index(split->split_patterns, ZMapSplitPatternStruct, split->window_index))))
    {
      printf("Trying pattern %d\n", split->window_index);
      /* Do it here! */
    }

  (split->window_index)++;

  return ;
}

static ZMapView createZMapView(GtkWidget *xremote_widget, char *view_name, GList *sequences, void *app_data)
{
  ZMapView zmap_view = NULL ;
  GList *first ;
  ZMapViewSequenceMap master_seq ;

  first = g_list_first(sequences) ;
  master_seq = (ZMapViewSequenceMap)(first->data) ;

  zmap_view = g_new0(ZMapViewStruct, 1) ;

  zmap_view->state = ZMAPVIEW_INIT ;
  zmap_view->busy = FALSE ;

  zmap_view->xremote_widget = xremote_widget ;
  zmap_view->xwid = zMapXRemoteWidgetGetXID(zmap_view->xremote_widget) ;

  zmapViewSetupXRemote(zmap_view, xremote_widget);

  zmap_view->view_name = g_strdup(view_name) ;

  /* TEMP CODE...UNTIL I GET THE MULTIPLE SEQUENCES IN ONE VIEW SORTED OUT..... */
  /* TOTAL HACK UP MESS.... */
  zmap_view->sequence = g_strdup(master_seq->sequence) ;
  zmap_view->start = master_seq->start ;
  zmap_view->end = master_seq->end ;

#ifdef NOT_REQUIRED_ATM
  /* TOTAL LASH UP FOR NOW..... */
  if (!(zmap_view->sequence_2_server))
    {
      getSequenceServers(zmap_view, NULL) ;
    }
#endif

  /* Set the regions we want to display. */
  zmap_view->sequence_mapping = sequences ;

  zmap_view->window_list = zmap_view->connection_list = NULL ;

  zmap_view->app_data = app_data ;

  zmap_view->revcomped_features = FALSE ;

  zmap_view->kill_blixems = TRUE ;

  zmap_view->session_data = g_new0(ZMapViewSessionStruct, 1) ;

  zmap_view->session_data->sequence = zmap_view->sequence ;


  return zmap_view ;
}


/* Adds a window to a view. */
static ZMapViewWindow addWindow(ZMapView zmap_view, GtkWidget *parent_widget)
{
  ZMapViewWindow view_window = NULL ;
  ZMapWindow window ;

  view_window = createWindow(zmap_view, NULL) ;

  /* There are no steps where this can fail at the moment. */
  window = zMapWindowCreate(parent_widget, zmap_view->sequence, view_window, NULL) ;
  zMapAssert(window) ;

  view_window->window = window ;

  /* add to list of windows.... */
  zmap_view->window_list = g_list_append(zmap_view->window_list, view_window) ;
	  

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE

  /* This seems redundant now....old code ???? */

  /* There is a bit of a hole in the "busy" state here in that we do the windowcreate
   * but have not signalled that we are busy, I suppose we could do the busy stuff twice,
   * once before the windowCreate and once here to make sure we set cursors everywhere.. */
  zmapViewBusy(zmap_view, TRUE) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* Start polling function that checks state of this view and its connections, note
   * that at this stage there is no data to display. */
  startStateConnectionChecking(zmap_view) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  zmapViewBusy(zmap_view, FALSE) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return view_window ;
}

static void killAllSpawned(ZMapView zmap_view)
{
  GPid pid;
  GList *processes = zmap_view->spawned_processes;

  if (zmap_view->kill_blixems)
    {
      while (processes)
	{
	  pid = GPOINTER_TO_INT(processes->data);
	  g_spawn_close_pid(pid);
	  kill(pid, 9);
	  processes = processes->next;
	}
    }

  if (zmap_view->spawned_processes)
    {
      g_list_free(zmap_view->spawned_processes);
      zmap_view->spawned_processes = NULL ;
    }

  return ;
}

/* Should only do this after the window and all threads have gone as this is our only handle
 * to these resources. The lists of windows and thread connections are dealt with somewhat
 * asynchronously by killAllWindows() & checkConnections() */
static void destroyZMapView(ZMapView *zmap_view_out)
{
  ZMapView zmap_view = *zmap_view_out ;

  g_free(zmap_view->sequence) ;

#ifdef NOT_REQUIRED_ATM
  if (zmap_view->sequence_2_server)
    {
      g_list_foreach(zmap_view->sequence_2_server, destroySeq2ServerCB, NULL) ;
      g_list_free(zmap_view->sequence_2_server) ;
      zmap_view->sequence_2_server = NULL ;
    }
#endif /* NOT_REQUIRED_ATM */

  if (zmap_view->cwh_hash)
    zmapViewCWHDestroy(&(zmap_view->cwh_hash));

  if (zmap_view->session_data)
    {
      if (zmap_view->session_data->servers)
	{
	  g_list_foreach(zmap_view->session_data->servers, zmapViewSessionFreeServer, NULL) ;
	  g_list_free(zmap_view->session_data->servers) ;
	}

      g_free(zmap_view->session_data) ;
    }

  if (zmap_view->step_list)
    zmapViewStepListDestroy(zmap_view->step_list) ;

  killAllSpawned(zmap_view);

  g_free(zmap_view) ;

  *zmap_view_out = NULL ;

  return ;
}




/* 
 *       Connection control functions, interface to the data fetching threads.
 */


/* Start the ZMapView GTK idle function (gets run when the GUI is doing nothing).
 */
static void startStateConnectionChecking(ZMapView zmap_view)
{

#ifdef UTILISE_ALL_CPU_ON_DESKPRO203
  zmap_view->idle_handle = gtk_idle_add(zmapIdleCB, (gpointer)zmap_view) ;
#endif /* UTILISE_ALL_CPU_ON_DESKPRO203 */ 

  zmap_view->idle_handle = gtk_timeout_add(100, zmapIdleCB, (gpointer)zmap_view) ;
  // WARNING: gtk_timeout_add is deprecated and should not be used in newly-written code. Use g_timeout_add() instead.

  return ;
}



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* I think that probably I won't need this most of the time, it could be used to remove the idle
 * function in a kind of unilateral way as a last resort, otherwise the idle function needs
 * to cancel itself.... */
static void stopStateConnectionChecking(ZMapView zmap_view)
{
  gtk_timeout_remove(zmap_view->idle_handle) ;

  return ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */





/* This function checks the status of the connection and checks for any reply and
 * then acts on it, it gets called from the ZMap idle function.
 * If all threads are ok and zmap has not been killed then routine returns TRUE
 * meaning it wants to be called again, otherwise FALSE.
 * 
 * The function monitors the View state so that when the last connection has disappeared
 * and the View is dying then the View is cleaned up and the caller gets called to say
 * the View is now dead.
 * 
 * NOTE that you cannot use a condvar here, if the connection thread signals us using a
 * condvar we will probably miss it, that just doesn't work, we have to pole for changes
 * and this is possible because this routine is called from the idle function of the GUI.
 * 
 *  */
static gboolean checkStateConnections(ZMapView zmap_view)
{
  gboolean call_again = TRUE ;				    /* Normally we want to called continuously. */
  gboolean state_change = TRUE ;			    /* Has view state changed ?. */
  gboolean threads_have_died = FALSE ;			    /* Have any threads died ? */


  /* should assert the zmapview_state here to save checking later..... */

  if (zmap_view->connection_list)
    {
      GList *list_item ;

      list_item = g_list_first(zmap_view->connection_list) ;

      do
	{
	  ZMapViewConnection view_con ;
	  ZMapThread thread ;
	  ZMapThreadReply reply ;
	  void *data = NULL ;
	  char *err_msg = NULL ;
      
	  view_con = list_item->data ;
	  thread = view_con->thread ;

	  /* NOTE HOW THE FACT THAT WE KNOW NOTHING ABOUT WHERE THIS DATA CAME FROM
	   * MEANS THAT WE SHOULD BE PASSING A HEADER WITH THE DATA SO WE CAN SAY WHERE
	   * THE INFORMATION CAME FROM AND WHAT SORT OF REQUEST IT WAS.....ACTUALLY WE
	   * GET A LOT OF INFO FROM THE CONNECTION ITSELF, E.G. SERVER NAME ETC. */

	  data = NULL ;
	  err_msg = NULL ;

	  if (!(zMapThreadGetReplyWithData(thread, &reply, &data, &err_msg)))
	    {
	      threadDebugMsg(thread, "GUI: thread %s, cannot access reply from thread - %s\n", err_msg) ;

	      /* should abort or dump here....or at least kill this connection. */
	    }
	  else
	    {
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	      threadDebugMsg(thread, "GUI: thread %s, thread reply = %s\n", zMapVarGetReplyString(reply)) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	      switch (reply)
		{
		case ZMAPTHREAD_REPLY_WAIT:
		  {
		    state_change = FALSE ;

		    break ;
		  }
		case ZMAPTHREAD_REPLY_GOTDATA:
		case ZMAPTHREAD_REPLY_REQERROR:
		  {
		    ZMapServerReqAny req_any ;
		    ZMapViewConnectionRequest request ;
		    gboolean kill_connection = FALSE ;

		    view_con->curr_request = ZMAPTHREAD_REQUEST_WAIT ;

		    /* Recover the request from the thread data. */
		    req_any = (ZMapServerReqAny)data ;

		    /* Recover the stepRequest from the view connection and process the data from
		     * the request. */
		    if (!(request = zmapViewStepListFindRequest(zmap_view->step_list, req_any->type, view_con)))
		      {
			zMapLogCritical("Request of type %s for connection %s not found in view %s step list !",
					zMapServerReqType2ExactStr(req_any->type),
					view_con->url,
					zmap_view->view_name) ;

			kill_connection = TRUE ;
		      }		      
		    else
		      {
			if (reply == ZMAPTHREAD_REPLY_REQERROR)
			  {
			    /* This means the request failed for some reason. */
			    threadDebugMsg(thread, "GUI: thread %s, request to server failed....\n", NULL) ;

			    if (err_msg)
			      {
				zMapLogCritical("Source \"%s\" has returned an error,"
						" request that failed was: %s", view_con->url, err_msg) ;

				g_free(err_msg) ;
			      }

			    if (zmap_view->on_fail == REQUEST_ONFAIL_CANCEL_THREAD)
			      kill_connection = TRUE ;
			  }
			else
			  {
			    threadDebugMsg(thread, "GUI: thread %s, got data\n", NULL) ;

			    /* Really we should only be loading stuff if we are LOADING.... */
			    if (zmap_view->state != ZMAPVIEW_LOADING && zmap_view->state != ZMAPVIEW_LOADED)
			      {
				threadDebugMsg(thread, "GUI: thread %s, got data but ZMap state is - %s\n",
					       zmapViewGetStatusAsStr(zMapViewGetStatus(zmap_view))) ;
			      }

			    zmapViewStepListStepProcessRequest(zmap_view->step_list, request) ;
			  }
		      }


		    if (reply == ZMAPTHREAD_REPLY_REQERROR
			&& (zmap_view->on_fail == REQUEST_ONFAIL_CANCEL_THREAD
			    || zmap_view->on_fail == REQUEST_ONFAIL_CANCEL_REQUEST))
		      {
			/* Remove request from all steps.... */
			zmapViewStepListStepRequestDeleteAll(zmap_view->step_list, request) ;
		      }

		    if (kill_connection)
		      {
			/* Warn the user ! */
			zMapWarning("Source \"%s\" has been cancelled, check log for details.", view_con->url) ;

			zMapLogCritical("Source \"%s\" has been terminated"
					" because a request to it has failed,"
					" error was: %s", view_con->url, err_msg) ;

			/* Signal thread to die. */
			zMapThreadKill(thread) ;
		      }


		    /* Reset the reply from the slave. */
		    zMapThreadSetReply(thread, ZMAPTHREAD_REPLY_WAIT) ;

		    break ;
		  }
		case ZMAPTHREAD_REPLY_DIED:
		  {
		    /* Thread has failed for some reason and we should clean up. */
		    if (err_msg)
		      zMapWarning("%s", err_msg) ;

		    threads_have_died = TRUE ;

		    threadDebugMsg(thread, "GUI: thread %s has died so cleaning up....\n", NULL) ;
		    
		    /* We are going to remove an item from the list so better move on from
		     * this item. */
		    list_item = g_list_next(list_item) ;
		    zmap_view->connection_list = g_list_remove(zmap_view->connection_list, view_con) ;
		    
		    destroyConnection(&view_con) ;

		    break ;
		  }
		case ZMAPTHREAD_REPLY_CANCELLED:
		  {
		    /* This happens when we have signalled the threads to die and they are
		     * replying to say that they have now died. */
		    threads_have_died = TRUE ;

		    /* This means the thread was cancelled so we should clean up..... */
		    threadDebugMsg(thread, "GUI: thread %s has been cancelled so cleaning up....\n", NULL) ;

		    /* We are going to remove an item from the list so better move on from
		     * this item. */
		    list_item = g_list_next(list_item) ;
		    zmap_view->connection_list = g_list_remove(zmap_view->connection_list, view_con) ;
		    
		    destroyConnection(&view_con) ;

		    break ;
		  }
		default:
		  {	  
		    zMapLogFatalLogicErr("switch(), unknown value: %d", reply) ;

		    break ;
		  }
		}
	    }
	}
      while ((list_item = g_list_next(list_item))) ;
    }


  /* Fiddly logic here as this could be combined with the following code that handles if we don't
   * have any connections any more...but not so easy as some of the code below kills the zmap so.
   * easier to do this here. */
  if (zmap_view->busy && !zmapAnyConnBusy(zmap_view->connection_list))
    zmapViewBusy(zmap_view, FALSE) ;




#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if (reply == ZMAPTHREAD_REPLY_REQERROR && zmap_view->on_fail == REQUEST_ONFAIL_CANCEL_REQUEST)
    {
      zmapViewStepListDestroy(zmap_view->step_list) ;
      zmap_view->step_list = NULL ;
    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



  /* At this point if we have connections then we carry on looping looking for
   * replies from the views. If there are no threads left then we need to examine
   * our state and take action depending on whether we are dying or threads
   * have died or whatever.... */
  if (zmap_view->connection_list)
    {
      /* Signal layer above us if view has changed state. */
      if (state_change)
	(*(view_cbs_G->state_change))(zmap_view, zmap_view->app_data, NULL) ;

      /* Check for more connection steps and dispatch them or clear up if finished. */
      if ((zmap_view->step_list))
	{
	  /* If there were errors then all connections may have been removed from
	   * step list or if we have finished then destroy step_list. */
	  if (zmapViewStepListAreConnections(zmap_view->step_list) && zmapViewStepListIsNext(zmap_view->step_list))
	    {
	      zmapViewStepListIter(zmap_view->step_list) ;
	    }
	  else
	    {
	      zmapViewStepListDestroy(zmap_view->step_list) ;
	      zmap_view->step_list = NULL ;
	    }
	}
    }
  else
    {
      /* Decide if we need to be called again or if everythings dead. */
      call_again = connections(zmap_view, threads_have_died) ;
    }


  return call_again ;
}




/* This is _not_ a generalised dispatch function, it handles a sequence of requests that
 * will end up fetching a feature context from a source. The steps are interdependent
 * and data from one step must be available to the next. */
static gboolean dispatchContextRequests(ZMapViewConnection connection, ZMapServerReqAny req_any)
{
  gboolean result = TRUE ;
  ConnectionData connect_data = (ConnectionData)(connection->request_data) ;

  switch (req_any->type)
    {
    case ZMAP_SERVERREQ_CREATE:
    case ZMAP_SERVERREQ_OPEN:
    case ZMAP_SERVERREQ_GETSERVERINFO:
    case ZMAP_SERVERREQ_FEATURESETS:
      {

	break ;
      }
    case ZMAP_SERVERREQ_STYLES:
      {
	ZMapServerReqStyles get_styles = (ZMapServerReqStyles)req_any ;

	/* required styles comes from featuresets call. */
	get_styles->required_styles_in = connect_data->required_styles ;

	break ;
      }
    case ZMAP_SERVERREQ_NEWCONTEXT:
      {
	ZMapServerReqNewContext new_context = (ZMapServerReqNewContext)req_any ;

	new_context->context = connect_data->curr_context ;

	break ;
      }
    case ZMAP_SERVERREQ_FEATURES:
      {
	ZMapServerReqGetFeatures get_features = (ZMapServerReqGetFeatures)req_any ;

	get_features->context = connect_data->curr_context ;
	get_features->styles = connect_data->curr_styles ;

	break ;
      }
    case ZMAP_SERVERREQ_SEQUENCE:
      {
	ZMapServerReqGetFeatures get_features = (ZMapServerReqGetFeatures)req_any ;

	get_features->context = connect_data->curr_context ;
	get_features->styles = connect_data->curr_styles ;

	break ;
      }
    default:
      {	  
	zMapLogFatalLogicErr("switch(), unknown value: %d", req_any->type) ;

	break ;
      }
    }

  result = TRUE ;

  return result ;
}



/* This is _not_ a generalised processing function, it handles a sequence of replies from
 * a thread that build up a feature context from a source. The steps are interdependent
 * and data from one step must be available to the next. */
static gboolean processDataRequests(ZMapViewConnection view_con, ZMapServerReqAny req_any)
{
  gboolean result = TRUE ;
  ConnectionData connect_data = (ConnectionData)(view_con->request_data) ;
  ZMapView zmap_view = view_con->parent_view ;

  /* Process the different types of data coming back. */
  switch (req_any->type)
    {
    case ZMAP_SERVERREQ_CREATE:
    case ZMAP_SERVERREQ_OPEN:
      {
	break ;
      }
    case ZMAP_SERVERREQ_GETSERVERINFO:
      {
	ZMapServerReqGetServerInfo get_info = (ZMapServerReqGetServerInfo)req_any ;

	connect_data->database_path = get_info->database_path_out ;

	break ;
      }
    case ZMAP_SERVERREQ_FEATURESETS:
      {
	ZMapServerReqFeatureSets feature_sets = (ZMapServerReqFeatureSets)req_any ;

	if (req_any->response == ZMAP_SERVERRESPONSE_OK)
	  {
	    /* Got the feature sets so record them in the server context. */
	    connect_data->curr_context->feature_set_names = feature_sets->feature_sets_inout ;
	  }
	else if (req_any->response == ZMAP_SERVERRESPONSE_UNSUPPORTED && feature_sets->feature_sets_inout)
	  {
	    /* If server doesn't support checking feature sets then just use those supplied. */
	    connect_data->curr_context->feature_set_names = feature_sets->feature_sets_inout ;

	    feature_sets->required_styles_out = g_list_copy(feature_sets->feature_sets_inout) ;
	  }

	/* I don't know if we need these, can get from context. */
	connect_data->feature_sets = feature_sets->feature_sets_inout ;
	connect_data->required_styles = feature_sets->required_styles_out ;

	break ;
      }
    case ZMAP_SERVERREQ_STYLES:
      {
	ZMapServerReqStyles get_styles = (ZMapServerReqStyles)req_any ;


	zMapStyleSetPrintAllStdOut(get_styles->styles_out, "styles from server", FALSE) ;


	zMapStyleSetPrintAllStdOut(zmap_view->orig_styles, "view orig styles", FALSE) ;

	/* Merge the retrieved styles into the views canonical style list. */
	zmap_view->orig_styles = zMapStyleMergeStyles(zmap_view->orig_styles, get_styles->styles_out,
						      ZMAPSTYLE_MERGE_PRESERVE) ;

	zMapStyleSetPrintAllStdOut(zmap_view->orig_styles, "view merged styles", FALSE) ;
	

	/* For dynamic loading the styles need to be set to load the features.*/
	if (connect_data->dynamic_loading)
	  {
	    g_datalist_foreach(&(get_styles->styles_out), unsetDeferredLoadStylesCB, NULL) ;
	  }


	/* Store the curr styles for use in creating the context and drawing features. */
	connect_data->curr_styles = get_styles->styles_out ;
	


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	/* THIS WILL HAVE TO CHANGE WHEN WE PUT STYLES SOMEWHERE ELSE.... */
	/* Store curr styles from this source in curr context. */
	connect_data->curr_context->styles = get_styles->styles_out ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


	break ;
      }
    case ZMAP_SERVERREQ_NEWCONTEXT:
      {
	break ;
      }
    case ZMAP_SERVERREQ_FEATURES:
    case ZMAP_SERVERREQ_SEQUENCE:
      {
	ZMapServerReqGetFeatures get_features = (ZMapServerReqGetFeatures)req_any ;

	if (req_any->type == ZMAP_SERVERREQ_FEATURES)
	  {
	    char *missing_styles = NULL ;


	    if (!(connect_data->server_styles_have_mode)
		&& !zMapFeatureAnyAddModesToStyles((ZMapFeatureAny)(connect_data->curr_context),
						   connect_data->curr_styles))
	      {
		zMapLogWarning("Source %s, inferring Style modes from Features failed.",
			       view_con->url) ;
		    
		result = FALSE ;
	      }

	    if (!(connect_data->server_styles_have_mode)
		&& !zMapFeatureAnyAddModesToStyles((ZMapFeatureAny)(connect_data->curr_context),
						   zmap_view->orig_styles))
	      {
		zMapLogWarning("Source %s, inferring Style modes from Features failed.",
			       view_con->url) ;
		    
		result = FALSE ;
	      }


	    /* I'm not sure if this couldn't come much earlier actually....something
	     * to investigate.... */

	    if (result && !makeStylesDrawable(connect_data->curr_styles, &missing_styles))
	      {
		zMapLogWarning("Failed to make following styles drawable: %s", missing_styles) ;

		result = FALSE ;
	      }

	    if (result && !makeStylesDrawable(zmap_view->orig_styles, &missing_styles))
	      {
		zMapLogWarning("Failed to make following styles drawable: %s", missing_styles) ;

		result = FALSE ;
	      }
	  }

	/* ok...once we are here we can display stuff.... */
	if (result && req_any->type == connect_data->last_request)
	  {
	    /* Isn't there a problem here...which bit of info goes with which server ???? */
	    zmapViewSessionAddServerInfo(zmap_view->session_data, connect_data->database_path) ;

	    getFeatures(zmap_view, get_features, connect_data->curr_styles) ;

	    zmap_view->connections_loaded++ ;

	    /* This will need to be more sophisticated, we will need to time
	     * connections out. */
	    if (zmap_view->connections_loaded == g_list_length(zmap_view->connection_list))
	      zmap_view->state = ZMAPVIEW_LOADED ;
	  }

	break ;
      }

    default:
      {	  
	zMapLogFatalLogicErr("switch(), unknown value: %d", req_any->type) ;
	result = FALSE ;

	break ;
      }
    }

  return result ;
}

static void freeDataRequest(ZMapViewConnection view_con, ZMapServerReqAny req_any)
{
  g_free(view_con->request_data) ;
  view_con->request_data = NULL ;

  zMapServerCreateRequestDestroy(req_any) ;

  return ;
}



/* 
 *      Callbacks for getting local sequences for passing to blixem.
 */
static gboolean processGetSeqRequests(ZMapViewConnection view_con, ZMapServerReqAny req_any)
{
  gboolean result = FALSE ;
  ZMapView zmap_view = view_con->parent_view ;

  if (req_any->type == ZMAP_SERVERREQ_GETSEQUENCE)
    {
      ZMapServerReqGetSequence get_sequence = (ZMapServerReqGetSequence)req_any ;
      GPid blixem_pid ;
      gboolean status ;

      /* Got the sequences so launch blixem. */
      if ((status = zmapViewCallBlixem(zmap_view,
				       get_sequence->orig_feature, 
				       get_sequence->sequences,
				       get_sequence->flags,
				       &blixem_pid,
				       &(zmap_view->kill_blixems))))
	zmap_view->spawned_processes = g_list_append(zmap_view->spawned_processes,
						     GINT_TO_POINTER(blixem_pid)) ;

      result = TRUE ;
    }
  else
    {	  
      zMapLogFatalLogicErr("wrong request type: %d", req_any->type) ;

      result = FALSE ;
    }


  return result ;
}







/* Calls the control window callback to remove any reference to the zmap and then destroys
 * the actual zmap itself.
 *  */
static void killGUI(ZMapView zmap_view)
{
  killAllWindows(zmap_view) ;

  return ;
}


static void killConnections(ZMapView zmap_view)
{
  GList* list_item ;

  zMapAssert(zmap_view->connection_list) ;

  list_item = g_list_first(zmap_view->connection_list) ;
  do
    {
      ZMapViewConnection view_con ;
      ZMapThread thread ;

      view_con = list_item->data ;
      thread = view_con->thread ;

      /* NOTE, we do a _kill_ here, not a destroy. This just signals the thread to die, it
       * will actually die sometime later. */
      zMapThreadKill(thread) ;
    }
  while ((list_item = g_list_next(list_item))) ;

  return ;
}





static void invoke_merge_in_names(gpointer list_data, gpointer user_data)
{
  ZMapViewWindow view_window = (ZMapViewWindow)list_data;
  GList *feature_set_names = (GList *)user_data;

  /* This relies on hokey code... with a number of issues...
   * 1) the window function only concats the lists.
   * 2) this view code might well want to do the merge?
   * 3) how do we order all these columns?
   */
  zMapWindowMergeInFeatureSetNames(view_window->window, feature_set_names);

  return ;
}


/* Allocate a connection and send over the request to get the sequence displayed. */
static ZMapViewConnection createConnection(ZMapView zmap_view,
					   ZMapURL url, char *format,
					   int timeout, char *version,
					   char *styles, char *styles_file,
					   char *featuresets_names, char *navigator_set_names,
					   gboolean sequence_server, gboolean writeback_server,
					   char *sequence, int start, int end)
{
  ZMapViewConnection view_con = NULL ;
  GList *req_featuresets = NULL, *tmp_navigator_sets = NULL ;
  ZMapThread thread ;
  gboolean status = TRUE ;
  gboolean dna_requested = FALSE ;
  ConnectionData connect_data ;


  /* User can specify feature set names that should be displayed in an ordered list. Order of
   * list determines order of columns. */
  if (featuresets_names)
    {
      /* If user only wants some featuresets displayed then build a list of their quark names. */
      req_featuresets = zMapFeatureString2QuarkList(featuresets_names) ;


      /* Check whether dna was requested, see comments below about setting up sequence req. */
      if ((zMap_g_list_find_quark(req_featuresets, zMapStyleCreateID(ZMAP_FIXED_STYLE_DNA_NAME))))
	dna_requested = TRUE ;

      g_list_foreach(zmap_view->window_list, invoke_merge_in_names, req_featuresets);

    }

  /* Navigator styles are all predefined so no need to merge into main feature sets to get the styles. */
  if (navigator_set_names)
    {
      tmp_navigator_sets = zmap_view->navigator_set_names = zMapFeatureString2QuarkList(navigator_set_names);

      /* We _must_ merge the set names into the navigator though. */
      /* The navigator knows nothing of view, so saving them there isn't really useful for getting them drawn.
       * N.B. This is zMapWindowNavigatorMergeInFeatureSetNames _not_ zMapWindowMergeInFeatureSetNames. */
      if(zmap_view->navigator_window)
        zMapWindowNavigatorMergeInFeatureSetNames(zmap_view->navigator_window, tmp_navigator_sets);
    }

  /* Create the thread to service the connection requests, we give it a function that it will call
   * to decode the requests we send it and a terminate function. */
  if (status && (thread = zMapThreadCreate(zMapServerRequestHandler, zMapServerTerminateHandler)))
    {
      ZMapViewConnectionRequest request ;
      ZMapFeatureContext context ;
      ZMapServerReqAny req_any ;


      /* Create the connection struct. */
      view_con = g_new0(ZMapViewConnectionStruct, 1) ;


      /* THESE MUST GO ONCE STEP LIST SET UP PROPERLY..... */
      view_con->sequence_server = sequence_server ;
      view_con->writeback_server = writeback_server ;


      view_con->parent_view = zmap_view ;

      view_con->curr_request = ZMAPTHREAD_REQUEST_EXECUTE ;

      view_con->thread = thread ;

      view_con->url = g_strdup(url->url) ;

      /* Create data specific to this step list...and set it in the connection. */
      context = createContext(sequence, start, end, req_featuresets) ;

      connect_data = g_new0(ConnectionDataStruct, 1) ;
      connect_data->curr_context = context ;

      view_con->request_data = connect_data ;
      

      /* CHECK WHAT THIS IS ABOUT.... */
      /* Record info. for this session. */
      zmapViewSessionAddServer(zmap_view->session_data,
			       url,
			       format) ;


      /* Set up this connection in the step list. */
      req_any = zMapServerRequestCreate(ZMAP_SERVERREQ_CREATE, url, format, timeout, version) ;
      request = zmapViewStepListAddServerReq(zmap_view->step_list, view_con, ZMAP_SERVERREQ_CREATE, req_any) ;
      req_any = zMapServerRequestCreate(ZMAP_SERVERREQ_OPEN) ;
      zmapViewStepListAddServerReq(zmap_view->step_list, view_con, ZMAP_SERVERREQ_OPEN, req_any) ;
      req_any = zMapServerRequestCreate(ZMAP_SERVERREQ_GETSERVERINFO) ;
      zmapViewStepListAddServerReq(zmap_view->step_list, view_con, ZMAP_SERVERREQ_GETSERVERINFO, req_any) ;
      req_any = zMapServerRequestCreate(ZMAP_SERVERREQ_FEATURESETS, req_featuresets) ;
      zmapViewStepListAddServerReq(zmap_view->step_list, view_con, ZMAP_SERVERREQ_FEATURESETS, req_any) ;
      req_any = zMapServerRequestCreate(ZMAP_SERVERREQ_STYLES, styles, styles_file) ;
      zmapViewStepListAddServerReq(zmap_view->step_list, view_con, ZMAP_SERVERREQ_STYLES, req_any) ;
      req_any = zMapServerRequestCreate(ZMAP_SERVERREQ_NEWCONTEXT, context) ;
      zmapViewStepListAddServerReq(zmap_view->step_list, view_con, ZMAP_SERVERREQ_NEWCONTEXT, req_any) ;
      req_any = zMapServerRequestCreate(ZMAP_SERVERREQ_FEATURES) ;
      zmapViewStepListAddServerReq(zmap_view->step_list, view_con, ZMAP_SERVERREQ_FEATURES, req_any) ;
      if (sequence_server && dna_requested)
	{
	  req_any = zMapServerRequestCreate(ZMAP_SERVERREQ_SEQUENCE) ;
	  zmapViewStepListAddServerReq(zmap_view->step_list, view_con, ZMAP_SERVERREQ_SEQUENCE, req_any) ;
	}

      if (sequence_server && dna_requested)
	connect_data->last_request = ZMAP_SERVERREQ_SEQUENCE ;
      else
	connect_data->last_request = ZMAP_SERVERREQ_FEATURES ;
    }


  return view_con ;
}



static void destroyConnection(ZMapViewConnection *view_conn_ptr)
{
  ZMapViewConnection view_conn = *view_conn_ptr ;

  zMapThreadDestroy(view_conn->thread) ;

  g_free(view_conn->url) ;

  /* Need to destroy the types array here....... */

  g_free(view_conn) ;

  *view_conn_ptr = NULL ;

  return ;
}


/* set all the windows attached to this view so that they contain nothing. */
static void resetWindows(ZMapView zmap_view)
{
  GList* list_item ;

  list_item = g_list_first(zmap_view->window_list) ;

  do
    {
      ZMapViewWindow view_window ;

      view_window = list_item->data ;

      zMapWindowReset(view_window->window) ;
    }
  while ((list_item = g_list_next(list_item))) ;

  return ;
}



/* Signal all windows there is data to draw. */
static void displayDataWindows(ZMapView zmap_view,
			       ZMapFeatureContext all_features, 
                               ZMapFeatureContext new_features, GData *new_styles,
                               gboolean undisplay)
{
  GList *list_item, *window_list  = NULL;
  gboolean clean_required = FALSE;

  list_item = g_list_first(zmap_view->window_list) ;

  /* when the new features aren't the stored features i.e. not the first draw */
  /* un-drawing the features doesn't work the same way as drawing */
  if(all_features != new_features && !undisplay)
    {
      clean_required = TRUE;
    }

  do
    {
      ZMapViewWindow view_window ;

      view_window = list_item->data ;

      zMapStyleSetPrintAllStdOut(view_window->parent_view->orig_styles, "display styles", FALSE) ;

      if (!undisplay)
        zMapWindowDisplayData(view_window->window, NULL, all_features, new_features,
			      view_window->parent_view->orig_styles, new_styles) ;
      else
        zMapWindowUnDisplayData(view_window->window, all_features, new_features);

      if(clean_required)
        window_list = g_list_append(window_list, view_window->window);
    }
  while ((list_item = g_list_next(list_item))) ;

  if(clean_required)
    zmapViewCWHSetList(zmap_view->cwh_hash, new_features, window_list);

  return ;
}


static void loaded_dataCB(ZMapWindow window, void *caller_data, void *window_data)
{
  ZMapFeatureContext context = (ZMapFeatureContext)window_data;
  ZMapViewWindow view_window = (ZMapViewWindow)caller_data;
  ZMapView view;
  gboolean removed, debug = FALSE, unique_context;

  view = zMapViewGetView(view_window);

  if(debug)
    zMapLogWarning("%s", "Attempting to Destroy Diff Context");

  removed = zmapViewCWHRemoveContextWindow(view->cwh_hash, &context, window, &unique_context);

  if(debug)
    {
      if(removed)
        zMapLogWarning("%s", "Context was destroyed");
      else if(unique_context)
        zMapLogWarning("%s", "Context is the _only_ context");
      else
        zMapLogWarning("%s", "Another window still needs the context memory");
    }

  if(removed || unique_context)
    (*(view_cbs_G->load_data))(view, view->app_data, NULL) ;

  return ;
}


/* Kill all the windows... */
static void killAllWindows(ZMapView zmap_view)
{

  while (zmap_view->window_list)
    {
      ZMapViewWindow view_window ;

      view_window = zmap_view->window_list->data ;

      destroyWindow(zmap_view, view_window) ;
    }


  return ;
}


static ZMapViewWindow createWindow(ZMapView zmap_view, ZMapWindow window)
{
  ZMapViewWindow view_window ;
  
  view_window = g_new0(ZMapViewWindowStruct, 1) ;
  view_window->parent_view = zmap_view ;		    /* back pointer. */
  view_window->window = window ;

  return view_window ;
}


static void destroyWindow(ZMapView zmap_view, ZMapViewWindow view_window)
{
  zmap_view->window_list = g_list_remove(zmap_view->window_list, view_window) ;

  zMapWindowDestroy(view_window->window) ;

  g_free(view_window) ;

  return ;
}



static void getFeatures(ZMapView zmap_view, ZMapServerReqGetFeatures feature_req, GData *styles)
{
  ZMapFeatureContext new_features = NULL ;

  zMapPrintTimer(NULL, "Got Features from Thread") ;

  /* May be a new context or a merge with an existing one. */
  if (feature_req->context)
    {
      new_features = feature_req->context ;

      /* Truth means view->features == new_features i.e. first time round */
      if (!mergeAndDrawContext(zmap_view, new_features, styles))
	feature_req->context = NULL ;
    }

  return ;
}



/* Error handling is rubbish here...stuff needs to be free whether there is an error or not.
 * 
 * new_features should be freed (but not the data...ahhhh actually the merge should
 * free any replicated data...yes, that is what should happen. Then when it comes to
 * the diff we should not free the data but should free all our structs...
 * 
 * We should free the context_inout context here....actually better
 * would to have a "free" flag............ 
 *  */
static gboolean mergeAndDrawContext(ZMapView view, ZMapFeatureContext context_in, GData *styles)
{
  ZMapFeatureContext diff_context = NULL ;
  gboolean identical_contexts = FALSE, free_diff_context = FALSE;

  if (justMergeContext(view, &context_in, styles))
    {
      if (diff_context == context_in)
        identical_contexts = TRUE;

      diff_context = context_in;

      free_diff_context = !(identical_contexts);

      justDrawContext(view, diff_context, styles);
    }
  else
    zMapLogCritical("%s", "Unable to draw diff context after mangled merge!");

  return identical_contexts;
}

static gboolean justMergeContext(ZMapView view, ZMapFeatureContext *context_inout, GData *styles)
{
  ZMapFeatureContext new_features, diff_context = NULL ;
  gboolean merged = FALSE;

  new_features = *context_inout ;

  zMapPrintTimer(NULL, "Merge Context starting...") ;

  if (view->revcomped_features)
    {
      zMapPrintTimer(NULL, "Merge Context has to rev comp first, starting") ;
        
      zMapFeatureReverseComplement(new_features, view->orig_styles);

      zMapPrintTimer(NULL, "Merge Context has to rev comp first, finished") ;
    }

  /* Need to stick the styles in here..... */


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE

  /* THIS MAY ALREADY BE DONE.....GO THROUGH LOGIC..... */

  /* Here is the code from zMapFeatureContextMerge for the styles merge... */
      /* Merge the styles from the new context into the existing context. */
      current_context->styles = zMapStyleMergeStyles(current_context->styles,
						     new_context->styles, ZMAPSTYLE_MERGE_MERGE) ;

      /* Make the diff_context point at the merged styles, not its own copies... */
      replaceStyles((ZMapFeatureAny)new_context, &(current_context->styles)) ;



#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  if (!(merged = zMapFeatureContextMerge(&(view->features), &new_features, &diff_context)))
    {
      zMapLogCritical("%s", "Cannot merge feature data from....") ;
    }
  else
    {
      zMapLogWarning("%s", "Helpful message to say merge went well...");
    }

  zMapPrintTimer(NULL, "Merge Context Finished.") ;

  /* Return the diff_context which is the just the new features (NULL if merge fails). */
  *context_inout = diff_context ;

  return merged;
}

static void justDrawContext(ZMapView view, ZMapFeatureContext diff_context, GData *new_styles)
{
   /* Signal the ZMap that there is work to be done. */
  displayDataWindows(view, view->features, diff_context, new_styles, FALSE) ;
 
  /* Not sure about the timing of the next bit. */
 
  /* We have to redraw the whole navigator here.  This is a bit of
   * a pain, but it's due to the scaling we do to make the rest of
   * the navigator work.  If the length of the sequence changes the 
   * all the previously drawn features need to move.  It also 
   * negates the need to keep state as to the length of the sequence,
   * the number of times the scale bar has been drawn, etc... */
  zMapWindowNavigatorReset(view->navigator_window); /* So reset */
  zMapWindowNavigatorSetStrand(view->navigator_window, view->revcomped_features);
  /* and draw with _all_ the view's features. */
  zMapWindowNavigatorDrawFeatures(view->navigator_window, view->features, view->orig_styles);
  
  /* signal our caller that we have data. */
  (*(view_cbs_G->load_data))(view, view->app_data, NULL) ;
  
  return ;
}

static void eraseAndUndrawContext(ZMapView view, ZMapFeatureContext context_inout)
{                              
  ZMapFeatureContext diff_context = NULL;

  if(!zMapFeatureContextErase(&(view->features), context_inout, &diff_context))
    zMapLogCritical("%s", "Cannot erase feature data from...");
  else
    {
      displayDataWindows(view, view->features, diff_context, NULL, TRUE);
      
      zMapFeatureContextDestroy(diff_context, TRUE);
    }

  return ;
}



/* Layer below wants us to execute a command. */
static void commandCB(ZMapWindow window, void *caller_data, void *window_data)
{
  ZMapViewWindow view_window = (ZMapViewWindow)caller_data ;
  ZMapView view = view_window->parent_view ;
  ZMapWindowCallbackCommandAny cmd_any = (ZMapWindowCallbackCommandAny)window_data ;

  switch(cmd_any->cmd)
    {
    case ZMAPWINDOW_CMD_SHOWALIGN:
      {
	ZMapWindowCallbackCommandAlign align_cmd = (ZMapWindowCallbackCommandAlign)cmd_any ;
	gboolean status ;
	GList *local_sequences = NULL ;
	ZMapViewBlixemFlags flags = BLIXEM_NO_FLAG;
	
	if (align_cmd->obey_protein_featuresets)
	  flags |= BLIXEM_OBEY_PROTEIN_SETS;
	
	if (align_cmd->obey_dna_featuresets)
	  flags |= BLIXEM_OBEY_DNA_SETS;
	
	if ((status = zmapViewBlixemLocalSequences(view, align_cmd->feature, &local_sequences)))
	  {
	    if (!view->sequence_server)
	      {
		zMapWarning("%s", "No sequence server was specified so cannot fetch raw sequences for blixem.") ;
	      }
	    else if (view->step_list)
	      {
		zMapLogCritical("GUI: %s", "View is already executing connection requests"
				" so cannot fetch local sequences for blixem.") ;
		
	      }
	    else
	      {
		ZMapViewConnection view_con ;
		ZMapViewConnectionRequest request ;
		ZMapServerReqAny req_any ;

		view_con = view->sequence_server ;

		if (!(view_con->sequence_server))
		  {
		    zMapWarning("%s", "Sequence server incorrectly specified in config file"
				" so cannot fetch local sequences for blixem.") ;
		  }
		else
		  {
		    /* Create the step list that will be used to fetch the sequences. */
		    view->on_fail = REQUEST_ONFAIL_CANCEL_REQUEST ;
		    view->step_list = zmapViewStepListCreate(NULL, processGetSeqRequests, NULL) ;
		    zmapViewStepListAddStep(view->step_list, ZMAP_SERVERREQ_GETSEQUENCE) ;

		    /* Add the request to the step list. */
		    req_any = zMapServerRequestCreate(ZMAP_SERVERREQ_GETSEQUENCE,
						      align_cmd->feature, local_sequences, flags) ;
		    request = zmapViewStepListAddServerReq(view->step_list,
							   view_con, ZMAP_SERVERREQ_GETSEQUENCE, req_any) ;
		    
		    /* Start the step list. */
		    zmapViewStepListIter(view->step_list) ;
		  }
	      }
	  }
	else
	  {
	    GPid blixem_pid ;

	    if ((status = zmapViewCallBlixem(view, align_cmd->feature, NULL, flags, &blixem_pid, &(view->kill_blixems))))
	      view->spawned_processes = g_list_append(view->spawned_processes, GINT_TO_POINTER(blixem_pid)) ;
	  }

	break ;
      }

    case ZMAPWINDOW_CMD_GETFEATURES:
      {
	ZMapWindowCallbackGetFeatures get_data = (ZMapWindowCallbackGetFeatures)cmd_any ;

	loadFeatures(view, get_data) ;

	break ;
      }


    case ZMAPWINDOW_CMD_REVERSECOMPLEMENT:
      {
	gboolean status ;

	/* NOTE, there is no need to signal the layer above that things are changing,
	 * the layer above does the complement and handles all that. */
	if (!(status = zMapViewReverseComplement(view)))
	  {
	    zMapLogCritical("%s", "View Reverse Complement failed.") ;

	    zMapWarning("%s", "View Reverse Complement failed.") ;
	  }

	break ;
      }
    default:
      {
	zMapAssertNotReached() ;
	break ;
      }
    }


  return ;
}


static void viewVisibilityChangeCB(ZMapWindow window, void *caller_data, void *window_data)
{
  ZMapViewWindow view_window = (ZMapViewWindow)caller_data ;
  ZMapWindowVisibilityChange vis = (ZMapWindowVisibilityChange)window_data;

  /* signal our caller that something has changed. */
  (*(view_cbs_G->visibility_change))(view_window, view_window->parent_view->app_data, window_data) ;

  /* view_window->window can be NULL (when window copying) so we use the passed in window... */
  /* Yes it's a bit messy, but it's stopping it crashing. */
  zMapWindowNavigatorSetCurrentWindow(view_window->parent_view->navigator_window, window);

  zMapWindowNavigatorDrawLocator(view_window->parent_view->navigator_window, vis->scrollable_top, vis->scrollable_bot);

  return;
}

/* When a window is split, we set the zoom status of all windows */
static void setZoomStatusCB(ZMapWindow window, void *caller_data, void *window_data)
{
  ZMapViewWindow view_window = (ZMapViewWindow)caller_data ;

  g_list_foreach(view_window->parent_view->window_list, setZoomStatus, NULL) ;

  return;
}

static void setZoomStatus(gpointer data, gpointer user_data)
{
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  ZMapViewWindow view_window = (ZMapViewWindow)data ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
  return;
}



/* Trial code to get alignments from a file and create a context...... */
static ZMapFeatureContext createContext(char *sequence, int start, int end, GList *feature_set_names)
{
  ZMapFeatureContext context = NULL ;
  gboolean master = TRUE ;
  ZMapFeatureAlignment alignment ;
  ZMapFeatureBlock block ;

  context = zMapFeatureContextCreate(sequence, start, end, feature_set_names) ;

  /* Add the master alignment and block. */
  alignment = zMapFeatureAlignmentCreate(sequence, master) ; /* TRUE => master alignment. */

  zMapFeatureContextAddAlignment(context, alignment, master) ;

  block = zMapFeatureBlockCreate(sequence,
				 start, end, ZMAPSTRAND_FORWARD,
				 start, end, ZMAPSTRAND_FORWARD) ;

  zMapFeatureAlignmentAddBlock(alignment, block) ;


  /* Add other alignments if any were specified in a config file. */
  addAlignments(context) ;


  return context ;
}


/* Add other alignments if any specified in a config file. */
static void addAlignments(ZMapFeatureContext context)
{
#ifdef THIS_NEEDS_REDOING
  ZMapConfigStanzaSet blocks_list = NULL ;
  ZMapConfig config ;
  char *config_file ;
  char *stanza_name = "align" ;
  gboolean result = FALSE ;
  char *ref_seq ;
  int start, end ;
  ZMapFeatureAlignment alignment = NULL ;


  ref_seq = (char *)g_quark_to_string(context->sequence_name) ;
  start = context->sequence_to_parent.c1 ;
  end = context->sequence_to_parent.c2 ;


  config_file = zMapConfigDirGetFile() ;
  if ((config = zMapConfigCreateFromFile(config_file)))
    {
      ZMapConfigStanza block_stanza ;
      ZMapConfigStanzaElementStruct block_elements[]
	= {{"reference_seq"        , ZMAPCONFIG_STRING , {NULL}},
	   {"reference_start"      , ZMAPCONFIG_INT    , {NULL}},
	   {"reference_end"        , ZMAPCONFIG_INT    , {NULL}},
	   {"reference_strand"     , ZMAPCONFIG_INT    , {NULL}},
	   {"non_reference_seq"    , ZMAPCONFIG_STRING , {NULL}},
	   {"non_reference_start"  , ZMAPCONFIG_INT    , {NULL}},
	   {"non_reference_end"    , ZMAPCONFIG_INT    , {NULL}},
	   {"non_reference_strand" , ZMAPCONFIG_INT    , {NULL}},
	   {NULL, -1, {NULL}}} ;

      /* init non string elements. */
      zMapConfigGetStructInt(block_elements, "reference_start")      = 0 ;
      zMapConfigGetStructInt(block_elements, "reference_end")        = 0 ;
      zMapConfigGetStructInt(block_elements, "reference_strand")     = 0 ;
      zMapConfigGetStructInt(block_elements, "non_reference_start")  = 0 ;
      zMapConfigGetStructInt(block_elements, "non_reference_end")    = 0 ;
      zMapConfigGetStructInt(block_elements, "non_reference_strand") = 0 ;

      block_stanza = zMapConfigMakeStanza(stanza_name, block_elements) ;

      result = zMapConfigFindStanzas(config, block_stanza, &blocks_list) ;
    }

  if (result)
    {
      ZMapConfigStanza next_block = NULL;

      while ((next_block = zMapConfigGetNextStanza(blocks_list, next_block)) != NULL)
        {
          ZMapFeatureBlock data_block = NULL ;
	  char *reference_seq, *non_reference_seq ;
	  int reference_start, reference_end, non_reference_start, non_reference_end ;
	  ZMapStrand ref_strand = ZMAPSTRAND_REVERSE, non_strand = ZMAPSTRAND_REVERSE ;
	  int diff ;

	  reference_seq = zMapConfigGetElementString(next_block, "reference_seq") ;
	  reference_start = zMapConfigGetElementInt(next_block, "reference_start") ;
	  reference_end = zMapConfigGetElementInt(next_block, "reference_end") ;
	  non_reference_seq = zMapConfigGetElementString(next_block, "non_reference_seq") ;
	  non_reference_start = zMapConfigGetElementInt(next_block, "non_reference_start") ;
	  non_reference_end = zMapConfigGetElementInt(next_block, "non_reference_end") ;
	  if (zMapConfigGetElementInt(next_block, "reference_strand"))
	    ref_strand = ZMAPSTRAND_FORWARD ;
	  if (zMapConfigGetElementInt(next_block, "non_reference_strand"))
	    non_strand = ZMAPSTRAND_FORWARD ;


	  /* We only add aligns/blocks that are for the relevant reference sequence and within
	   * the start/end for that sequence. */
	  if (g_ascii_strcasecmp(ref_seq, reference_seq) == 0
	      && !(reference_start > end || reference_end < start))
	    {

	      if (!alignment)
		{
		  /* Add the other alignment, note that we do this dumbly at the moment assuming
		   * that there is only one other alignment */
		  alignment = zMapFeatureAlignmentCreate(non_reference_seq, FALSE) ;
	  
		  zMapFeatureContextAddAlignment(context, alignment, FALSE) ;
		}


	      /* Add the block for this set of data. */

	      /* clamp coords...SHOULD WE DO THIS, PERHAPS WE JUST REJECT THINGS THAT DON'T FIT ? */
	      if ((diff = start - reference_start) > 0)
		{
		  reference_start += diff ;
		  non_reference_start += diff ;
		}
	      if ((diff = reference_end - end) > 0)
		{
		  reference_end -= diff ;
		  non_reference_end -= diff ;
		}

	      data_block = zMapFeatureBlockCreate(non_reference_seq,
						  reference_start, reference_end, ref_strand,
						  non_reference_start, non_reference_end, non_strand) ;

	      zMapFeatureAlignmentAddBlock(alignment, data_block) ;
	    }
        }
    }
#endif /* THIS_NEEDS_REDOING */
  return ;
}




#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* Creates a copy of a context from the given block upwards and sets the start/end
 * of features within the block. */
static ZMapFeatureContext createContextCopyFromBlock(ZMapFeatureBlock block, GList *feature_set_names,
						     int features_start, int features_end)
{
  ZMapFeatureContext context = NULL ;
  gboolean master = TRUE ;
  ZMapFeatureAlignment alignment ;


  context = zMapFeatureContextCreate(sequence, start, end, feature_set_names) ;

  /* Add the master alignment and block. */
  alignment = zMapFeatureAlignmentCreate(sequence, master) ; /* TRUE => master alignment. */

  zMapFeatureContextAddAlignment(context, alignment, master) ;

  block = zMapFeatureBlockCreate(sequence,
				 start, end, ZMAPSTRAND_FORWARD,
				 start, end, ZMAPSTRAND_FORWARD) ;

  zMapFeatureBlockSetFeaturesCoords(block, features_start, features_end) ;

  zMapFeatureAlignmentAddBlock(alignment, block) ;


  return context ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */







#ifdef NOT_REQUIRED_ATM

/* Read list of sequence to server mappings (i.e. which sequences must be fetched from which
 * servers) from the zmap config file. */
static gboolean getSequenceServers(ZMapView zmap_view, char *config_str)
{
  gboolean result = FALSE ;
  ZMapConfig config ;
  ZMapConfigStanzaSet zmap_list = NULL ;
  ZMapConfigStanza zmap_stanza ;
  char *zmap_stanza_name = ZMAPSTANZA_APP_CONFIG ;
  ZMapConfigStanzaElementStruct zmap_elements[] = {{ZMAPSTANZA_APP_SEQUENCE_SERVERS, ZMAPCONFIG_STRING, {NULL}},
						   {NULL, -1, {NULL}}} ;

  if ((config = getConfigFromBufferOrFile(config_str)))
    {
      zmap_stanza = zMapConfigMakeStanza(zmap_stanza_name, zmap_elements) ;

      if (zMapConfigFindStanzas(config, zmap_stanza, &zmap_list))
	{
	  ZMapConfigStanza next_zmap = NULL ;

	  /* We only read the first of these stanzas.... */
	  if ((next_zmap = zMapConfigGetNextStanza(zmap_list, next_zmap)) != NULL)
	    {
	      char *server_seq_str ;

	      if ((server_seq_str
		   = zMapConfigGetElementString(next_zmap, ZMAPSTANZA_APP_SEQUENCE_SERVERS)))
		{
		  char *sequence, *server ;
		  ZMapViewSequence2Server seq_2_server ;
		  char *search_str ;

		  search_str = server_seq_str ;
		  while ((sequence = strtok(search_str, " ")))
		    {
		      search_str = NULL ;
		      server = strtok(NULL, " ") ;

		      seq_2_server = createSeq2Server(sequence, server) ;

		      zmap_view->sequence_2_server = g_list_append(zmap_view->sequence_2_server, seq_2_server) ;
		    }
		}
	    }

	  zMapConfigDeleteStanzaSet(zmap_list) ;		    /* Not needed anymore. */
	}
      
      zMapConfigDestroyStanza(zmap_stanza) ;
      
      zMapConfigDestroy(config) ;

      result = TRUE ;
    }

  return result ;
}


static void destroySeq2ServerCB(gpointer data, gpointer user_data_unused)
{
  ZMapViewSequence2Server seq_2_server = (ZMapViewSequence2Server)data ;

  destroySeq2Server(seq_2_server) ;

  return ;
}

static ZMapViewSequence2Server createSeq2Server(char *sequence, char *server)
{
  ZMapViewSequence2Server seq_2_server = NULL ;

  seq_2_server = g_new0(ZMapViewSequence2ServerStruct, 1) ;
  seq_2_server->sequence = g_strdup(sequence) ;
  seq_2_server->server = g_strdup(server) ;

  return seq_2_server ;
}


static void destroySeq2Server(ZMapViewSequence2Server seq_2_server)
{
  g_free(seq_2_server->sequence) ;
  g_free(seq_2_server->server) ;
  g_free(seq_2_server) ;

  return ;
}

/* Check to see if a sequence/server pair match any in the list mapping sequences to servers held
 * in the view. NOTE that if the sequence is not in the list at all then this function returns
 * TRUE as it is assumed by default that sequences can be fetched from any servers.
 * */
static gboolean checkSequenceToServerMatch(GList *seq_2_server, ZMapViewSequence2Server target_seq_server)
{
  gboolean result = FALSE ;
  GList *server_item ;
    
  /* If the sequence is in the list then check the server names. */
  if ((server_item = g_list_find_custom(seq_2_server, target_seq_server, findSequence)))
    {
      ZMapViewSequence2Server curr_seq_2_server = (ZMapViewSequence2Server)server_item->data ;

      if ((g_ascii_strcasecmp(curr_seq_2_server->server, target_seq_server->server) == 0))
	result = TRUE ;
    }
  else
    {
      /* Return TRUE if sequence not in list. */
      result = TRUE ;
    }

  return result ;
}


/* A GCompareFunc to check whether servers match in the sequence to server mapping. */
static gint findSequence(gconstpointer a, gconstpointer b)
{
  gint result = -1 ;
  ZMapViewSequence2Server seq_2_server = (ZMapViewSequence2Server)a ;
  ZMapViewSequence2Server target_fetch = (ZMapViewSequence2Server)b ;
    
  if ((g_ascii_strcasecmp(seq_2_server->sequence, target_fetch->sequence) == 0))
    result = 0 ;

  return result ;
}
#endif /* NOT_REQUIRED_ATM */

/* Hacky...sorry.... */
static void threadDebugMsg(ZMapThread thread, char *format_str, char *msg)
{
  char *thread_id ;
  char *full_msg ;

  thread_id = zMapThreadGetThreadID(thread) ;
  full_msg = g_strdup_printf(format_str, thread_id, msg ? msg : "") ;

  zMapDebug("%s", full_msg) ;

  g_free(full_msg) ;
  g_free(thread_id) ;

  return ;
}



/* check whether there are live connections or not, return TRUE if there are, FALSE otherwise. */
static gboolean connections(ZMapView zmap_view, gboolean threads_have_died)
{
  gboolean connections = FALSE ;

  switch (zmap_view->state)
    {
    case ZMAPVIEW_INIT:
      {
	/* Nothing to do here I think.... */
	connections = TRUE ;
	break ;
      }
    case ZMAPVIEW_CONNECTING:
    case ZMAPVIEW_CONNECTED:
    case ZMAPVIEW_LOADING:
    case ZMAPVIEW_LOADED:
      {
	/* We should kill the ZMap here with an error message....or allow user to reload... */

	if (threads_have_died)
	  {
	    /* Threads have died because of their own errors....but the ZMap is not dying so
	     * reset state to init and we should return an error here....(but how ?), we 
	     * should not be outputting messages....I think..... */
	    zMapWarning("%s", "Cannot show ZMap because server connections have all died or been cancelled.") ;
	    zMapLogWarning("%s", "Cannot show ZMap because server connections have all died or been cancelled.") ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	    /* Should we kill the whole window ??? */

	    killGUI(zmap_view) ;
	    destroyZMapView(zmap_view) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
	    /* need to reset here..... */


	    connections = FALSE ;
	  }
	/* I don't think the "else" should be possible......so perhaps we should fail if that
	 * happens..... */

	zmap_view->state = ZMAPVIEW_INIT ;

	break ;
      }
    case ZMAPVIEW_RESETTING:
      {
	/* zmap is ok but user has reset it and all threads have died so we need to stop
	 * checking.... */
	zmap_view->state = ZMAPVIEW_INIT ;

	/* Signal layer above us because the view has reset. */
	(*(view_cbs_G->state_change))(zmap_view, zmap_view->app_data, NULL) ;

	connections = FALSE ;

	break ;
      }
    case ZMAPVIEW_DYING:
      {
	/* Tricky coding here: we need to destroy the view _before_ we notify the layer above us
	 * that the zmap_view has gone but we need to pass information from the view back, so we
	 * keep a temporary record of certain parts of the view. */
	ZMapView zmap_view_ref = zmap_view ;
	void *app_data = zmap_view->app_data ;
	ZMapViewCallbackDestroyData destroy_data ;

	destroy_data = g_new(ZMapViewCallbackDestroyDataStruct, 1) ; /* Caller must free. */
	destroy_data->xwid = zmap_view->xwid ;

	/* view was waiting for threads to die, now they have we can free everything. */
	destroyZMapView(&zmap_view) ;

	/* Signal layer above us that view has died. */
	(*(view_cbs_G->destroy))(zmap_view_ref, app_data, destroy_data) ;

	connections = FALSE ;

	break ;
      }
    default:
      {
	zMapLogFatalLogicErr("switch(), unknown value: %d", zmap_view->state) ;

	break ;
      }
    }

  return connections ;
}







static gboolean makeStylesDrawable(GData *styles, char **missing_styles_out)
{
  gboolean result = FALSE ;
  DrawableDataStruct drawable_data = {FALSE} ;

  g_datalist_foreach(&styles, drawableCB, &drawable_data) ;

  if (drawable_data.missing_styles)
    *missing_styles_out = g_string_free(drawable_data.missing_styles, FALSE) ;

  result = drawable_data.found_style ;

  return result ;
}



/* A GDataForeachFunc() to make the given style drawable. */
static void drawableCB(GQuark key_id, gpointer data, gpointer user_data)
{
  ZMapFeatureTypeStyle style = (ZMapFeatureTypeStyle)data ;
  DrawableData drawable_data = (DrawableData)user_data ;

  if (zMapStyleIsDisplayable(style))
    {
      if (zMapStyleMakeDrawable(style))
	{
	  drawable_data->found_style = TRUE ;
	}
      else
	{
	  if (!(drawable_data->missing_styles))
	    drawable_data->missing_styles = g_string_sized_new(1000) ;

	  g_string_append_printf(drawable_data->missing_styles, "%s ", g_quark_to_string(key_id)) ;
	}
    }

  return ;
}



static void loadFeatures(ZMapView view, ZMapWindowCallbackGetFeatures get_data)
{
  ZMapViewConnection view_con ;
  ZMapServerReqAny req_any ;
  ZMapFeatureContext orig_context, context ;
  GList *req_featuresets = NULL ;
  ConnectionData connect_data ;
  ZMapFeatureBlock block_orig, block ;
  int features_start, features_end ;


  block_orig = get_data->block ;
  req_featuresets = get_data->feature_set_ids ;
  features_start = get_data->start ;
  features_end = get_data->end ;

  orig_context = (ZMapFeatureContext)zMapFeatureGetParentGroup((ZMapFeatureAny)block_orig,
							       ZMAPFEATURE_STRUCT_CONTEXT) ;

  /* Copy the original context from the target block upwards setting feature set names
   * and the range of features to be copied. */
  context = zMapFeatureContextCopyWithParents((ZMapFeatureAny)block_orig) ;

  context->feature_set_names = req_featuresets ;

  block = zMapFeatureAlignmentGetBlockByID(context->master_align, block_orig->unique_id) ;

  zMapFeatureBlockSetFeaturesCoords(block, features_start, features_end) ;


  /* Create the step list that will be used to control obtaining the feature
   * context from the multiple sources. */
  view->on_fail = REQUEST_ONFAIL_CONTINUE ;
  view->step_list = zmapViewStepListCreate(dispatchContextRequests,
					   processDataRequests,
					   freeDataRequest) ;
  zmapViewStepListAddStep(view->step_list, ZMAP_SERVERREQ_FEATURESETS) ;
  zmapViewStepListAddStep(view->step_list, ZMAP_SERVERREQ_STYLES) ;
  zmapViewStepListAddStep(view->step_list, ZMAP_SERVERREQ_NEWCONTEXT) ;
  zmapViewStepListAddStep(view->step_list, ZMAP_SERVERREQ_FEATURES) ;


  /* HACK...MAKE THIS DO ALL SERVERS..... */
  /* should add all servers ???? need to loop for all servers.....
   * for test just add one... */
  view_con = (ZMapViewConnection)(view->connection_list->data) ;


  /* Create data specific to this step list...and set it in the connection. */
  connect_data = g_new0(ConnectionDataStruct, 1) ;
  connect_data->curr_context = context ;
  connect_data->dynamic_loading = TRUE ;

  view_con->request_data = connect_data ;

  zmapViewStepListAddServerReq(view->step_list, view_con, ZMAP_SERVERREQ_GETSERVERINFO, req_any) ;
  req_any = zMapServerRequestCreate(ZMAP_SERVERREQ_FEATURESETS, req_featuresets) ;
  zmapViewStepListAddServerReq(view->step_list, view_con, ZMAP_SERVERREQ_FEATURESETS, req_any) ;
  req_any = zMapServerRequestCreate(ZMAP_SERVERREQ_STYLES, NULL, NULL) ;
  zmapViewStepListAddServerReq(view->step_list, view_con, ZMAP_SERVERREQ_STYLES, req_any) ;
  req_any = zMapServerRequestCreate(ZMAP_SERVERREQ_NEWCONTEXT, context) ;
  zmapViewStepListAddServerReq(view->step_list, view_con, ZMAP_SERVERREQ_NEWCONTEXT, req_any) ;
  req_any = zMapServerRequestCreate(ZMAP_SERVERREQ_FEATURES) ;
  zmapViewStepListAddServerReq(view->step_list, view_con, ZMAP_SERVERREQ_FEATURES, req_any) ;

  connect_data->last_request = ZMAP_SERVERREQ_FEATURES ;


  /* Start the step list. */
  zmapViewStepListIter(view->step_list) ;


  return ;
}



/* A GDataForeachFunc() func that unsets DeferredLoads for styles in the target. */
static void unsetDeferredLoadStylesCB(GQuark key_id, gpointer data, gpointer user_data_unused)
{
  ZMapFeatureTypeStyle orig_style = (ZMapFeatureTypeStyle)data ;

  zMapStyleSetDeferred(orig_style, FALSE) ;

  return ;
}
