/*  File: zmapMaster.cpp
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2016: Genome Research Ltd.
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
 *      Steve Miller (Sanger Institute, UK) sm23@sanger.ac.uk
 *      Gemma Barson (Sanger Institute, UK) gb10@sanger.ac.uk
 *
 * Description: Functions to handle replies from slave threads.
 *
 * Exported functions: See ZMap/zmapThreads.hpp
 *
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>

#include <glib.h>
#include <gtk/gtk.h>
#include <string>

#include <ZMap/zmapThreads.hpp>
#include <ZMap/zmapUtilsDebug.hpp>

#include <zmapSlave_P.hpp>


using namespace std ;




namespace ZMapThreadSource
{



/* Formatting thread debug messages. */
#define THREAD_DEBUG_MSG_PREFIX " Reply from NEW_CODE slave thread %s, "

#define THREAD_DEBUG_MSG(CHILD_THREAD, FORMAT_STR, ...)        \
  G_STMT_START                                                                \
  {                                                                        \
    if (thread_debug_G)                                                 \
      {                                                                 \
        char *thread_str ;                                              \
                                                                        \
        thread_str = zMapThreadGetThreadID((CHILD_THREAD)) ;                \
                                                                        \
        zMapDebugPrint(thread_debug_G, THREAD_DEBUG_MSG_PREFIX FORMAT_STR, thread_str, __VA_ARGS__) ; \
                                                                        \
        zMapLogMessage(THREAD_DEBUG_MSG_PREFIX FORMAT_STR, thread_str, __VA_ARGS__) ; \
                                                                        \
        g_free(thread_str) ;                                                \
      }                                                                 \
  } G_STMT_END

/* Define thread debug messages with extra information, used in checkStateConnections() mostly. */
#define THREAD_DEBUG_MSG_FULL(CHILD_THREAD, VIEW_CON, REQUEST_TYPE, REPLY, FORMAT_STR, ...) \
  G_STMT_START                                                                \
  {                                                                        \
    if (thread_debug_G)                                                 \
      {                                                                 \
        GString *msg_str ;                                              \
                                                                        \
        msg_str = g_string_new("") ;                                    \
                                                                        \
        g_string_append_printf(msg_str, "status = \"%s\", request = \"%s\", reply = \"%s\", msg = \"" FORMAT_STR "\"", \
                               zMapViewThreadStatus2ExactStr((VIEW_CON)->thread_status), \
                               zMapServerReqType2ExactStr((REQUEST_TYPE)), \
                               zMapThreadReply2ExactStr((REPLY)),       \
                               __VA_ARGS__) ;                           \
                                                                        \
        g_string_append_printf(msg_str, ", url = \"%s\"", zMapServerGetUrl((VIEW_CON))) ; \
                                                                        \
        THREAD_DEBUG_MSG((CHILD_THREAD), "%s", msg_str->str) ;          \
                                                                        \
        g_string_free(msg_str, TRUE) ;                                  \
      }                                                                 \
  } G_STMT_END




static gint sourceCheckCB(gpointer cb_data) ;



//
// Globals
//

// milliseconds delay between checks for reply from source thread.
static const guint32 source_check_interval_G = 100 ;

static bool thread_debug_G = false ;





//
//                           External Interface
//


  ThreadSource::ThreadSource(ZMapSlaveRequestHandlerFunc req_handler_func,
                 ZMapSlaveTerminateHandlerFunc terminate_handler_func,
                 ZMapSlaveDestroyHandlerFunc destroy_handler_func)
{
  // Add checking of args to this function, throw if not provided....


  thread_ = zMapThreadCreate(TRUE, zmapNewThread,
                                req_handler_func, terminate_handler_func, destroy_handler_func) ;


  return ;
}


  ThreadSource::ThreadSource(bool new_interface,
                 ZMapSlaveRequestHandlerFunc req_handler_func,
                 ZMapSlaveTerminateHandlerFunc terminate_handler_func,
                 ZMapSlaveDestroyHandlerFunc destroy_handler_func)
{
  // Add checking of args to this function, throw if not provided....


  thread_ = zMapThreadCreate(new_interface, zmapNewThread,
                             req_handler_func, terminate_handler_func, destroy_handler_func) ;


  return ;
}





// NEED TO ALLOW A SEPARATE TYPE OF TIMER TO BE USED....PERHAPS THROUGH A GET/SET INTERFACE....

