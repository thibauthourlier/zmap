/*  File: zmapRemoteControl.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2010: Genome Research Ltd.
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
 * originally written by:
 *
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *      Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description: Implements the "Session" layer (OSI 7 layer model)
 *              of the Remote Control interface: the code sends
 *              a request and then waits for the reply. The code
 *              knows nothing about the content of the messages,
 *              it simply ensures that each request receives a
 *              reply or an error is reported (e.g. timeout).
 *
 * Exported functions: See ZMap/zmapRemoteControl.h
 *              
 * HISTORY:
 * Last edited: Nov 26 17:39 2010 (edgrif)
 * Created: Fri Sep 24 14:49:23 2010 (edgrif)
 * CVS info:   $Id$
 *-------------------------------------------------------------------
 */

#include <string.h>
#include <gdk/gdkx.h>					    /* For X Windows stuff. */

#include <ZMap/zmapUtils.h>
#include <zmapRemoteControl_.h>


/* Text for debug/log messages.... */
#define PEER_PREFIX "PEER"
#define CLIENT_PREFIX "CLIENT"
#define SERVER_PREFIX "SERVER"

#define ENTER_TXT ">>> Enter  ------------------------------------------"
#define EXIT_TXT  "<<< Exit   ------------------------------------------"


/* Text to go into ClientMessage events...allows us to check the event is ours...
 * 
 * NOTE that max length of text is 19 chars !!!!!! */
#define MAX_BYTES        "-------------------"
#define REQUEST_RECEIVED "Request Received"
#define CALL_SERVER      "Call Server Func"




/* Use this macro for _ALL_ logging calls in this code to ensure that they go to the right place. */
#define REMOTELOGMSG(REMOTE_CONTROL, FORMAT_STR, ...)			\
  do									\
    {									\
      if (remote_debug_G)						\
        {								\
	  debugMsg((REMOTE_CONTROL),					\
		   __PRETTY_FUNCTION__,					\
		   (FORMAT_STR), __VA_ARGS__) ;				\
	}								\
    } while (0)



/* For printing out events. */
typedef struct eventTxtStructName
{
  char* text ;
  GdkEventMask mask ;
} eventTxtStruct, *eventTxt ;



static gboolean testNotifyCB(GtkWidget *widget, GdkEventProperty *property_event, gpointer user_data) ;


static gboolean serverAppReplyCB(void *remote_data, void *reply, int reply_len) ;

static gboolean serverSelectionClearCB(GtkWidget *widget, GdkEventSelection *event, gpointer user_data) ;
static gboolean clientSelectionRequestCB(GtkWidget *widget, GdkEventSelection *event, gpointer user_data) ;
static gboolean serverSelectionNotifyCB(GtkWidget *widget, GdkEventSelection *event, gpointer user_data) ;
static gboolean clientClientMessageCB(GtkWidget *widget, GdkEventClient *client_event, gpointer user_data) ;

static gboolean serverClientMessageCB(GtkWidget *widget, GdkEventClient *client_event, gpointer user_data) ;

static gboolean clientSelectionNotifyCB(GtkWidget *widget, GdkEventSelection *event, gpointer user_data) ;
static gboolean serverPropertyNotifyCB(GtkWidget *widget, GdkEventProperty *property_event, gpointer user_data) ;

static gboolean windowGeneralEventCB(GtkWidget *wigdet, GdkEvent *event, gpointer user_data) ;
static gboolean timeOutCB(gpointer user_data) ;

static void debugMsg(ZMapRemoteControl remote_control, const char *func_name, char *format_str, ...) ;
static gboolean stderrOutputCB(gpointer user_data, char *err_msg) ;

static gboolean resetToReady(ZMapRemoteControl remote_control) ;

static AnyRequest createRemoteRequest(RemoteType request_type,
				      GdkAtom peer_atom, gulong curr_req_time, char *request) ;
static void destroyRemoteRequest(AnyRequest remote_request) ;
static char *getStateAsString(ZMapRemoteControl remote_control) ;

static void disconnectSignalHandler(GtkWidget *widget, gulong *func_id) ;
  static void disconnectTimerHandler(guint *func_id) ;


/* 
 *        Globals.
 */

/* Used for validation of ZMapRemoteControlStruct. */
ZMAP_MAGIC_NEW(remote_control_magic_G, ZMapRemoteControlStruct) ;


/* Debugging stuff... */
static gboolean remote_debug_G = TRUE ;

/* If zero then all events are reported otherwise any events masked will not be reported. */
static GdkEventMask msg_exclude_mask_G = (GDK_POINTER_MOTION_MASK | GDK_EXPOSURE_MASK
					  | GDK_FOCUS_CHANGE_MASK | GDK_VISIBILITY_NOTIFY_MASK
					  | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK) ;



/* 
 *                     External interface
 */


/* Creates a remote control object.
 *
 * remote_control_unique_str 
 *            Must be X window system unique, it is up to the caller to ensure this is so.
 */ 
ZMapRemoteControl zMapRemoteControlCreate(char *app_id, char *remote_control_unique_str,
					  ZMapRemoteControlRequestHandlerFunc request_func, gpointer request_data,
					  ZMapRemoteControlReplyHandlerFunc reply_func, gpointer reply_data,
					  ZMapRemoteControlTimeoutHandlerFunc timeout_func, gpointer timeout_data)
{
  ZMapRemoteControl remote_control = NULL ;

  zMapAssert((remote_control_unique_str && *remote_control_unique_str)) ;
  zMapAssert(reply_func && timeout_func) ;


  remote_control = g_new0(ZMapRemoteControlStruct, 1) ;
  remote_control->magic = remote_control_magic_G ;

  /* Set app_id and default error reporting, now we can output error messages. */
  remote_control->app_id = g_strdup(app_id) ;
  remote_control->err_func = stderrOutputCB ;
  remote_control->err_data = NULL ;

  /* can't be earlier as we need app_id and err_XX stuff. */
  REMOTELOGMSG(remote_control, "%s", ENTER_TXT) ;

  remote_control->our_atom = gdk_atom_intern(remote_control_unique_str, FALSE) ;
  remote_control->our_atom_string = gdk_atom_name(remote_control->our_atom) ;


  /* boolean should be TRUE here to check data format supported. */
  remote_control->data_format_atom = gdk_atom_intern("XA_STRING", FALSE) ;

  remote_control->show_all_events = TRUE ;

  remote_control->request_func = request_func ;
  remote_control->request_data = request_data ;
  remote_control->reply_func = reply_func ;
  remote_control->reply_data = reply_data ;
  remote_control->timeout_func = timeout_func ;
  remote_control->timeout_data = timeout_data ;

  REMOTELOGMSG(remote_control, "%s", "RemoteControl object created.") ;

  REMOTELOGMSG(remote_control, "%s", EXIT_TXT) ;

  return  remote_control ;
}


