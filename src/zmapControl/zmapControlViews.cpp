/*  File: zmapControlViews.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
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
 * Description: Splits the zmap window to show either the same view twice
 *              or two different views.
 *
 *              This is a complete rewrite of the original.
 *
 * Exported functions: See zmapControl.h
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>

#include <ZMap/zmapUtilsDebug.hpp>
#include <zmapControl_P.hpp>



/* Used to record which child we are of a pane widget. */
typedef enum {ZMAP_PANE_NONE, ZMAP_PANE_CHILD_1, ZMAP_PANE_CHILD_2} ZMapPaneChild ;


/* Used in doing a reverse lookup for the ZMapViewWindow -> container widget hash. */
typedef struct
{
  GtkWidget *widget ;
  ZMapViewWindow view_window ;
} ZMapControlWidget2ViewwindowStruct ;



typedef struct FindViewWindowStructName
{
  ZMapView view ;
  ZMapViewWindow view_window ;
} FindViewWindowStruct, *FindViewWindow ;



/* GTK didn't introduce a function call to get at pane children until at least release 2.6,
 * I hope this is correct, no easy way to check exactly when function calls were introduced.
 */
#if ((GTK_MAJOR_VERSION == 2) && (GTK_MINOR_VERSION == 6))
#define myGetChild(WIDGET, CHILD_NUMBER)         \
  (gtk_paned_get_child##CHILD_NUMBER(GTK_PANED(WIDGET)))
#else
#define myGetChild(WIDGET, CHILD_NUMBER)         \
  ((GTK_PANED(WIDGET))->child##CHILD_NUMBER)
#endif


static ZMapPaneChild whichChildOfPane(GtkWidget *child) ;
static void splitPane(GtkWidget *curr_frame, GtkWidget *new_frame,
		      GtkOrientation orientation, ZMapControlSplitOrder window_order) ;
static GtkWidget *closePane(GtkWidget *close_frame) ;

static ZMapViewWindow widget2ViewWindow(GHashTable* hash_table, GtkWidget *widget) ;
static void findViewWindow(gpointer key, gpointer value, gpointer user_data) ;
static ZMapViewWindow closeWindow(ZMap zmap, GtkWidget *close_container) ;
static void labelDestroyCB(GtkWidget *widget, gpointer cb_data) ;

static void findViewWindowCB(gpointer key, gpointer value, gpointer user_data) ;



/* 
 *                         External routines
 */



gpointer zMapControlFindView(ZMap zmap, gpointer view_id)
{
  gpointer view = NULL ;

  zMapReturnValIfFail(zmap, view) ; 

  if (zmap->view_list)
    {
      GList *list_view ;

      /* Try to find the view_id in the current zmaps. */
      list_view = g_list_first(zmap->view_list) ;
      do
	{
	  ZMapView next_view = (ZMapView)(list_view->data) ;

	  if (next_view == view_id
	      || (zMapViewFindView(next_view, view_id)))
	    {
	      view = next_view ;

	      break ;
	    }
	}
      while ((list_view = g_list_next(list_view))) ;
    }

  return view ;
}



/* 
 *                     Package external routines.
 */


/* New func for brand new view windows.... */
ZMapViewWindow zmapControlNewWindow(ZMap zmap, ZMapFeatureSequenceMap sequence_map)
{
  ZMapViewWindow view_window = NULL ;
  GtkWidget *curr_container, *view_container ;
  char *view_title ;
  GtkOrientation orientation = GTK_ORIENTATION_VERTICAL ;   /* arbitrary for first window. */

  zMapReturnValIfFail(zmap, view_window) ; 

  /* If there is a focus window then that will be the one we split and we need to find out
   * the container parent of that canvas. */
  if (zmap->focus_viewwindow)
    curr_container = (GtkWidget*)g_hash_table_lookup(zmap->viewwindow_2_parent, zmap->focus_viewwindow) ;
  else
    curr_container = NULL ;

  view_title = zMapViewGetSequenceName(sequence_map) ;


  /* Create a new container that will hold the new view window. */
  view_container = zmapControlAddWindow(zmap, curr_container, orientation, ZMAPCONTROL_SPLIT_FIRST, view_title) ;

  if (!(view_window = zMapViewCreate(view_container, sequence_map, (void *)zmap)))
    {
      /* remove window we just added....not sure we need to do anything with remaining view... */
      closeWindow(zmap, view_container) ;
    }
  else
    {
      ZMapInfoPanelLabels labels;
      GtkWidget *infopanel;
      ZMapView view ;

      view = zMapViewGetView(view_window) ;

      labels = g_new0(ZMapInfoPanelLabelsStruct, 1);

      infopanel = zmapControlWindowMakeInfoPanel(zmap, labels);

      labels->hbox = infopanel;
      gtk_signal_connect(GTK_OBJECT(labels->hbox), "destroy",
			 GTK_SIGNAL_FUNC(labelDestroyCB), (gpointer)labels) ;

      g_hash_table_insert(zmap->view2infopanel, view, labels);

      gtk_box_pack_end(GTK_BOX(zmap->info_panel_vbox), infopanel, FALSE, FALSE, 0);

      zMapViewSetupNavigator(view_window, zmap->nav_canvas) ;

      /* Add to hash of viewwindows to frames */
      g_hash_table_insert(zmap->viewwindow_2_parent, view_window, view_container) ;

      /* If there is no current focus window we need to make this new one the focus,
       * if we don't of code will fail because it relies on having a focus window and
       * the user will not get visual feedback that a window is the focus window. */
      if (!zmap->focus_viewwindow)
	zmapControlSetWindowFocus(zmap, view_window) ;

      /* We'll need to update the display..... */
      gtk_widget_show_all(zmap->toplevel) ;

      zmapControlWindowSetGUIState(zmap) ;
    }

  return view_window ;
}



ZMapViewWindow zmapControlNewWidgetAndWindowForView(ZMap zmap,
                                                    ZMapView zmap_view,
                                                    ZMapWindow zmap_window,
                                                    GtkWidget *curr_container,
                                                    GtkOrientation orientation,
						    ZMapControlSplitOrder window_order,
                                                    char *view_title)
{
  GtkWidget *view_container = NULL ;
  ZMapViewWindow view_window = NULL ;
  ZMapWindowLockType window_locking = ZMAP_WINLOCK_NONE ;

  zMapReturnValIfFail(zmap, view_window) ; 

  /* Add a new container that will hold the new view window. */
  view_container = zmapControlAddWindow(zmap, curr_container, orientation, window_order, view_title) ;

  /* Copy the focus view window. */
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    window_locking = ZMAP_WINLOCK_HORIZONTAL ;
  else if (orientation == GTK_ORIENTATION_VERTICAL)
    window_locking = ZMAP_WINLOCK_VERTICAL ;

  view_window = zMapViewCopyWindow(zmap_view, view_container, zmap_window, window_locking) ;

  /* Add to hash of viewwindows to frames */
  g_hash_table_insert(zmap->viewwindow_2_parent, view_window, view_container) ;

  return view_window;
}


/* Not a great name as it may not split and orientation may be ignored..... */
void zmapControlSplitWindow(ZMap zmap, GtkOrientation orientation, ZMapControlSplitOrder window_order)
{
  GtkWidget *curr_container ;
  ZMapViewWindow view_window ;
  ZMapView zmap_view ;
  ZMapWindow zmap_window ;
  char *view_title ;

  zMapReturnIfFail(zmap) ; 

  /* If there is a focus window then that will be the one we split and we need to find out
   * the container parent of that canvas. */
  if (zmap->focus_viewwindow)
    curr_container = (GtkWidget*)g_hash_table_lookup(zmap->viewwindow_2_parent, zmap->focus_viewwindow) ;
  else
    curr_container = NULL ;

  /* If we are adding a new view then there won't yet be a window for it, if we are splitting
   * an existing view then if it has a window we need to split that, otherwise we need to add
   * the first window to that view. */
  zmap_window = NULL ;

  if (zmap->focus_viewwindow)
    {
      zmap_view = zMapViewGetView(zmap->focus_viewwindow) ;
      zmap_window = zMapViewGetWindow(zmap->focus_viewwindow) ;
    }
  else
    {
      zmap_view = (ZMapView)(g_list_first(zmap->view_list)->data) ;
    }

  view_title  = zMapViewGetSequence(zmap_view) ;

  view_window = zmapControlNewWidgetAndWindowForView(zmap,
                                                     zmap_view,
                                                     zmap_window,
                                                     curr_container,
                                                     orientation,
						     window_order,
                                                     view_title);


  /* If there is no current focus window we need to make this new one the focus,
   * if we don't of code will fail because it relies on having a focus window and
   * the user will not get visual feedback that a window is the focus window. */
  if (!zmap->focus_viewwindow)
    zmapControlSetWindowFocus(zmap, view_window) ;

  /* We'll need to update the display..... */
  gtk_widget_show_all(zmap->toplevel) ;

  zmapControlWindowSetGUIState(zmap) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* CURRENTLY WE ARE NOT REPORTING WHEN THE USER SPLITS THE WINDOW, IT DOESN'T CREATE
   * A NEW VIEW SO LOGICALLY WE CAN'T SAY MUCH ABOUT IT, THIS MAY CHANGE THOUGH. */

  /* If there's a remote peer we need to tell them a window's been created.... */
  if (zmap->remote_control)
    zmapControlSendViewCreated(zmap, zMapViewGetView(view_window), zMapViewGetWindow(view_window)) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return ;
}



/* Utility function to return number of views left. */
int zmapControlNumViews(ZMap zmap)
{
  int num_views = 0 ;

  zMapReturnValIfFail(zmap, num_views) ; 

  if (zmap->view_list)
    num_views = g_list_length(zmap->view_list) ;

  return num_views ;
}



ZMapViewWindow zmapControlFindViewWindow(ZMap zmap, ZMapView view)
{
  ZMapViewWindow view_window = NULL ;
  FindViewWindowStruct find_viewwindow = {NULL} ;

  zMapReturnValIfFail(zmap, view_window) ; 

  /* Find the view_window for the view... */
  find_viewwindow.view = view ;
  g_hash_table_foreach(zmap->viewwindow_2_parent, findViewWindowCB, &find_viewwindow) ;
  view_window = find_viewwindow.view_window ;

  return view_window ;
}



/* Make this internal to this file.....not called from elsewhere.... */
/*
 * view_window is the view to add, this must be supplied.
 * orientation is GTK_ORIENTATION_HORIZONTAL or GTK_ORIENTATION_VERTICAL
 *
 *  */
GtkWidget *zmapControlAddWindow(ZMap zmap, GtkWidget *curr_frame,
				GtkOrientation orientation, ZMapControlSplitOrder window_order,
				char *view_title)
{
  GtkWidget *new_frame = NULL ;

  zMapReturnValIfFail(zmap, new_frame) ; 

  /* Supplying NULL will remove the title if its too big. */
  new_frame = gtk_frame_new(view_title) ;

  /* If there is a parent then add this pane as a child of parent, otherwise it means
   * this is the first pane and it it just gets added to the zmap vbox. */
  if (curr_frame)
    {
      /* Here we want to split the existing pane etc....... */
      splitPane(curr_frame, new_frame, orientation, window_order) ;
    }
  else
    {
      gtk_box_pack_start(GTK_BOX(zmap->pane_vbox), new_frame, TRUE, TRUE, 0) ;
    }

  return new_frame ;
}



/* You need to remember that there may be more than one view in a zmap. This means that
 * while a particular view may have lost all its windows and need closing, there might
 * be other views that have windows that can be focussed on. */
void zmapControlRemoveWindow(ZMap zmap, ZMapViewWindow view_window, GList **destroyed_views_inout)
{
  GtkWidget *close_container ;
  ZMapViewWindow remaining_view ;
  ZMapView view = NULL ;
  int num_windows ;

  num_windows = zMapViewNumWindows(view_window) ;

  view = zMapViewGetView(view_window) ;

  close_container = (GtkWidget*)g_hash_table_lookup(zmap->viewwindow_2_parent, view_window) ;


  /* Make sure we reset focus because we are removing the view it points to ! */
  if (view_window == zmap->focus_viewwindow)
    zmap->focus_viewwindow = NULL ;


  if (num_windows > 1)
    {
      /* Record view and window deleted if required. */
      if (destroyed_views_inout)
	{
	  GList *destroyed_views = NULL ;

	  destroyed_views = g_list_append(destroyed_views, view) ;

	  *destroyed_views_inout = destroyed_views ;
	}

      zMapViewRemoveWindow(view_window) ;
    }
  else
    {
      zMapDeleteView(zmap, view, destroyed_views_inout) ;
    }

  /* this needs to remove the pane.....AND  set a new focuspane....if there is one.... */
  remaining_view = closeWindow(zmap, close_container) ;

  /* Remove from hash of viewwindows to frames */
  g_hash_table_remove(zmap->viewwindow_2_parent, view_window) ;

  /* Having removed one window we need to refocus on another, if there is one....... */
  if (remaining_view)
    {
      zmapControlSetWindowFocus(zmap, remaining_view) ;
      zMapWindowSiblingWasRemoved(zMapViewGetWindow(remaining_view));
    }

  /* We'll need to update the display..... */
  gtk_widget_show_all(zmap->toplevel) ;

  zmapControlWindowSetGUIState(zmap) ;

  return ;
}





/* 
 *                      Internal routines
 */


/* Returns the viewWindow left in the other half of the pane, but note when its
 * last window, there is no viewWindow left over. */
static ZMapViewWindow closeWindow(ZMap zmap, GtkWidget *close_container)
{
  ZMapViewWindow remaining_view  = NULL ;
  GtkWidget *pane_parent ;

  zMapReturnValIfFail(zmap, remaining_view) ; 

  /* If parent is a pane then we need to remove that pane, otherwise we simply destroy the
   * container. */
  pane_parent = gtk_widget_get_parent(close_container) ;
  if (GTK_IS_PANED(pane_parent))
    {
      GtkWidget *keep_container ;

      keep_container = closePane(close_container) ;

      /* Set the focus to the window left over. */
      remaining_view = widget2ViewWindow(zmap->viewwindow_2_parent, keep_container) ;
    }
  else
    {
      gtk_widget_destroy(close_container) ;
      remaining_view = NULL ;
    }

  return remaining_view ;
}

static ZMapViewWindow widget2ViewWindow(GHashTable* hash_table, GtkWidget *widget)
{
  ZMapControlWidget2ViewwindowStruct widg2view ;

  widg2view.widget = widget ;
  widg2view.view_window = NULL ;

  g_hash_table_foreach(hash_table, findViewWindow, &widg2view) ;

  return widg2view.view_window ;
}


/* Test for value == user_data, i.e. have we found the widget given by user_data ? */
static void findViewWindow(gpointer key, gpointer value, gpointer user_data)
{
  ZMapControlWidget2ViewwindowStruct *widg2View = NULL ; 
  zMapReturnIfFail(user_data) ; 
  widg2View = (ZMapControlWidget2ViewwindowStruct *)user_data ;

  if (value == widg2View->widget)
    {
      widg2View->view_window = (ZMapViewWindow)key ;
    }

  return ;
}



/* Split a pane, actually we add a new pane into the selected window of the parent pane. */
static void splitPane(GtkWidget *curr_frame, GtkWidget *new_frame,
		      GtkOrientation orientation, ZMapControlSplitOrder window_order)
{
  GtkWidget *pane_parent, *new_pane ;
  ZMapPaneChild curr_child = ZMAP_PANE_NONE ;

  /* Get current frames parent, if window is unsplit it will be a container, otherwise its a pane
   * and we need to know which child we are of the pane. */
  pane_parent = gtk_widget_get_parent(curr_frame) ;
  if (GTK_IS_PANED(pane_parent))
    {
      /* Which child are we of the parent pane ? */
      curr_child = whichChildOfPane(curr_frame) ;
    }

  /* Remove the current frame from its container so we can insert a new container as the child
   * of that container, we have to increase its reference counter to stop it being.
   * destroyed once its removed from its container. */
  curr_frame = gtk_widget_ref(curr_frame) ;
  gtk_container_remove(GTK_CONTAINER(pane_parent), curr_frame) ;


  /* Create the new pane, note that horizontal split => splitting the pane across the middle,
   * vertical split => splitting the pane down the middle. */
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    new_pane = gtk_vpaned_new() ;
  else
    new_pane = gtk_hpaned_new() ;

  /* Now insert the new pane into the vbox or pane parent. */
  if (!GTK_IS_PANED(pane_parent))
    {
      gtk_container_add(GTK_CONTAINER(pane_parent), new_pane) ;
    }
  else
    {
      if (curr_child == ZMAP_PANE_CHILD_1)
	gtk_paned_pack1(GTK_PANED(pane_parent), new_pane, TRUE, TRUE) ;
      else
	gtk_paned_pack2(GTK_PANED(pane_parent), new_pane, TRUE, TRUE) ;
    }


  /* Add the frame views to the new pane. */
  if (window_order == ZMAPCONTROL_SPLIT_FIRST)
    {
      gtk_paned_pack1(GTK_PANED(new_pane), curr_frame, TRUE, TRUE) ;
      gtk_paned_pack2(GTK_PANED(new_pane), new_frame, TRUE, TRUE) ;
    }
  else
    {
      gtk_paned_pack1(GTK_PANED(new_pane), new_frame, TRUE, TRUE) ;
      gtk_paned_pack2(GTK_PANED(new_pane), curr_frame, TRUE, TRUE) ;
    }


  /* Now dereference the original frame as its now back in a container. */
  gtk_widget_unref(curr_frame) ;

  return ;
}



/* Close the container in one half of a pane, reparent the container in the other half into
 * the that panes parent, get rid of the original pane.
 * Returns a new/current direct container of a view. */
static GtkWidget *closePane(GtkWidget *close_frame)
{
  GtkWidget *keep_container, *keep_frame = NULL ;
  GtkWidget *parent_pane, *parents_parent ;
  ZMapPaneChild close_child, parent_parent_child ;

  parent_pane = gtk_widget_get_parent(close_frame) ;
  /* if (!GTK_IS_PANED(parent_pane)) 
    return keep_frame ;*/
  zMapReturnValIfFail(GTK_IS_PANED(parent_pane), keep_frame) ;  

  /* Find out which child of the pane the close_frame is and hence record the container
   * (n.b. might be another frame or might be a pane containing more panes/frames)
   * frame we want to keep. */
  close_child = whichChildOfPane(close_frame) ;
  if (close_child == ZMAP_PANE_CHILD_1)
    keep_container = myGetChild(parent_pane, 2) ;
  else
    keep_container = myGetChild(parent_pane, 1) ;


  /* Remove the keep_container from its container, we will insert it into the place where
   * its parent was originally. */
  keep_container = gtk_widget_ref(keep_container) ;
  gtk_container_remove(GTK_CONTAINER(parent_pane), keep_container) ;


  /* Find out what the parent of the parent_pane is, if its not a pane then we simply insert
   * keep_container into it, if it is a pane, we need to know which child of it our parent_pane
   * is so we insert the keep_container in the correct place. */
  parents_parent = gtk_widget_get_parent(parent_pane) ;
  if (GTK_IS_PANED(parents_parent))
    {
      parent_parent_child = whichChildOfPane(parent_pane) ;
    }

  /* Destroy the parent_pane, this will also destroy the close_frame as it is still a child. */
  gtk_widget_destroy(parent_pane) ;

  /* Put the keep_container into the parent_parent. */
  if (!GTK_IS_PANED(parents_parent))
    {
      gtk_container_add(GTK_CONTAINER(parents_parent), keep_container) ;
    }
  else
    {
      if (parent_parent_child == ZMAP_PANE_CHILD_1)
	gtk_paned_pack1(GTK_PANED(parents_parent), keep_container, TRUE, TRUE) ;
      else
	gtk_paned_pack2(GTK_PANED(parents_parent), keep_container, TRUE, TRUE) ;
    }

  /* Now dereference the keep_container as its now back in a container. */
  gtk_widget_unref(keep_container) ;

  /* The keep_container may be a frame _but_ it may also be a pane and its children may be
   * panes so we have to go down until we find a child that is a frame to return as the
   * new current frame. (Note that we arbitrarily go down the child1 children until we find
   * a frame.)
   * Note also that we have to deal with event boxes inserted for new views,
   * it makes the hierachy more complex, in another world I'll think of a better way
   * to handle all this.
   */
  while (GTK_IS_PANED(keep_container) || GTK_IS_EVENT_BOX(keep_container))
    {
      if (GTK_IS_PANED(keep_container))
	{
	  keep_container = myGetChild(keep_container, 1) ;
	}
      else
	{
	  GList *children ;

	  children = gtk_container_get_children(GTK_CONTAINER(keep_container)) ;
	  keep_container = (GtkWidget *)(children->data) ;

	  g_list_free(children) ;
	}
    }
  keep_frame = keep_container ;

  return keep_frame ;
}


/* Makes sure that everything that needs to happen does happen as a result
 * of a view window becoming the keyboard focus. */
void zmapControlSetWindowFocus(ZMap zmap, ZMapViewWindow new_viewwindow)
{
  GtkWidget *viewwindow_frame ;
  ZMapView view ;
  ZMapWindow window ;
  double top, bottom ;

  zMapReturnIfFail(zmap) ; 

  if (!new_viewwindow) 
    return ;

  if (new_viewwindow != zmap->focus_viewwindow)
    {
      GtkWidget *label ;
      char *label_txt ;
      ZMapWindowNavigator navigator ;

      /* Unfocus the old window. */
      if (zmap->focus_viewwindow)
	{
	  GtkWidget *unfocus_frame ;

	  unfocus_frame = (GtkWidget*)g_hash_table_lookup(zmap->viewwindow_2_parent, zmap->focus_viewwindow) ;
          gtk_frame_set_shadow_type(GTK_FRAME(unfocus_frame), GTK_SHADOW_OUT) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  /* This does not work currently because we not have a event box as a parent of the frame... */
          gtk_widget_set_name(GTK_WIDGET(unfocus_frame), "GtkFrame");
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	  /* Swop the frames label text colour back to "inactive" */
	  label = gtk_frame_get_label_widget(GTK_FRAME(unfocus_frame)) ;
	  label_txt = (char *)gtk_label_get_text(GTK_LABEL(label)) ;
	  label_txt = g_strdup_printf("<span foreground=\"black\">%s</span>", label_txt) ;
	  gtk_label_set_markup(GTK_LABEL(label), label_txt) ;
	  g_free(label_txt) ;
	}

      /* focus the new one. */
      zmap->focus_viewwindow = new_viewwindow ;
      view = zMapViewGetView(new_viewwindow) ;
      window = zMapViewGetWindow(new_viewwindow) ;

      viewwindow_frame = (GtkWidget*)g_hash_table_lookup(zmap->viewwindow_2_parent, zmap->focus_viewwindow) ;
      gtk_frame_set_shadow_type(GTK_FRAME(viewwindow_frame), GTK_SHADOW_IN);

      /* Swop the frames label text colour to "active", gosh gtk makes this hard work... */
      label = gtk_frame_get_label_widget(GTK_FRAME(viewwindow_frame)) ;
      label_txt = (char *)gtk_label_get_text(GTK_LABEL(label)) ;
      label_txt = g_strdup_printf("<span foreground=\"red\">%s</span>", label_txt) ;
      gtk_label_set_markup(GTK_LABEL(label), label_txt) ;
      g_free(label_txt) ;

      /* make sure zoom buttons etc. appropriately sensitised for this window. */
      zmapControlWindowSetGUIState(zmap) ;

      /* set up navigator with new focus window. */
      navigator = zMapViewGetNavigator(view) ;
      zMapWindowNavigatorFocus(navigator, TRUE);
      zMapWindowNavigatorSetCurrentWindow(navigator, window);

      zMapWindowGetVisible(window, &top, &bottom) ;
      zmapNavigatorSetView(zmap->navigator, zMapViewGetFeatures(view), top, bottom) ;
    }

  return ;
}



/* Returns which child of a pane the given widget is, the child widget _must_ be in the pane
 * otherwise the function will abort. */
static ZMapPaneChild whichChildOfPane(GtkWidget *child)
{
  ZMapPaneChild pane_child = ZMAP_PANE_NONE ;
  GtkWidget *pane_parent ;

  zMapReturnValIfFail(child, pane_child ) ;

  pane_parent = gtk_widget_get_parent(child) ;
  if (!GTK_IS_PANED(pane_parent)) 
    return pane_child ;

  if (myGetChild(pane_parent, 1) == child)
    pane_child = ZMAP_PANE_CHILD_1 ;
  else if (myGetChild(pane_parent, 2) == child)
    pane_child = ZMAP_PANE_CHILD_2 ;

  return pane_child ;
}


/* Called when a destroy signal has been sent to the labels widget.
 * Need to remove its reference in the labels struct.
 *  */
static void labelDestroyCB(GtkWidget *widget, gpointer cb_data)
{
  ZMapInfoPanelLabels labels = NULL ; 
  zMapReturnIfFail(cb_data) ; 
  labels = (ZMapInfoPanelLabels) cb_data ;

  labels->hbox = NULL ;

  return ;
}


static void findViewWindowCB(gpointer key, gpointer value, gpointer user_data)
{
  ZMapViewWindow view_window = (ZMapViewWindow)key ;
  FindViewWindow find_viewwindow = NULL ; 
  zMapReturnIfFail(user_data) ; 
  find_viewwindow = (FindViewWindow)user_data ;

  if (zMapViewGetView(view_window) == find_viewwindow->view)
    find_viewwindow->view_window = view_window ;

  return ;
}


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* I'D LIKE TO USE A STYLE TO SET THE FRAME COLOURS AUTOMATICALLY WHEN THE CANVAS IS SELECTED
 * BUT WE NEED AN EVENT BOX AS A PARENT TO THE FRAME....WE USED TO HAVE THIS BUT FOR RECEIVING
 * XREMOTE COMMANDS _AND_ FOR THIS STYLE STUFF WHICH I DIDN'T REALISE SO I MOVED THE EVENT
 * BOX TO FIX A BUG IN THE SPLITTING/XREMOTE STUFF. I'LL GET ROUND TO THE EVENT BOX STUFF
 * IF IT BECOMES NEEDED. CURRENTLY WE AT LEAST HAVE THE TITLE CHANGING COLOUR. */
#define FRAME_WIDG_STYLE "ZMap_View_Frame_Style"
#define FRAME_WIDG_NAME  "ZMap_View_Frame"

static void *addFrameRCconfig(GtkWidget *widg)
{
  static char *rc_text =
    "style \"" FRAME_WIDG_STYLE "\"\n"
    "{\n"
    "text[NORMAL] = \"red\"\n"
    "text[ACTIVE] = \"red\"\n"
    "fg[NORMAL] = \"red\"\n"
    "bg[NORMAL] = \"red\"\n"
    "fg[ACTIVE] = \"red\"\n"
    "bg[ACTIVE] = \"red\"\n"
    "fg[SELECTED] = \"red\"\n"
    "bg[SELECTED] = \"red\"\n"
    "xthickness = 100\n"
    "}\n"
    "\n"
    "widget \"" FRAME_WIDG_NAME "\" style \"" FRAME_WIDG_STYLE "\"\n" ;

  gtk_rc_parse_string(rc_text) ;

  gtk_widget_set_name(widg, FRAME_WIDG_NAME) ;

  return ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


