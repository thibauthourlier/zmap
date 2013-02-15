/*  File: zmapSlave.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006-2012: Genome Research Ltd.
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
 * and was written by
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *   Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description: This code is called when a new thread is created by the
 *              zmapThread code, it loops waiting for commands from
 *              a master thread.
 *
 * Exported functions: See zmapConn_P.h
 *              
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.h>

#include <stdio.h>
#include <string.h>

/* YOU SHOULD BE AWARE THAT ON SOME PLATFORMS (E.G. ALPHAS) THERE SEEMS TO BE SOME INTERACTION
 * BETWEEN pthread.h AND gtk.h SUCH THAT IF pthread.h COMES FIRST THEN gtk.h INCLUDES GET MESSED
 * UP AND THIS FILE WILL NOT COMPILE.... */
#include <pthread.h>

#include <ZMap/zmapUtils.h>

/* With some additional calls in the zmapConn code I could get rid of the need for
 * the private header here......check this out... */
#include <zmapThreads_P.h>
#include <zmapSlave_P.h>


enum {ZMAPTHREAD_SLAVE_REQ_BUFSIZE = 512} ;


static void cleanUpThread(void *thread_args) ;


/* This is the routine that is called by the pthread_create() function, in effect
 * this is an endless loop processing requests signalled to the thread via a
 * condition variable. If this thread kills itself we will exit this routine
 * normally, if the thread is cancelled however we may exit from any thread cancellable
 * system call currently in progress or any of our cancellation points so its
 * important that the clean up routine registered with pthread_cleanup_push()
 * really does clean up properly. */