// Start the GTK timer function that will check for a reply from the source thread.
//
// (Note, we don't use a gtk idle function because it gets repeatedly called ALL 
// the time that zmap is not doing anything which is unnecessary and makes it look
// like zmap is looping using 100% CPU.)
//
void ThreadSource::SlaveStartPoll(ZMapThreadPollSlaveUserReplyFunc user_reply_func,
                            void *user_reply_func_data)
{
  user_reply_func_ = user_reply_func ;
  user_reply_func_data_ = user_reply_func_data ;

  // WARNING: gtk_timeout_add is deprecated and should not be used in newly-written code. Use g_timeout_add() instead.
  poll_id_ = gtk_timeout_add(source_check_interval_G, sourceCheckCB, this) ;

  return ;
}


void ThreadSource::SendThreadRequest(void *request)
{

  zMapThreadRequest(thread_, request) ;

  return ;
}

  ZMapThread  ThreadSource::GetThread()
  {
    return thread_ ;
  }
  




// Stop the GTK timer function checking for a reply from the source thread.
//
void ThreadSource::SlaveStopPoll()
{
  if (poll_id_)
    {
      gtk_timeout_remove(poll_id_) ;

      poll_id_ = 0 ;
    }

  return ;
}


// AMAZINGLY THIS WAS MISSING....BUT IS NEEDED, WE DON'T WANT TO JUST CANCEL OUT OF A THREAD....
//
// Stop the thread and destroy it.
//
 ThreadSource::~ThreadSource()
{


  // stop polling if not already stopped....



  return ;
}









//
//                     Internal routines
//




//
//        Functions to check and control the connection to a source thread.
//


// Called every source_check_interval_G milliseconds to see if the source thread has replied.
//
static gint sourceCheckCB(gpointer cb_data)
{
  gint call_again = 0 ;
  ThreadSource *thread = (ThreadSource *)cb_data ;


  /* Returning a value > 0 tells gtk to call sourceCheckCB again, so if sourceCheck() returns
   * TRUE we ask to be called again. */
  if (ThreadSource::sourceCheck(*thread))
    call_again = 1 ;

  return call_again ;
}


/* This function checks the status of the connection and checks for any reply and
 * then acts on it, it gets called from the ZMap idle function.
 * If all threads are ok and zmap has not been killed then routine returns TRUE
 * meaning it wants to be called again, otherwise FALSE.
 *
 * NOTE that you cannot use a condvar here, if the connection thread signals us using a
 * condvar we will probably miss it, that just doesn't work, we have to pole for changes
 * and this is possible because this routine is called from the idle function of the GUI.
 *
 *  */


bool ThreadSource::sourceCheck(ThreadSource &thread_source)
{
  gboolean call_again = TRUE ;                                    /* Normally we want to be called continuously. */
  ZMapThreadReply reply = ZMAPTHREAD_REPLY_INVALID ;
  void *data = NULL ;
  char *err_msg = NULL ;


  if (!(zMapThreadGetReplyWithData(thread_source.thread_, &reply, &data, &err_msg)))
    {
      // something bad has happened if we can't get a reply so kill the thread.

      THREAD_DEBUG_MSG(thread_source.thread_, "cannot access reply from child, error was  \"%s\"", err_msg) ;

      zMapLogCritical("Thread %p will be killed as cannot access reply from child, error was  \"%s\"",
                      thread_source.thread_, err_msg) ;

      zMapThreadKill(thread_source.thread_) ;
      
      // signal that we are finished.
      call_again = FALSE ;
    }
  else
    {
      if (reply != ZMAPTHREAD_REPLY_WAIT)
        {
          // Thread has either quit normally with ZMAPTHREAD_REPLY_QUIT or something has gone
          // wrong and the thread needs to be killed.
          if (reply != ZMAPTHREAD_REPLY_QUIT)
            {
              zMapLogWarning("Thread %p failed with \"%s\" so will be killed.",
                             thread_source.thread_, zMapThreadReply2ExactStr(reply)) ;

              zMapThreadKill(thread_source.thread_) ;
            }

          // Call the user callback and tell them what happened.
          (thread_source.user_reply_func_)(thread_source.user_reply_func_data_) ;

          // signal that we are finished.
          call_again = FALSE ;
        }
    }


  return call_again ;
}



} // End of ZMapThread namespace

