/*  File: zmapthrutils.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) Sanger Institute, 2003
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
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk,
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk and
 *	Simon Kelley (Sanger Institute, UK) srk@sanger.ac.uk
 *
 * Description: 
 * Exported functions: See XXXXXXXXXXXXX.h
 * HISTORY:
 * Last edited: Nov 12 16:42 2003 (edgrif)
 * Created: Thu Jul 24 14:37:35 2003 (edgrif)
 * CVS info:   $Id: zmapConnutils.c,v 1.1 2003-11-13 15:02:13 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <errno.h>
#include <zmapConn_P.h>


static void releaseCondvarMutex(void *thread_data) ;




void zmapCondVarCreate(ZMapRequest thread_state)
{
  int status ;

  if ((status = pthread_mutex_init(&(thread_state->mutex), NULL)) != 0)
    {
      ZMAPSYSERR(status, "mutex init") ;
    }

  if ((status = pthread_cond_init(&(thread_state->cond), NULL)) != 0)
    {
      ZMAPSYSERR(status, "cond init") ;
    }
  
  thread_state->state = ZMAP_REQUEST_INIT ;
  
  return ;
}

void zmapCondVarSignal(ZMapRequest thread_state, ZMapThreadRequest new_state)
{
  int status ;
  
  pthread_cleanup_push(releaseCondvarMutex, (void *)thread_state) ;
  
  if ((status = pthread_mutex_lock(&(thread_state->mutex))) != 0)
    {
      ZMAPSYSERR(status, "zmapCondVarSignal mutex lock") ;
    }

  thread_state->state = new_state ;

  if ((status = pthread_cond_signal(&(thread_state->cond))) != 0)
    {
      ZMAPSYSERR(status, "zmapCondVarSignal cond signal") ;
    }

  if ((status = pthread_mutex_unlock(&(thread_state->mutex))) != 0)
    {
      ZMAPSYSERR(status, "zmapCondVarSignal mutex unlock") ;
    }

  pthread_cleanup_pop(0) ;				    /* 0 => only call cleanup if cancelled. */

  return ;
}


/* Blocking wait.... */
void zmapCondVarWait(ZMapRequest thread_state, ZMapThreadRequest waiting_state)
{
  int status ;

  pthread_cleanup_push(releaseCondvarMutex, (void *)thread_state) ;

  if ((status = pthread_mutex_lock(&(thread_state->mutex))) != 0)
    {
      ZMAPSYSERR(status, "zmapCondVarWait mutex lock") ;
    }

  while (thread_state->state == waiting_state)
    {
      if ((status = pthread_cond_wait(&(thread_state->cond), &(thread_state->mutex))) != 0)
	{
	  ZMAPSYSERR(status, "zmapCondVarWait cond wait") ;
	}
    }

  if ((status = pthread_mutex_unlock(&(thread_state->mutex))) != 0)
    {
      ZMAPSYSERR(status, "zmapCondVarWait mutex unlock") ;
    }

  pthread_cleanup_pop(0) ;				    /* 0 => only call cleanup if cancelled. */

  return ;
}


/* timed wait, returns FALSE if cond var was not signalled (i.e. it timed out),
 * TRUE otherwise....
 * NOTE that you can optionally get the condvar state reset to the waiting_state, this
 * can be useful if you are calling this routine from a loop in which you wait until
 * something has changed from the waiting state...i.e. somehow you need to return to the
 * waiting state before looping again. */