void *zmapNewThread(void *thread_args)
{
  zmapThreadCB thread_cb = NULL ;
  ZMapThread thread = (ZMapThread)thread_args ;
  ZMapRequest thread_state = &(thread->request) ;
  TIMESPEC timeout ;
  ZMapThreadRequest signalled_state ;
  ZMapThreadReturnCode slave_response =  ZMAPTHREAD_RETURNCODE_OK;
  int call_clean = 1 ;   // call the cleanup function on exit?


  zmap_thread_debug_G = TRUE ;


  ZMAPTHREAD_DEBUG(thread, "%s", "main thread routine starting....") ;

  /* We set up the thread struct and then immediately set up our pthread clean up routine
   * to catch errors, if we are cancelled before this then the clean up routine will not
   * be called. */
  thread_cb = g_new0(zmapThreadCBstruct, 1) ;
  thread_cb->thread = thread ;
  thread_cb->thread_died = FALSE ;
  thread_cb->initial_error = NULL ;
  thread_cb->slave_data = NULL ;
#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  thread_cb->server_request = ZMAPTHREAD_SERVERREQ_INVALID ;
  thread_cb->server_reply = NULL ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  pthread_cleanup_push(cleanUpThread, (void *)thread_cb) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* somehow doing this screws up the whole gui-slave communication bit...not sure how....
   * the slaves just die, almost all the time.... */

  /* Signal that we are ready and waiting... */
  zMapConnSetReply(thread, ZMAPTHREAD_REPLY_WAIT) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  /* Next two calls added to fix MACOSX pthread issues */
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) ;
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL) ;


  while (slave_response != ZMAPTHREAD_RETURNCODE_QUIT)
    {
      void *request ;

      ZMAPTHREAD_DEBUG(thread, "%s", "about to do timed wait.") ;

      /* this will crap over performance...asking the time all the time !! */
      timeout.tv_sec = 5 ;				    /* n.b. interface seems to absolute time. */
      timeout.tv_nsec = 0 ;
      request = NULL ;
      signalled_state = zmapCondVarWaitTimed(thread_state, ZMAPTHREAD_REQUEST_WAIT, &timeout, TRUE,
					     &request) ;

      ZMAPTHREAD_DEBUG(thread, "finished condvar wait, state = %s", zMapThreadRequest2ExactStr(signalled_state)) ;

      /* pthread_testcancel fix for MACOSX */
      pthread_testcancel();

      if (signalled_state == ZMAPTHREAD_REQUEST_TIMED_OUT)
	{
	  continue ;
	}
      else if (signalled_state == ZMAPTHREAD_REQUEST_EXECUTE)
	{

	  void *reply ;
	  char *slave_error = NULL ;

	  /* Must have a request at this stage.... */
	  zMapAssert(request) ;


	  ZMAPTHREAD_DEBUG(thread, "%s", "calling server to service request....") ;

	  zMapPrintTimer(NULL, "In thread, calling handler function") ;

	  /* Call the registered slave handler function. */
	  slave_response = (*(thread->handler_func))(&(thread_cb->slave_data), request, &reply, &slave_error) ;

	  zMapPrintTimer(NULL, "In thread, returned from handler function") ;

	  ZMAPTHREAD_DEBUG(thread, "returned from server, response was %s....",
			   zMapThreadReturnCode2ExactStr(slave_response)) ;


	  /* The handling below is not now correct, if a call fails we need to kill the thread
	   * we can't cope with dangling threads....we will need to set the thread_died flag */

	  switch (slave_response)
	    {
	    case ZMAPTHREAD_RETURNCODE_OK:
	      {
		ZMAPTHREAD_DEBUG(thread, "%s", "got all data....") ;

		/* Signal that we got some data. */
		zmapVarSetValueWithData(&(thread->reply), ZMAPTHREAD_REPLY_GOTDATA, request) ;

		request = NULL ;			    /* Reset, we don't free this data. */

		break ;
	      }
	    case ZMAPTHREAD_RETURNCODE_REQFAIL:
	      {
		char *error_msg ;

		ZMAPTHREAD_DEBUG(thread, "%s", "request failed....") ;

		error_msg = g_strdup_printf("(a) %s - %s", ZMAPTHREAD_SLAVEREQUEST, slave_error) ;

		/* Signal that we failed. */
		zmapVarSetValueWithErrorAndData(&(thread->reply), ZMAPTHREAD_REPLY_REQERROR, error_msg, request) ;

		request = NULL ;

		break ;
	      }
	    case ZMAPTHREAD_RETURNCODE_TIMEDOUT:
	      {
		char *error_msg ;

		ZMAPTHREAD_DEBUG(thread, "%s", "request failed....") ;

		error_msg = g_strdup_printf("(b) %s - %s", ZMAPTHREAD_SLAVEREQUEST, slave_error) ;

		/* Signal that we failed. */
		zmapVarSetValueWithError(&(thread->reply), ZMAPTHREAD_REPLY_REQERROR, error_msg) ;

		break ;
	      }
	    case ZMAPTHREAD_RETURNCODE_BADREQ:
	      {
		char *error_msg ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
		connection->initial_request = NULL ;	    /* make sure there is explicitly no
							       data returned. */
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

		ZMAPTHREAD_DEBUG(thread, "%s", "bad request....") ;

		error_msg = g_strdup_printf("(c) %s - %s", ZMAPTHREAD_SLAVEREQUEST, slave_error) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
		/* Should revisit use of pthread_cancel.....for clearing up.... */

		/* not sure we need to do this...happens in the cleanup routine.... */

		/* Signal that we failed. */
		zmapVarSetValueWithError(&(thread->reply), ZMAPTHREAD_REPLY_DIED, error_msg) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


		thread_cb->thread_died = TRUE ;

		thread_cb->initial_error = g_strdup(error_msg) ;

		goto clean_up ;

		break ;
	      }

	    case ZMAPTHREAD_RETURNCODE_SERVERDIED:
	      {
		char *error_msg ;

		ZMAPTHREAD_DEBUG(thread, "%s", "server died....") ;

		error_msg = g_strdup_printf("(d) %s - %s", ZMAPTHREAD_SLAVEREQUEST, slave_error) ;

		if (!thread_cb->thread_died)		/* a misnomer, it's the server that the thread talks to */
		  zMapLogWarning("server died","") ;
		thread_cb->thread_died = TRUE ;

		thread_cb->initial_error = g_strdup(error_msg) ;

		/* must continue on to getStatus if it's in the step list
		 * zmapServer functions will not run if status is DIED
		 */
		zmapVarSetValueWithError(&(thread->reply), ZMAPTHREAD_REPLY_DIED, error_msg) ;
		//            goto clean_up ;

		/* Try doing this....what used to go wrong ?? */
//		goto clean_up ;

		break;
	      }

	    case ZMAPTHREAD_RETURNCODE_QUIT:
	      {
		char * error_msg = "OK";

		/* this message goes to the otterlace features loaded message
		   and no error gets mangled into a string that says (Server Pipe: - null)
		   there's no obvious way to get the real exit status here
		   due to the structure of the code and data
		   its unfeasably difficult to detect a sucessful server here and we can only report
		   "(no error: ( no error ( no error)))"
		 */
		error_msg = g_strdup_printf("(e) %s - %s (%s)",
					    ZMAPTHREAD_SLAVEREQUEST, "server terminated", slave_error) ;
		zmapVarSetValueWithError(&(thread->reply), ZMAPTHREAD_REPLY_QUIT, error_msg) ;

		// we've already closed the connection and cleaned up data
		// but we still want to exit
		// don't call the cleanup function as it's there as an exception handler
		call_clean = 0 ;

		break;
	      }

	    default:
	      {
		zMapAssertNotReached() ;

		break ;
	      }

	    }
	}


      /* pthread_testcancel fix for MACOSX */
      // mh17: we could end up doing this after a QUIT, but WTH it's a race condition
      pthread_testcancel();


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      else
	{
	  /* unknown state......... */
	  thread_cb->thread_died = TRUE ;
	  thread_cb->initial_error = g_strdup_printf("Thread received unknown state from GUI: %s",
						     zMapVarGetRequestString(signalled_state)) ;
	  goto clean_up ;
	}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

    }



  /* Note that once we reach here the thread will exit, the pthread_cleanup_pop(1) ensures
   * we call our cleanup routine before we exit.
   * Most times we will not get here because we will be pthread_cancel'd and go straight into
   * our clean_up routine. */

 clean_up:

  ZMAPTHREAD_DEBUG(thread, "%s", "main thread routine exitting....") ;


  /* something about 64 bit pthread needs this at the end. */
  /* cleanup_push and pop are basically fancy open and close braces so
   * there must be some code between the "clean_up:" label and this pop or it doesn't compile! */
  pthread_cleanup_pop(call_clean) ;     /* 1 => always call clean up routine */

  return thread_args ;
}