/* Set debug on/off. */
gboolean zMapRemoteControlSetDebug(ZMapRemoteControl remote_control, gboolean debug_on)
{
  gboolean result = TRUE ;

  ZMAP_MAGIC_ASSERT(remote_control_magic_G, remote_control->magic) ;

  remote_debug_G = debug_on ;

  return result ;
 }



/* Set timeout, if timeout_secs == 0 then no timeout. */
gboolean zMapRemoteControlSetTimeout(ZMapRemoteControl remote_control, int timeout_secs)
{
  gboolean result = TRUE ;

  ZMAP_MAGIC_ASSERT(remote_control_magic_G, remote_control->magic) ;

  if (timeout_secs <= 0)
    remote_control->timeout_ms = 0 ;
  else
    remote_control->timeout_ms = timeout_secs * 1000 ;

  return result ;
 }



/* Set a function to handle error messages, by default errors are sent to stderr
 * but caller can specify their own routine.
 */
gboolean zMapRemoteControlSetErrorCB(ZMapRemoteControl remote_control,
				     ZMapRemoteControlErrorFunc err_func, gpointer err_data) 
{
  gboolean result = FALSE ;

  ZMAP_MAGIC_ASSERT(remote_control_magic_G, remote_control->magic) ;
  zMapAssert(err_func) ;

  remote_control->err_func = err_func ;
  remote_control->err_data = err_data ;

  return result ;
 }




/* Initialises a remote control object which can be used to communicate with a number of peer
 * clients. At the end of this routine the object will be ready to receive requests from
 * other clients.
 * 
 * remote_control_widget
 *            Must be mapped (i.e. have an X window) otherwise the selection
 *            setting call will fail which breaks the whole protocol.
 * 
 *  */
gboolean zMapRemoteControlInit(ZMapRemoteControl remote_control, GtkWidget *remote_control_widget)
{
  gboolean result = TRUE ;

  ZMAP_MAGIC_ASSERT(remote_control_magic_G, remote_control->magic) ;

  REMOTELOGMSG(remote_control, "%s", ENTER_TXT) ;

  if (result)
    {
      if (!(result = GTK_IS_WIDGET(remote_control_widget)))
	REMOTELOGMSG(remote_control, "",
		     "RemoteControl initialisation failed, bad widget %p.", remote_control_widget) ;
    }

  if (result)
    {
      /* temporary check until we move gtk versions... */
#if GTK_MINOR_VERSION > 20
      if (!(result = gtk_widget_get_mapped(remote_control_widget)))
#else
	if (!(result = GTK_WIDGET_MAPPED(remote_control_widget)))
#endif
	  REMOTELOGMSG(remote_control, "",
		       "RemoteControl initialisation failed, widget %p not mapped.", remote_control_widget) ;
	else
	  {
	    remote_control->our_widget = remote_control_widget ;
	    remote_control->our_window = GDK_WINDOW_XWINDOW(gtk_widget_get_window(remote_control->our_widget)) ;


	    if (remote_control->show_all_events)
	      remote_control->all_events_id = g_signal_connect(remote_control->our_widget, "event",
							       (GCallback)windowGeneralEventCB, remote_control) ;
	  }
    }

  if (result)
    {
      /* Take ownership of our request atom so we are ready to receive requests. */
      if (!(result = resetToReady(remote_control)))
	{
	  REMOTELOGMSG(remote_control, "",
		       "RemoteControl initialisation failed,"
		       " cannot become selection owner for atom \"%s\""
		       " on window of widget %p.",
		       remote_control->our_atom_string,
		       remote_control->our_widget) ;
	}
      else
	{
	  REMOTELOGMSG(remote_control, "",
		       "RemoteControl object initialised:"
		       "\tgtkwidget: %p,\tgdkwindow: %p\tXwindow: %u\tAtom: %s",
		       remote_control->our_widget,
		       gtk_widget_get_window(remote_control->our_widget),
		       remote_control->our_window,
		       remote_control->our_atom_string) ;
	}
    }

  REMOTELOGMSG(remote_control, "%s", EXIT_TXT) ;

  return result ;
}



/* Make a request to a peer, note that caller _must_ supply the peer_unique_str because each request
 * may be made to a different peer with each peer having its own unique string. */
gboolean zMapRemoteControlRequest(ZMapRemoteControl remote_control,
				  char *peer_unique_str, char *request, guint32 time)
{
  gboolean result = TRUE ;

  ZMAP_MAGIC_ASSERT(remote_control_magic_G, remote_control->magic) ;

  REMOTELOGMSG(remote_control, "%s", ENTER_TXT) ;

  if (remote_control->curr_request)
    {
      /* we are already processing a command so log/error message
	 and do nothing, could happen through normal user actions,
	 e.g. clicking lots of things quickly. */
      REMOTELOGMSG(remote_control,
		   "Already servicing request for %s, current state %s, command was: %s.",
		   peer_unique_str, getStateAsString(remote_control), request) ;

      result = FALSE ;
    }
  else
    {
      /* Disconnect our own selection-clear handler so we can't be called with a request while
       * issuing one. */
      disconnectSignalHandler(remote_control->our_widget, &(remote_control->select_clear_id)) ;

      /* If caller did not specify a time then use CurrentTime, not ideal but the
       * best we can do really. */
      if (!time)
	time = GDK_CURRENT_TIME ;


      /* If we no longer handle the selection clear this is redundant.... */
      if (result)
	{
	  /* First relinquish ownership of our own atom so we ignore other
	     peer requests until we have finished this one. */
	  if (!(result = gtk_selection_owner_set(NULL,
						 remote_control->our_atom,
						 time)))
	    REMOTELOGMSG(remote_control,
			 "Call to lose ownership of atom %s failed so cannot process command for \"%s\","
			 " command was: %s.",
			 remote_control->our_atom_string,
			 peer_unique_str, request) ;
	}




      if (result)
	{
	  GdkAtom peer_atom ;

	  peer_atom = gdk_atom_intern(peer_unique_str, FALSE) ;

	  REMOTELOGMSG(remote_control,
		       "About to get ownership of atom %s.",
		       peer_unique_str) ;

	  /* Taking ownership signals that we want to make a request.
	   * To avoid race conditions the time MUST be the time of the
	   * action/event that originally triggered this action and
	   * _not_ the current X Server time. */
	  if ((result = gtk_selection_owner_set(remote_control->our_widget,
						peer_atom,
						time)))
	    {
	      ClientRequest client_request ;

	      REMOTELOGMSG(remote_control,
			   "Got ownership of atom %s to signal we want to send a request.",
			   peer_unique_str) ;

	      /* Create the request object that will persist until the request is complete or
	       * an error has occurred. */
	      remote_control->curr_request = createRemoteRequest(REMOTE_TYPE_CLIENT, peer_atom, time, request) ;
	      client_request = (ClientRequest)remote_control->curr_request ;

	      client_request->state = CLIENT_STATE_SENDING ;

	      remote_control->select_request_id = g_signal_connect(remote_control->our_widget,
								   "selection-request-event",
								   (GCallback)clientSelectionRequestCB, remote_control) ;

	      if (remote_control->timeout_ms)
		remote_control->timer_source_id = g_timeout_add(remote_control->timeout_ms, 
								timeOutCB, remote_control) ;

	      REMOTELOGMSG(remote_control, "Set selection-received handler on %u, waiting for convert",
			   GDK_WINDOW_XWINDOW(gtk_widget_get_window(remote_control->our_widget))) ;

	      REMOTELOGMSG(remote_control, "%s", "registered request, waiting for selection-request event.") ;
	    }
	  else
	    {
	      disconnectSignalHandler(remote_control->our_widget, &(remote_control->select_request_id)) ;
	      disconnectTimerHandler(&(remote_control->timer_source_id)) ;
    
	      REMOTELOGMSG(remote_control,
			   "Cannot gain ownership of atom %s so cannot process command for \"%s\","
			   " command was: %s.",
			   remote_control->our_atom_string,
			   peer_unique_str, request) ;
	    }
	}
    }


  /* If we failed at any stage we need to reset so we are ready for new requests. */
  if (!result)
    resetToReady(remote_control) ;


  REMOTELOGMSG(remote_control, "%s", EXIT_TXT) ;

  return result ;
}