ZMapThreadRequest zmapCondVarWaitTimed(ZMapRequest condvar, ZMapThreadRequest waiting_state,
				       TIMESPEC *relative_timeout, gboolean reset_to_waiting)
{
  ZMapThreadRequest signalled_state = ZMAP_REQUEST_INIT ;
  int status ;
  TIMESPEC systimeout ;


  pthread_cleanup_push(releaseCondvarMutex, (void *)condvar) ;


  if ((status = pthread_mutex_lock(&(condvar->mutex))) != 0)
    {
      ZMAPSYSERR(status, "zmapCondVarWait mutex lock") ;
    }
  
  /* Get the relative timeout converted to absolute for the call. */
  if ((status = pthread_get_expiration_np(relative_timeout, &systimeout)) != 0)
    ZMAPSYSERR(status, "zmapCondVarWaitTimed invalid time") ;

  while (condvar->state == waiting_state)
    {
      if ((status = pthread_cond_timedwait(&(condvar->cond), &(condvar->mutex),
					   &systimeout)) != 0)
	{
	  if (status == ETIMEDOUT)			    /* Timed out so return. */
	    {
	      condvar->state = ZMAP_REQUEST_TIMED_OUT ;
	      break ;
	    }
	  else
	    ZMAPSYSERR(status, "zmapCondVarWait cond wait") ;
	}
    }

  signalled_state = condvar->state ;			    /* return signalled end state. */

  /* optionally reset current state to wait state. */
  if (reset_to_waiting)
    condvar->state = waiting_state ;

  if ((status = pthread_mutex_unlock(&(condvar->mutex))) != 0)
    {
      ZMAPSYSERR(status, "zmapCondVarWait mutex unlock") ;
    }

  pthread_cleanup_pop(0) ;				    /* 0 => only call cleanup if cancelled. */

  return signalled_state ;
}


void zmapCondVarDestroy(ZMapRequest thread_state)
{
  int status ;
  
  if ((status = pthread_cond_destroy(&(thread_state->cond))) != 0)
    {
      ZMAPSYSERR(status, "cond destroy") ;
    }
  

  if ((status = pthread_mutex_destroy(&(thread_state->mutex))) != 0)
    {
      ZMAPSYSERR(status, "mutex destroy") ;
    }
  
  return ;
}


/* Must be kept in step with declaration of ZMapThreadRequest enums in zmapConn_P.h */
char *zmapVarGetRequestString(ZMapThreadRequest signalled_state)
{
  char *str_states[] = {"ZMAP_REQUEST_INIT", "ZMAP_REQUEST_WAIT", "ZMAP_REQUEST_TIMED_OUT",
			"ZMAP_REQUEST_GETDATA", "ZMAP_REQUEST_EXIT"} ;

  return str_states[signalled_state] ;
}




/* this set of routines manipulates the variable in the thread state struct but do not
 * involve the Condition Variable. */

void zmapVarCreate(ZMapReply thread_state)
{
  int status ;

  if ((status = pthread_mutex_init(&(thread_state->mutex), NULL)) != 0)
    {
      ZMAPSYSERR(status, "mutex init") ;
    }

  thread_state->state = ZMAP_REPLY_INIT ;
  
  return ;
}



void zmapVarSetValue(ZMapReply thread_state, ZMapThreadReply new_state)
{
  int status ;

  if ((status = pthread_mutex_lock(&(thread_state->mutex))) != 0)
    {
      ZMAPSYSERR(status, "zmapVarSetValue mutex lock") ;
    }

  thread_state->state = new_state ;

  if ((status = pthread_mutex_unlock(&(thread_state->mutex))) != 0)
    {
      ZMAPSYSERR(status, "zmapVarSetValue mutex unlock") ;
    }

  return ;
}


/* Returns TRUE if it could read the value (i.e. the mutex was unlocked)
 * and returns the value in state_out,
 * returns FALSE otherwise. */
gboolean zmapVarGetValue(ZMapReply thread_state, ZMapThreadReply *state_out)
{
  gboolean unlocked = TRUE ;
  int status ;

  if ((status = pthread_mutex_trylock(&(thread_state->mutex))) != 0)
    {
      if (status == EBUSY)
	unlocked = FALSE ;
      else
	ZMAPSYSERR(status, "zmapVarGetValue mutex lock") ;
    }
  else
    {
      *state_out = thread_state->state ;
      unlocked = TRUE ;

      if ((status = pthread_mutex_unlock(&(thread_state->mutex))) != 0)
	{
	  ZMAPSYSERR(status, "zmapVarGetValue mutex unlock") ;
	}
    }

  return unlocked ;
}