/* Gets called when:
 *  1) There is an error and the thread has to exit.
 *  2) The thread gets cancelled, can happen in any of the POSIX cancellable
 *     system calls (e.g. read, select etc.) or from anywhere we declare as a cancellation point.
 *
 * This means some care is needed in handling resources to be released, we may need to set flags
 * to remember which resources have been allocated.
 * In particular we DO NOT free the thread_cb->thread, this is done by the GUI thread
 * which created it.
 */
static void cleanUpThread(void *thread_args)
{
  zmapThreadCB thread_cb = (zmapThreadCB)thread_args ;
  ZMapThread thread = thread_cb->thread ;
  ZMapThreadReply reply ;
  gchar *error_msg = NULL ;

  zMapAssert(thread_args) ;

  ZMAPTHREAD_DEBUG(thread, "%s", "thread clean-up routine: starting....") ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* I think this is probably a very bad idea isn't it...what if this thread didn't have the lock,
   * just seems like asking for trouble.... */

  zMapThreadForkUnlock(); // not needed, but play safe. See zmapThread.c
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



  if (thread_cb->thread_died)
    {
      ZMAPTHREAD_DEBUG(thread, "%s", "thread clean-up routine: thread has died...") ;

      reply = ZMAPTHREAD_REPLY_DIED ;

      if (thread_cb->initial_error)
	error_msg = g_strdup(thread_cb->initial_error) ;
    }
  else
    {
      ZMAPTHREAD_DEBUG(thread, "%s", "thread clean-up routine: thread has been cancelled...") ;

      reply = ZMAPTHREAD_REPLY_CANCELLED ;

      /* If thread was cancelled we need to ensure it is terminated correctly. */
      if (thread_cb->slave_data)
	{
	  ZMapThreadReturnCode slave_response ;

	  /* Call the registered slave handler function. */
	  if ((slave_response = (*(thread->terminate_func))(&(thread_cb->slave_data), &error_msg))
	      != ZMAPTHREAD_RETURNCODE_OK)
	    {
	      ZMAPTHREAD_DEBUG(thread, "%s", "Unable to close connection to server cleanly") ;
	    }
	}
    }

  /* Now make sure thread is destroyed correctly. */
  if (thread_cb->slave_data)
    {
      ZMapThreadReturnCode slave_response ;

      ZMAPTHREAD_DEBUG(thread, "%s", "thread clean-up routine: calling slave terminate function...") ;

      /* Call the registered slave handler function. */
      if ((slave_response = (*(thread->destroy_func))(&(thread_cb->slave_data)))
	  != ZMAPTHREAD_RETURNCODE_OK)
	{
	  ZMAPTHREAD_DEBUG(thread, "%s", "thread clean-up routine: Unable to destroy connection") ;
	}
    }

  g_free(thread_cb) ;

  /* And report what happened..... */
  if (!error_msg)
    {
      ZMAPTHREAD_DEBUG(thread, "%s", "thread clean-up routine: no errors") ;

      zmapVarSetValue(&(thread->reply), reply) ;
    }
  else
    {
      ZMAPTHREAD_DEBUG(thread, "%s", "thread clean-up routine: error: %s") ;

      zmapVarSetValueWithError(&(thread->reply), reply, error_msg) ;
    }


  ZMAPTHREAD_DEBUG(thread, "%s", "thread clean-up routine exitting because....") ;

  return ;
}