/* Free all resources and destroy the remote control block. */
void zMapRemoteControlDestroy(ZMapRemoteControl remote_control)
{
  ZMAP_MAGIC_ASSERT(remote_control_magic_G, remote_control->magic) ;

  REMOTELOGMSG(remote_control, "%s", ENTER_TXT) ;

  /* Need to remove callbacks....??? Would need widget to still be functional... */
  if (GTK_IS_WIDGET(remote_control->our_widget))
    {
      disconnectSignalHandler(remote_control->our_widget, &(remote_control->all_events_id)) ;
      disconnectTimerHandler(&(remote_control->timer_source_id)) ;
      disconnectSignalHandler(remote_control->our_widget, &(remote_control->select_clear_id)) ;
      disconnectSignalHandler(remote_control->our_widget, &(remote_control->select_notify_id)) ;
      disconnectSignalHandler(remote_control->our_widget, &(remote_control->select_request_id)) ;
      disconnectSignalHandler(remote_control->our_widget, &(remote_control->client_message_id)) ;
    }

  g_free(remote_control->our_atom_string) ;

  if (remote_control->curr_request)
    destroyRemoteRequest(remote_control->curr_request) ;


  REMOTELOGMSG(remote_control, "%s", "Destroyed RemoteControl object.") ;


  REMOTELOGMSG(remote_control, "%s", EXIT_TXT) ;	    /* Latest we can make this call as we 
							       need remote_control. */

  ZMAP_MAGIC_RESET(remote_control->magic) ;		    /* Before free() reset magic to invalidate memory. */
  g_free(remote_control) ;

  return ;
}





/* 
 *                    Internal routines.
 */



/* Auto generated Enum -> String functions, converts the enums _directly_ to strings.
 *
 * Function signature is of the form:
 *
 *  const char *remoteState2ExactStr(RemoteControlState state) ;
 *
 *  */
ZMAP_ENUM_AS_EXACT_STRING_FUNC(remoteType2ExactStr, RemoteType, REMOTE_TYPE_LIST) ;

ZMAP_ENUM_AS_EXACT_STRING_FUNC(clientState2ExactStr, ClientControlState, CLIENT_STATE_LIST) ;

ZMAP_ENUM_AS_EXACT_STRING_FUNC(serverState2ExactStr, ServerControlState, SERVER_STATE_LIST) ;




/* Reset object to be ready to receive commands. */
static gboolean resetToReady(ZMapRemoteControl remote_control)
{
  gboolean result = FALSE ;

  if (!(result = gdk_selection_owner_set(remote_control->our_widget->window, remote_control->our_atom,
					 GDK_CURRENT_TIME,  /* Not sure about this, pass time in ? */
					 FALSE)))
    {
      REMOTELOGMSG(remote_control, "",
		   "Call failed to become selection owner for atom \"%s\""
		   " on window of widget %p.",
		   remote_control->our_atom_string, remote_control->our_widget) ;
    }
  else
    {
      if (remote_control->curr_request)
	destroyRemoteRequest(remote_control->curr_request) ;

      /* Attach event handler so we know when someone makes a request. */
      remote_control->select_clear_id = g_signal_connect(remote_control->our_widget, "selection-clear-event",
							 (GCallback)serverSelectionClearCB, remote_control) ;

      REMOTELOGMSG(remote_control, "",
		   "Now selection owner for atom \"%s\""
		   " on window of widget %p.",
		   remote_control->our_atom_string, remote_control->our_widget) ;
    }

  return result ;
}



/* "selection-clear-event" handler, called when a peer signals that they have a request
 * by taking ownership of the atom.
 * 
 * Event struct:
 *
 * typedef struct {
 *   GdkEventType type;
 *   GdkWindow *window;
 *   gint8 send_event;
 *   GdkAtom selection;
 *   GdkAtom target;
 *   GdkAtom property;
 *   guint32 time;
 *   GdkNativeWindow requestor;    This seems to be zero when it should be the peer window !!
 * } GdkEventSelection;
 *
 *
 * Note that we don't use the gdk selection calls because they seem broken and indeed some
 * parts are actually documented as not working very well, e.g. gdk_selection_owner_get()
 * is fundamentally broken, it doesn't allow you to retrieve windows from other processes !
 * Hence we are using raw X Windows calls.
 *
 */