void zmapVarSetValueWithData(ZMapReply thread_state, ZMapThreadReply new_state, void *data)
{
  int status ;

  if ((status = pthread_mutex_lock(&(thread_state->mutex))) != 0)
    {
      ZMAPSYSERR(status, "zmapVarSetValueWithData mutex lock") ;
    }

  thread_state->state = new_state ;
  thread_state->data = data ;

  if ((status = pthread_mutex_unlock(&(thread_state->mutex))) != 0)
    {
      ZMAPSYSERR(status, "zmapVarSetValueWithData mutex unlock") ;
    }

  return ;
}


void zmapVarSetValueWithError(ZMapReply thread_state, ZMapThreadReply new_state, char *err_msg)
{
  int status ;

  if ((status = pthread_mutex_lock(&(thread_state->mutex))) != 0)
    {
      ZMAPSYSERR(status, "zmapVarSetValueWithError mutex lock") ;
    }

  thread_state->state = new_state ;
  thread_state->error_msg = err_msg ;

  if ((status = pthread_mutex_unlock(&(thread_state->mutex))) != 0)
    {
      ZMAPSYSERR(status, "zmapVarSetValueWithError mutex unlock") ;
    }

  return ;
}


/* Returns TRUE if it could read the value (i.e. the mutex was unlocked)
 * and returns the value in state_out and also if there is any data it
 * is returned in data_out and if there is an err_msg it is returned in err_msg_out,
 * returns FALSE if it could not read the value. */
gboolean zmapVarGetValueWithData(ZMapReply thread_state, ZMapThreadReply *state_out,
				 void **data_out, char **err_msg_out)
{
  gboolean unlocked = TRUE ;
  int status ;

  if ((status = pthread_mutex_trylock(&(thread_state->mutex))) != 0)
    {
      if (status == EBUSY)
	unlocked = FALSE ;
      else
	ZMAPSYSERR(status, "zmapVarGetValue mutex lock") ;
    }
  else
    {
      *state_out = thread_state->state ;
      if (thread_state->data)
	*data_out = thread_state->data ;
      if (thread_state->error_msg)
	*err_msg_out = thread_state->error_msg ;

      unlocked = TRUE ;

      if ((status = pthread_mutex_unlock(&(thread_state->mutex))) != 0)
	{
	  ZMAPSYSERR(status, "zmapVarGetValue mutex unlock") ;
	}
    }

  return unlocked ;
}


void zmapVarDestroy(ZMapReply thread_state)
{
  int status ;
  
  if ((status = pthread_mutex_destroy(&(thread_state->mutex))) != 0)
    {
      ZMAPSYSERR(status, "mutex destroy") ;
    }
  
  return ;
}



/* Must be kept in step with declaration of ZMapThreadReply enums in zmapConn_P.h */
char *zmapVarGetReplyString(ZMapThreadReply signalled_state)
{
  char *str_states[] = {"ZMAP_REPLY_INIT", "ZMAP_REPLY_WAIT",
			"ZMAP_REPLY_GOTDATA", "ZMAP_REPLY_EXIT", "ZMAP_REPLY_DESTROYED"} ;

  return str_states[signalled_state] ;
}




/* 
 * -----------------------  Internal Routines  -----------------------
 */

/* Called when a thread gets cancelled while waiting on a mutex to ensure that the mutex
 * gets released. */
static void releaseCondvarMutex(void *thread_data)
{
  ZMapRequest condvar = (ZMapRequest)thread_data ;
  int status ;

  ZMAP_THR_DEBUG(("releaseCondvarMutex cleanup handler\n")) ;

  if ((status = pthread_mutex_unlock(&(condvar->mutex))) != 0)
    {
      ZMAPSYSERR(status, "releaseCondvarMutex cleanup handler - mutex unlock") ;
    }

  return ;
}