static gboolean serverSelectionClearCB(GtkWidget *widget, GdkEventSelection *event, gpointer user_data)
{
  gboolean result = FALSE ;
  ZMapRemoteControl remote_control = (ZMapRemoteControl)user_data ;
  ServerRequest server_request ;
  GdkWindow *our_window ;
  Display *x_display ;
  Window x_our_window, x_peer_window ;
  Atom x_property_atom, x_data_format_atom, x_data_receiving_atom ;

  REMOTELOGMSG(remote_control, "%s", ENTER_TXT) ;

  /* I don't think this should be able to happen because of how we disconnect the handler
   * when we make a request... */
  zMapAssert(!(remote_control->curr_request)) ;

  our_window = gtk_widget_get_window(remote_control->our_widget) ;
  x_display = GDK_WINDOW_XDISPLAY(our_window) ;
  x_our_window = GDK_WINDOW_XWINDOW(our_window) ;
  x_property_atom = gdk_x11_atom_to_xatom(remote_control->our_atom) ;


  /* WE SHOULD STORE THIS WINDOW ID in the request struct....!! */
  /* Get the current owner of the selection, this is the peer window that is signalling they
   * have a request. */
  x_peer_window = XGetSelectionOwner(x_display, x_property_atom) ;

  REMOTELOGMSG(remote_control,
	       "Client Peer X-window %u has signalled they have a request on \"%s\".",
	       x_peer_window,
	       remote_control->our_atom_string) ;

  /* Allocate server bit.... */
  remote_control->curr_request = createRemoteRequest(REMOTE_TYPE_SERVER, event->selection, event->time, NULL) ;

  server_request = (ServerRequest)(remote_control->curr_request) ;
  server_request->state = SERVER_STATE_RETRIEVING ;
  server_request->peer_window = x_peer_window ;

  /* Set up SelectionNotify handler which will be called when client replies to us. */
  remote_control->select_notify_id = g_signal_connect(remote_control->our_widget,
						      "selection-notify-event",
						      (GCallback)serverSelectionNotifyCB, remote_control) ;

  x_data_format_atom = gdk_x11_atom_to_xatom(remote_control->data_format_atom) ;
  x_data_receiving_atom = x_property_atom ;

  XConvertSelection(x_display,
		    x_property_atom, x_data_format_atom,
		    x_data_receiving_atom, x_our_window,
		    remote_control->curr_request->curr_req_time) ;

  REMOTELOGMSG(remote_control,
	       "Replied to Client peer %u with XConvertSelection() asking for their request on \"%s\".",
	       x_peer_window, remote_control->our_atom_string) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* Check selection owner.... */
  x_peer_window = XGetSelectionOwner(x_display, x_property_atom) ;

  REMOTELOGMSG(remote_control,
	       "Current selection owner is: %u.",
	       x_peer_window) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  REMOTELOGMSG(remote_control, "%s", EXIT_TXT) ;

  return result ;
}



/* "selection-request-event" handler, called as a result of an XConvertSelection() call
 * by the "server" peer asking for our request. We put the request into the atom on
 * the "server" window and signal that they can pick it up.
 * 
 * Here's the contents of the selection event:
 *
 * typedef struct {
 * GdkEventType type;
 * GdkWindow *window;
 * gint8 send_event;
 * GdkAtom selection;
 * GdkAtom target;
 * GdkAtom property;
 * guint32 time;
 * GdkNativeWindow requestor;
 * } GdkEventSelection;
 *
 */
static gboolean clientSelectionRequestCB(GtkWidget *widget, GdkEventSelection *select_event, gpointer user_data)
{
  gboolean result = FALSE ;
  ZMapRemoteControl remote_control = (ZMapRemoteControl)user_data ;
  ClientRequest client_request = (ClientRequest)remote_control->curr_request ;
  GdkWindow *our_window ;
  Display *x_display ;
  Window x_our_window ;
  Atom x_property_atom, x_data_format_atom ;
  Atom x_selection_atom ;
  char *x_selection_str ;
  XSelectionEvent x_selection_event ;


  REMOTELOGMSG(remote_control, "%s", ENTER_TXT) ;

  zMapAssert(client_request->state == CLIENT_STATE_SENDING) ;

  /* Disconnect to prevent stray events inteferring. */
  disconnectSignalHandler(remote_control->our_widget, &(remote_control->select_request_id)) ;

  /* Record the servers window, needed later so we can send a SelectionNotify event to it. */
  client_request->peer_window = select_event->requestor ;

  our_window = gtk_widget_get_window(remote_control->our_widget) ;
  x_display = GDK_WINDOW_XDISPLAY(our_window) ;
  x_our_window = GDK_WINDOW_XWINDOW(our_window) ;
  x_property_atom = gdk_x11_atom_to_xatom(remote_control->our_atom) ;
  x_data_format_atom = gdk_x11_atom_to_xatom(remote_control->data_format_atom) ;

  x_selection_atom = gdk_x11_atom_to_xatom(select_event->selection) ;
  x_selection_str = gdk_atom_name(select_event->selection) ;

  REMOTELOGMSG(remote_control,
	       "Server Peer X-window %u has signalled they want the request on \"%s\".",
	       client_request->peer_window, x_selection_str) ;

  /* Put the request into the property/window passed into us. */
  XChangeProperty(x_display,
		  client_request->peer_window,
		  x_selection_atom,
		  x_data_format_atom,
		  8,
		  PropModeReplace,
		  (unsigned char*)(remote_control->curr_request->request),
		  (strlen(remote_control->curr_request->request) + 1)) ;

  REMOTELOGMSG(remote_control,
	       "Put request on \"%s\" on X-window %u, request was \"%s\".",
	       x_selection_str,
	       client_request->peer_window,
	       remote_control->curr_request->request) ;

  
  /* Set ClientMessage handler which will be called by server once it has retrieved request. */
  remote_control->client_message_id = g_signal_connect(remote_control->our_widget,
						       "client-event",
						       (GCallback)clientClientMessageCB, remote_control) ;

  REMOTELOGMSG(remote_control,
	       "Attached client-event handler to our window %u.",
	       x_our_window) ;



  /* Attach a test propertyNotify handler.... */
  remote_control->test_notify_id = g_signal_connect(remote_control->our_widget,
						    "property-notify-event",
						    (GCallback)testNotifyCB, remote_control) ;

  REMOTELOGMSG(remote_control,
	       "Attached test propertynotify handler to our window %u.",
	       x_our_window) ;





  /* Send selection which will tell our peer how to retrieve our request.
   * 
   * Event struct is:
   *  typedef struct
   *  {
   *    int type;
   *    unsigned long serial ;
   *    Bool send_event;
   *    Display *display;
   *    Window requestor;
   *    Atom selection;
   *    Atom target;
   *    Atom property;
   *    Time time;
   *  } XSelectionEvent;
   * 
   */
  x_selection_event.type = SelectionNotify ;
  x_selection_event.serial = 0 ;			    /* I don't know how to set this field. */
  x_selection_event.send_event = TRUE ;
  x_selection_event.display = x_display ;
  x_selection_event.requestor = x_our_window ;
  x_selection_event.selection = x_property_atom ;	    /* Don't know what to set here.... */
  x_selection_event.target = x_property_atom ;
  x_selection_event.property = x_property_atom ;
  x_selection_event.time = select_event->time ;

  /* Tell the server that we've sent the request using a SelectionNotify event. */
  if (XSendEvent(x_display, client_request->peer_window, True, 0, (XEvent *)(&x_selection_event)) == 0)
    REMOTELOGMSG(remote_control, "XSendEvent() call to send SelectionNotify to peer %u failed.",
		 client_request->peer_window) ;
  else
    REMOTELOGMSG(remote_control, "XSendEvent() sent SelectionNotify to peer %u.",
		 client_request->peer_window) ;


  REMOTELOGMSG(remote_control, "%s", EXIT_TXT) ;

  return result ;
}



/* "selection-notify-event" handler.
 * 
 * 
 * 
 * 
 * 
 * Here's the contents of the selection event:
 *
 * typedef struct {
 * GdkEventType type;
 * GdkWindow *window;
 * gint8 send_event;
 * GdkAtom selection;
 * GdkAtom target;
 * GdkAtom property;
 * guint32 time;
 * GdkNativeWindow requestor;
 * } GdkEventSelection;
 * 
 * 
 * 
 * 
 */
static gboolean serverSelectionNotifyCB(GtkWidget *widget, GdkEventSelection *selection_event, gpointer user_data)
{
  gboolean result = FALSE ;
  gboolean status = TRUE ;
  ZMapRemoteControl remote_control = (ZMapRemoteControl)user_data ;
  ServerRequest server_request = (ServerRequest)remote_control->curr_request ;
  GdkWindow *our_window ;
  Window x_our_window ;
  Display *x_display ;
  Atom x_property_atom, x_data_format_atom ;
  Atom x_selection_atom ;
  long max_data = 20000 ;				    /* Do something sensible about length... */
  Atom actual_type_return = None ;
  int actual_format_return = 0 ;
  unsigned long nitems_return = 0, bytes_still_left = 0 ;
  unsigned char *prop_return = NULL ;


  REMOTELOGMSG(remote_control, "%s", ENTER_TXT) ;

  zMapAssert(server_request->state == SERVER_STATE_RETRIEVING) ;

  /* Disconnect to prevent stray events inteferring. */
  disconnectSignalHandler(remote_control->our_widget, &(remote_control->select_notify_id)) ;

  /* Get display and other stuff in a raw X Window form. */
  our_window = gtk_widget_get_window(remote_control->our_widget) ;
  x_display = GDK_WINDOW_XDISPLAY(our_window) ;
  x_our_window = GDK_WINDOW_XWINDOW(our_window) ;
  x_property_atom = gdk_x11_atom_to_xatom(remote_control->our_atom) ;
  x_data_format_atom = gdk_x11_atom_to_xatom(remote_control->data_format_atom) ;

  x_selection_atom = gdk_x11_atom_to_xatom(selection_event->selection) ;

  /* Get the request from our (server) window. */
  if (XGetWindowProperty(x_display,
			 x_our_window,
			 x_selection_atom,
			 0, max_data,
			 TRUE,				    /* delete property after reading. */
			 AnyPropertyType,			    /* Should be more specific. */
			 &actual_type_return, &actual_format_return,
			 &nitems_return, &bytes_still_left, &prop_return) == Success)
    {
      REMOTELOGMSG(remote_control, "Got window property: \"%s\"", prop_return) ;

      status = TRUE ;
    }
  else
    {
      REMOTELOGMSG(remote_control, "%s", "Could not get window property.") ;

      status = FALSE ;
      /* need to clean up here.... */
    }


  /* I think we can get round this....I think we are passed an atom which we should use.... */
  /* This is a bit of a hack, what we really wanted to happen was for the client to have
   * set a PropertyNotify handler on _our_ window/property but this seems not possible
   * within normal gdk/gtk functions. Instead by doing an XSendEvent() direct to the client
   * we can let them know we have got the property.
   */
  if (status)
    {
      GdkEventClient client_event ;

      /*
	typedef struct {
	GdkEventType type;
	GdkWindow *window;
	gint8 send_event;
	GdkAtom message_type;
	gushort data_format;
	union {
	char b[20];
	short s[10];
	long l[5];
	} data;
	} GdkEventClient;
      */


      REMOTELOGMSG(remote_control, "%s", "Sending event to tell client we have the request.") ;

      client_event.type = GDK_CLIENT_EVENT ;
      client_event.window = our_window ;
      client_event.send_event = TRUE ;
      client_event.message_type = remote_control->data_format_atom ;
      client_event.data_format = 8 ;
      memcpy((void *)(&client_event.data.b), (const void *)REQUEST_RECEIVED, strlen(REQUEST_RECEIVED) + 1) ;

      if ((status = gdk_event_send_client_message((GdkEvent *)&client_event,
						  server_request->peer_window)))
	REMOTELOGMSG(remote_control, "gdk_event_send_client_message() sent ClientMessage to peer %u.",
		     server_request->peer_window) ;
      else
	REMOTELOGMSG(remote_control, "gdk_event_send_client_message() call to send ClientMessage to peer %u failed.",
		     server_request->peer_window) ;
    }


  if (status)
    {
      GdkEventClient do_request_event = {0} ;

      server_request->state = SERVER_STATE_PROCESSING ;

      remote_control->server_message_id = g_signal_connect(remote_control->our_widget,
							   "client-event",
							   (GCallback)serverClientMessageCB, remote_control) ;

      REMOTELOGMSG(remote_control, "%s", "Sending event to call server code to service request.") ;

      do_request_event.type = GDK_CLIENT_EVENT ;
      do_request_event.window = our_window ;
      do_request_event.send_event = TRUE ;
      do_request_event.message_type = remote_control->data_format_atom ;
      do_request_event.data_format = 8 ;
      memcpy((void *)(&do_request_event.data.b), (const void *)CALL_SERVER, strlen(CALL_SERVER) + 1) ;

      if ((status = gdk_event_send_client_message((GdkEvent *)&do_request_event,
						  remote_control->our_window)))
	REMOTELOGMSG(remote_control, "gdk_event_send_client_message() sent ClientMessage to peer %u.",
		     remote_control->our_window) ;
      else
	REMOTELOGMSG(remote_control, "gdk_event_send_client_message() call to send ClientMessage to peer %u failed.",
		     remote_control->our_window) ;
    }


  gdk_display_sync(gdk_display_get_default()) ;


  REMOTELOGMSG(remote_control, "%s", EXIT_TXT) ;

  return result ;
}


/* "client-event" handler, called when the server has retrieved the request from it's property.
 * 
 * 
 * typedef struct {
 *   GdkEventType type;
 *   GdkWindow *window;
 *   gint8 send_event;
 *   GdkAtom message_type;
 *   gushort data_format;
 *   union {
 *     char b[20];
 *     short s[10];
 *     long l[5];
 *   } data;
 * } GdkEventClient;
 * 
 *  */
static gboolean clientClientMessageCB(GtkWidget *widget, GdkEventClient *client_event, gpointer user_data)
{
  gboolean result = FALSE ;
  ZMapRemoteControl remote_control = (ZMapRemoteControl)user_data ;
  ClientRequest client_request = (ClientRequest)remote_control->curr_request ;

  REMOTELOGMSG(remote_control, "%s", ENTER_TXT) ;

  zMapAssert(client_request->state == CLIENT_STATE_SENDING) ;

  /* Disconnect to prevent stray events inteferring. */
  disconnectSignalHandler(remote_control->our_widget, &(remote_control->client_message_id)) ;


  /* We can now free our copy of the request data as the server has a copy and
   * has deleted the copy on our window. */

  REMOTELOGMSG(remote_control, "%s", "Client notifed that server has request, waiting for reply.") ;
  client_request->state = CLIENT_STATE_WAITING ;


  /* This comes too late...this function is called too later for this to work here... */

  REMOTELOGMSG(remote_control, "%s", "set up selection-notify handler to wait for reply from server.") ;
  /* Set up SelectionNotify handler which will be called when client replies to us. */
  remote_control->client_select_notify_id = g_signal_connect(remote_control->our_widget,
							     "selection-notify-event",
							     (GCallback)clientSelectionNotifyCB, remote_control) ;


  REMOTELOGMSG(remote_control, "%s", EXIT_TXT) ;

  return result ;
}



/* "client-event" handler, called when the server sends itself an event to kick off processing
 * the request asynchromously.
 * 
 * 
 * typedef struct {
 *   GdkEventType type;
 *   GdkWindow *window;
 *   gint8 send_event;
 *   GdkAtom message_type;
 *   gushort data_format;
 *   union {
 *     char b[20];
 *     short s[10];
 *     long l[5];
 *   } data;
 * } GdkEventClient;
 * 
 *  */
static gboolean serverClientMessageCB(GtkWidget *widget, GdkEventClient *client_event, gpointer user_data)
{
  gboolean result = FALSE ;
  ZMapRemoteControl remote_control = (ZMapRemoteControl)user_data ;
  ServerRequest server_request = (ServerRequest)remote_control->curr_request ;

  REMOTELOGMSG(remote_control, "%s", ENTER_TXT) ;

  zMapAssert(server_request->state == SERVER_STATE_PROCESSING) ;

  /* Disconnect to prevent stray events inteferring. */
  disconnectSignalHandler(remote_control->our_widget, &(remote_control->server_message_id)) ;

  /* At this point we should give a callback which the server can call us with as request
   * may need to be asynch..... */
  REMOTELOGMSG(remote_control, "%s", "Calling server code....") ;

  (remote_control->request_func)(serverAppReplyCB, remote_control,
				 server_request->request, remote_control->request_data) ;


  REMOTELOGMSG(remote_control, "%s", EXIT_TXT) ;

  return result ;
}


/* Called by app server code when it has finished doing the reply. We then pass reply back to client. */
static gboolean serverAppReplyCB(void *remote_data, void *reply, int reply_len)
{
  gboolean result = FALSE ;
  ZMapRemoteControl remote_control = (ZMapRemoteControl)remote_data ;
  ServerRequest server_request = (ServerRequest)remote_control->curr_request ;
  GdkWindow *our_window ;
  Display *x_display ;
  Window x_our_window ;
  Atom x_property_atom, x_data_format_atom ;
  Atom x_selection_atom ;
  char *x_selection_str ;
  XSelectionEvent x_selection_event ;

  REMOTELOGMSG(remote_control, "%s", ENTER_TXT) ;

  zMapAssert(server_request->state == SERVER_STATE_PROCESSING) ;

  server_request->state = SERVER_STATE_SENDING ;


  our_window = gtk_widget_get_window(remote_control->our_widget) ;
  x_display = GDK_WINDOW_XDISPLAY(our_window) ;
  x_our_window = GDK_WINDOW_XWINDOW(our_window) ;
  x_property_atom = gdk_x11_atom_to_xatom(remote_control->our_atom) ;
  x_data_format_atom = gdk_x11_atom_to_xatom(remote_control->data_format_atom) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  x_selection_atom = gdk_x11_atom_to_xatom(select_event->selection) ;
  x_selection_str = gdk_atom_name(select_event->selection) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  x_selection_atom = gdk_x11_atom_to_xatom(remote_control->our_atom) ;
  x_selection_str = gdk_atom_name(remote_control->our_atom) ;


  REMOTELOGMSG(remote_control, "%s", "Application has returned reply.") ;

  /* Put the reply on to our window. */
  XChangeProperty(x_display,
		  remote_control->our_window,
		  x_selection_atom,
		  x_data_format_atom,
		  8,
		  PropModeReplace,
		  (unsigned char*)(reply),
		  reply_len) ;

  REMOTELOGMSG(remote_control,
	       "Put reply on \"%s\" on X-window %u.",
	       x_selection_str,
	       remote_control->our_window) ;


  /* Set up PropertyNotify handler which will be called when client retrieves reply. */
  remote_control->client_select_notify_id = g_signal_connect(remote_control->our_widget,
							     "property-notify-event",
							     (GCallback)serverPropertyNotifyCB, remote_control) ;



  /* Try putting reply on clients window.... */
  XChangeProperty(x_display,
		  server_request->peer_window,
		  x_selection_atom,
		  x_data_format_atom,
		  8,
		  PropModeReplace,
		  (unsigned char*)(reply),
		  reply_len) ;

  REMOTELOGMSG(remote_control,
	       "Put reply on \"%s\" on X-window %u.",
	       x_selection_str,
	       server_request->peer_window) ;






  /* Send selection which will tell our peer how to retrieve our request.
   * 
   * Event struct is:
   *  typedef struct
   *  {
   *    int type;
   *    unsigned long serial ;
   *    Bool send_event;
   *    Display *display;
   *    Window requestor;
   *    Atom selection;
   *    Atom target;
   *    Atom property;
   *    Time time;
   *  } XSelectionEvent;
   * 
   */
  x_selection_event.type = SelectionNotify ;
  x_selection_event.serial = 0 ;			    /* I don't know how to set this field. */
  x_selection_event.send_event = TRUE ;
  x_selection_event.display = x_display ;
  x_selection_event.requestor = x_our_window ;
  x_selection_event.selection = x_property_atom ;	    /* Don't know what to set here.... */
  x_selection_event.target = x_property_atom ;
  x_selection_event.property = x_property_atom ;
  x_selection_event.time = CurrentTime ;

  /* Tell the server that we've sent the request using a SelectionNotify event. */
  if (XSendEvent(x_display, server_request->peer_window, True, 0, (XEvent *)(&x_selection_event)) == 0)
    REMOTELOGMSG(remote_control, "XSendEvent() call to send SelectionNotify to peer %u failed.",
		 server_request->peer_window) ;
  else
    REMOTELOGMSG(remote_control, "XSendEvent() sent SelectionNotify to peer %u.",
		 server_request->peer_window) ;

  REMOTELOGMSG(remote_control, "%s", EXIT_TXT) ;

  return result ;
}


/* Client "selection-notify-event" handler, called when server has posted reply.
 * 
 * Here's the contents of the selection event:
 *
 * typedef struct {
 * GdkEventType type;
 * GdkWindow *window;
 * gint8 send_event;
 * GdkAtom selection;
 * GdkAtom target;
 * GdkAtom property;
 * guint32 time;
 * GdkNativeWindow requestor;
 * } GdkEventSelection;
 * 
 * 
 * 
 * 
 */
static gboolean clientSelectionNotifyCB(GtkWidget *widget, GdkEventSelection *selection_event, gpointer user_data)
{
  gboolean result = FALSE ;
  gboolean status = TRUE ;
  ZMapRemoteControl remote_control = (ZMapRemoteControl)user_data ;
  ClientRequest client_request = (ClientRequest)remote_control->curr_request ;
  GdkWindow *our_window ;
  Window x_our_window ;
  Display *x_display ;
  Atom x_property_atom, x_data_format_atom ;
  Atom x_selection_atom ;
  long max_data = 20000 ;				    /* Do something sensible about length... */
  Atom actual_type_return = None ;
  int actual_format_return = 0 ;
  unsigned long nitems_return = 0, bytes_still_left = 0 ;
  unsigned char *prop_return = NULL ;


  REMOTELOGMSG(remote_control, "%s", ENTER_TXT) ;

  zMapAssert(client_request->state == CLIENT_STATE_RETRIEVING) ;

  /* Disconnect to prevent stray events inteferring. */
  disconnectSignalHandler(remote_control->our_widget, &(remote_control->client_select_notify_id)) ;

  our_window = gtk_widget_get_window(remote_control->our_widget) ;
  x_display = GDK_WINDOW_XDISPLAY(our_window) ;
  x_our_window = GDK_WINDOW_XWINDOW(our_window) ;
  x_property_atom = gdk_x11_atom_to_xatom(remote_control->our_atom) ;
  x_data_format_atom = gdk_x11_atom_to_xatom(remote_control->data_format_atom) ;

  x_selection_atom = gdk_x11_atom_to_xatom(selection_event->selection) ;

  /* Get the request from our (server) window. */
  if (XGetWindowProperty(x_display,
			 x_our_window,
			 x_selection_atom,
			 0, max_data,
			 TRUE,				    /* delete property after reading. */
			 AnyPropertyType,			    /* Should be more specific. */
			 &actual_type_return, &actual_format_return,
			 &nitems_return, &bytes_still_left, &prop_return) == Success)
    {
      REMOTELOGMSG(remote_control, "Got window property: \"%s\"", prop_return) ;

      status = TRUE ;
    }
  else
    {
      REMOTELOGMSG(remote_control, "%s", "Could not get window property.") ;

      status = FALSE ;
      /* need to clean up here.... */
    }


  /* ok, we should have the reply now so retake ownership of our atom and wait for next
   * request etc.... */
  if ((status = gtk_selection_owner_set(remote_control->our_widget,
					remote_control->our_atom,
					selection_event->time)))
    {
      REMOTELOGMSG(remote_control,
		   "Ownership of atom %s retaken so we are ready for the next request.",
		   remote_control->our_atom_string) ;
    }
  else
    {
      REMOTELOGMSG(remote_control, "%s",
		   "Cannot gain ownership of atom %s, cannot service new requests.") ;
    }


  REMOTELOGMSG(remote_control, "%s", EXIT_TXT) ;

  return result ;
}


/* Server "property-notify-event" handler, called when client has received reply.
 * 
 * Here's the contents of the property event:
 *
 * typedef struct {
 *   GdkEventType type;
 *   GdkWindow *window;
 *   gint8 send_event;
 *   GdkAtom atom;
 *   guint32 time;
 *   guint state;
 * } GdkEventProperty;
 * 
 * 
 */
static gboolean serverPropertyNotifyCB(GtkWidget *widget, GdkEventProperty *property_event, gpointer user_data)
{
  gboolean result = FALSE ;
  gboolean status = TRUE ;
  ZMapRemoteControl remote_control = (ZMapRemoteControl)user_data ;
  ServerRequest server_request = (ServerRequest)remote_control->curr_request ;

  REMOTELOGMSG(remote_control, "%s", ENTER_TXT) ;

  /* Retake ownership of our atom and wait for next request etc.... */
  if ((status = gtk_selection_owner_set(remote_control->our_widget,
					remote_control->our_atom,
					property_event->time)))
    {
      REMOTELOGMSG(remote_control,
		   "Ownership of atom %s retaken so we are ready for the next request.",
		   remote_control->our_atom_string) ;
    }
  else
    {
      REMOTELOGMSG(remote_control, "%s",
		   "Cannot gain ownership of atom %s, cannot service new requests.") ;
    }

  REMOTELOGMSG(remote_control, "%s", EXIT_TXT) ;

  return result ;
}


/* Test if we can get events for property on another window
 * 
 * Here's the contents of the property event:
 *
 * typedef struct {
 *   GdkEventType type;
 *   GdkWindow *window;
 *   gint8 send_event;
 *   GdkAtom atom;
 *   guint32 time;
 *   guint state;
 * } GdkEventProperty;
 * 
 * 
 */
static gboolean testNotifyCB(GtkWidget *widget, GdkEventProperty *property_event, gpointer user_data)
{
  gboolean result = FALSE ;
  gboolean status = TRUE ;
  ZMapRemoteControl remote_control = (ZMapRemoteControl)user_data ;
  ServerRequest server_request = (ServerRequest)remote_control->curr_request ;

  REMOTELOGMSG(remote_control, "%s", ENTER_TXT) ;


  REMOTELOGMSG(remote_control, "%s",
	       "in test notify.") ;


  REMOTELOGMSG(remote_control, "%s", EXIT_TXT) ;

  return result ;
}



/* A GSourceFunc() for time outs. */
static gboolean timeOutCB(gpointer user_data)
{
  gboolean result = TRUE ;
  ZMapRemoteControl remote_control = (ZMapRemoteControl)user_data ;

  REMOTELOGMSG(remote_control, "%s", ENTER_TXT) ;

  remote_control->curr_request->has_timed_out = TRUE ;

  (remote_control->timeout_func)(remote_control->timeout_data) ;

  result = FALSE ;					    /* Returning FALSE removes timer. */

  REMOTELOGMSG(remote_control, "%s", EXIT_TXT) ;

  return result ;
}


/* Make a remote request struct of either client or server type. */
static AnyRequest createRemoteRequest(RemoteType request_type,
				      GdkAtom peer_atom, gulong curr_req_time, char *request)
{
  AnyRequest remote_request = NULL ;

  if (request_type == REMOTE_TYPE_CLIENT)
    {
      ClientRequest client_request ;

      client_request = g_new0(ClientRequestStruct, 1) ;

      client_request->state = CLIENT_STATE_INACTIVE ;

      remote_request = (AnyRequest)client_request ;
    }
  else
    {
      ServerRequest server_request ;

      server_request = g_new0(ServerRequestStruct, 1) ;

      server_request->state = SERVER_STATE_INACTIVE ;

      remote_request = (AnyRequest)server_request ;
    }

  remote_request->request_type = request_type ;

  remote_request->peer_atom = peer_atom ;

  remote_request->curr_req_time = curr_req_time ;

  remote_request->request = g_strdup(request) ;

  return remote_request ;
}



/* Destroy either a client or server request. */
static void destroyRemoteRequest(AnyRequest remote_request)
{
  /* No client or server specific code currently. */

  g_free(remote_request->request) ;

  g_free(remote_request) ;

  return ;
}




/* Called using REMOTELOGMSG(), by default this calls stderrOutputCB() unless caller
 * sets a different output function. */
static void debugMsg(ZMapRemoteControl remote_control, const char *func_name, char *format_str, ...)
{				
  GString *msg ;
  va_list args ;
  char *prefix ;

  msg = g_string_sized_new(500) ;				
		
  if (!remote_control->curr_request)
    {
      prefix = PEER_PREFIX ;
    }
  else
    {
      if (remote_control->curr_request->request_type == REMOTE_TYPE_CLIENT)
	prefix = CLIENT_PREFIX ;
      else
	prefix = SERVER_PREFIX ;
    }

						
  g_string_append_printf(msg, "%s(%s) %s(%u) -\t",			
			 remote_control->app_id,
			 func_name,
			 prefix,
			 remote_control->our_window) ;

  va_start(args, format_str) ;

  g_string_append_vprintf(msg, format_str, args) ;

  va_end(args) ;
									
  (remote_control->err_func)(remote_control->err_data, msg->str) ;			
									
  g_string_free(msg, TRUE) ;					

  return ;
}



/* Sends err_msg to stderr and flushes stderr to make the message appear immediately. */
static gboolean stderrOutputCB(gpointer user_data_unused, char *err_msg)
{
  gboolean result = TRUE ;

  fprintf(stderr, "%s\n", err_msg) ;

  fflush(stderr) ;

  return result ;
}



/* Candidate to go in GUI Utils....
 * 
 * NOTE, the other event masks need filling in, I've only done the ones I'm interested in...
 *  */
static gboolean windowGeneralEventCB(GtkWidget *wigdet, GdkEvent *event, gpointer user_data)
{
  gboolean handled = FALSE;
  ZMapRemoteControl remote_control = (ZMapRemoteControl)user_data ;
  GdkEventAny *any_event = (GdkEventAny *)event ;
  int true_index ;
  eventTxtStruct event_txt[] =
    {
      {"GDK_NOTHING", 0},				    /* = -1 */
      {"GDK_DELETE", 0},				    /* = 0 */
      {"GDK_DESTROY", 0},				    /* = 1 */
      {"GDK_EXPOSE", GDK_EXPOSURE_MASK},		    /* = 2 */
      {"GDK_MOTION_NOTIFY", GDK_POINTER_MOTION_MASK},	    /* = 3 */
      {"GDK_BUTTON_PRESS", 0},				    /* = 4 */
      {"GDK_2BUTTON_PRESS", 0},				    /* = 5 */
      {"GDK_3BUTTON_PRESS", 0},				    /* = 6 */
      {"GDK_BUTTON_RELEASE", 0},			    /* = 7 */
      {"GDK_KEY_PRESS", 0},				    /* = 8 */
      {"GDK_KEY_RELEASE", 0},				    /* = 9 */
      {"GDK_ENTER_NOTIFY", GDK_ENTER_NOTIFY_MASK},	    /* = 10 */
      {"GDK_LEAVE_NOTIFY", GDK_LEAVE_NOTIFY_MASK},	    /* = 11 */
      {"GDK_FOCUS_CHANGE", GDK_FOCUS_CHANGE_MASK},	    /* = 12 */
      {"GDK_CONFIGURE", 0},				    /* = 13 */
      {"GDK_MAP", 0},					    /* = 14 */
      {"GDK_UNMAP", 0},					    /* = 15 */
      {"GDK_PROPERTY_NOTIFY", 0},			    /* = 16 */
      {"GDK_SELECTION_CLEAR", 0},			    /* = 17 */
      {"GDK_SELECTION_REQUEST", 0},			    /* = 18 */
      {"GDK_SELECTION_NOTIFY", 0},			    /* = 19 */
      {"GDK_PROXIMITY_IN", 0},				    /* = 20 */
      {"GDK_PROXIMITY_OUT", 0},				    /* = 21 */
      {"GDK_DRAG_ENTER", 0},				    /* = 22 */
      {"GDK_DRAG_LEAVE", 0},				    /* = 23 */
      {"GDK_DRAG_MOTION", 0},				    /* = 24 */
      {"GDK_DRAG_STATUS", 0},				    /* = 25 */
      {"GDK_DROP_START", 0},				    /* = 26 */
      {"GDK_DROP_FINISHED", 0},				    /* = 27 */
      {"GDK_CLIENT_EVENT", 0},				    /* = 28 */
      {"GDK_VISIBILITY_NOTIFY", GDK_VISIBILITY_NOTIFY_MASK}, /* = 29 */
      {"GDK_NO_EXPOSE", 0},				    /* = 30 */
      {"GDK_SCROLL", 0},				    /* = 31 */
      {"GDK_WINDOW_STATE", 0},				    /* = 32 */
      {"GDK_SETTING", 0},				    /* = 33 */
      {"GDK_OWNER_CHANGE", 0},				    /* = 34 */
      {"GDK_GRAB_BROKEN", 0},				    /* = 35 */
      {"GDK_DAMAGE", 0},				    /* = 36 */
      {"GDK_EVENT_LAST", 0}				    /* helper variable for decls */
    } ;

  true_index = any_event->type + 1 ;			    /* yuch, see enum values in comments above. */


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if (any_event->type == GDK_CLIENT_EVENT)
    {
      GdkEventClient *client_event = (GdkEventClient *)any_event ;

      printf("found it\n") ;
    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  if (!msg_exclude_mask_G || !(event_txt[true_index].mask & msg_exclude_mask_G))
    {
      REMOTELOGMSG(remote_control, "Event: \"%s\"\tXWindow:%u",
		   event_txt[true_index].text,
		   GDK_WINDOW_XWINDOW(any_event->window)) ;
    }

  return handled ;
}



/* These two noddy functions stop us forgetting to reset the func ids.... */
static void disconnectSignalHandler(GtkWidget *widget, gulong *func_id)
{
  if (*func_id)
    {
      g_signal_handler_disconnect(widget, *func_id) ;
      *func_id = 0 ;
    }

  return ;
}


static void disconnectTimerHandler(guint *func_id)
{
  if (*func_id)
    {
      gtk_timeout_remove(*func_id) ;
      *func_id = 0 ;
    }

  return ;
}


/* Return state whether client or server as a string. */
static char *getStateAsString(ZMapRemoteControl remote_control)
{
  char *state_str = NULL ;

  if (remote_control->curr_request->request_type == REMOTE_TYPE_CLIENT)
    state_str = (char *)clientState2ExactStr(((ClientRequest)remote_control->curr_request)->state) ;
  else
    state_str = (char *)serverState2ExactStr(((ServerRequest)remote_control->curr_request)->state) ;

  return state_str ;
}

