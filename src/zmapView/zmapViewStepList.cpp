/*  File: zmapViewStepList.cpp
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
 * Description: 
 *
 * Exported functions: See XXXXXXXXXXXXX.h
 *
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>

#include <glib.h>


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
#include <zmapView_P.hpp>
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

#include <zmapViewServer.hpp>


/* Used in stepListFindRequest() to search for a step or a connection. */
typedef struct
{
  ZMapServerReqType request_type ;
  ZMapViewConnectionStep step ;
  ZMapViewConnectionRequest request ;
} StepListFindStruct, *StepListFind ;


/* Used in zmapViewStepListStepRequestDeleteAll(). */
typedef struct
{
  ZMapViewConnectionStepList step_list ;
  ZMapViewConnection connection ;
} StepListDeleteStruct, *StepListDelete ;



static void stepFindReq(gpointer data, gpointer user_data) ;
static ZMapViewConnectionStep stepListFindStep(ZMapViewConnectionStepList step_list, ZMapServerReqType request_type) ;
static void stepDestroy(gpointer data, gpointer user_data) ;




/*
 *                  External interface functions.
 */




/*
 *                  External package functions.
 */


/*
 *
 * A series of functions for handling the stepping through of sending requests to slave threads.
 *
 */


/* Make the step list. */
ZMapViewConnectionStepList zmapViewStepListCreate(StepListDispatchCB dispatch_func,
						  StepListProcessDataCB process_func,
						  StepListFreeDataCB free_func)
{
  ZMapViewConnectionStepList step_list ;

  step_list = g_new0(ZMapViewConnectionStepListStruct, 1) ;

  step_list->dispatch_func = dispatch_func ;
  step_list->process_func = process_func ;
  step_list->free_func = free_func ;

  return step_list ;
}


/* Add a step to the step list, if it's the first step to be added then the current step will be
 * set to this first step. */
void zmapViewStepListAddStep(ZMapViewConnectionStepList step_list, ZMapServerReqType request_type,
			     StepListActionOnFailureType on_fail)
{
  ZMapViewConnectionStep step ;
  gboolean first = FALSE ;

  if (!(step_list->steps))
    first = TRUE ;

  step = g_new0(ZMapViewConnectionStepStruct, 1) ;
  step->state = STEPLIST_INVALID ;        // might not be requested
  step->on_fail = on_fail ;
  step->request = request_type ;
  step_list->steps = g_list_append(step_list->steps, step) ;

  if (first)
    step_list->current = step_list->steps ;		    /* Make current the first step. */

  return ;
}


/* Add a connection to the specified step in the step list. */
ZMapViewConnectionRequest zmapViewStepListAddServerReq(ZMapViewConnectionStepList step_list,
						       ZMapViewConnection view_con,
						       ZMapServerReqType request_type,
						       gpointer request_data,
						       StepListActionOnFailureType on_fail)
{
  ZMapViewConnectionRequest request = NULL ;
  ZMapViewConnectionStep step ;

  if ((step = stepListFindStep(step_list, request_type)))
    {
      request = g_new0(ZMapViewConnectionRequestStruct, 1) ;
      step->state = request->state = STEPLIST_PENDING ;           // some duplication here?
      request->request_data = request_data ;
      step->connection_req = request ;

      if(on_fail != 0)
	step->on_fail = on_fail ;
      //printf("add request %d to %s\n",request_type,view_con->url);
    }

  return request ;
}



// create a complete step list. each step will be processed is a request is added
// steps default to STEPLIST_INVALID
ZMapViewConnectionStepList zmapViewConnectionStepListCreate(StepListDispatchCB dispatch_func,
							    StepListProcessDataCB process_func,
							    StepListFreeDataCB free_func)
{
  ZMapViewConnectionStepList step_list;

  step_list = zmapViewStepListCreate(dispatch_func,
				     process_func,
				     free_func) ;
  zmapViewStepListAddStep(step_list, ZMAP_SERVERREQ_CREATE,    REQUEST_ONFAIL_CANCEL_THREAD) ;
  zmapViewStepListAddStep(step_list, ZMAP_SERVERREQ_OPEN,      REQUEST_ONFAIL_CANCEL_THREAD) ;
  zmapViewStepListAddStep(step_list, ZMAP_SERVERREQ_GETSERVERINFO, REQUEST_ONFAIL_CANCEL_THREAD) ;
  zmapViewStepListAddStep(step_list, ZMAP_SERVERREQ_FEATURESETS, REQUEST_ONFAIL_CANCEL_THREAD) ;
  zmapViewStepListAddStep(step_list, ZMAP_SERVERREQ_STYLES,    REQUEST_ONFAIL_CANCEL_THREAD) ;
  zmapViewStepListAddStep(step_list, ZMAP_SERVERREQ_NEWCONTEXT,REQUEST_ONFAIL_CANCEL_THREAD) ;
  zmapViewStepListAddStep(step_list, ZMAP_SERVERREQ_FEATURES,  REQUEST_ONFAIL_CANCEL_THREAD) ;
  zmapViewStepListAddStep(step_list, ZMAP_SERVERREQ_SEQUENCE,  REQUEST_ONFAIL_CANCEL_THREAD) ;
  zmapViewStepListAddStep(step_list, ZMAP_SERVERREQ_GETSTATUS,  REQUEST_ONFAIL_CANCEL_THREAD) ;
  zmapViewStepListAddStep(step_list, ZMAP_SERVERREQ_TERMINATE, REQUEST_ONFAIL_CANCEL_THREAD) ;

  return(step_list);

}


/* The meat of the step list stuff, check step list to see where we are and dispatch next
 * step if we are ready for it.
 *
 *  */
void zmapViewStepListIter(ZMapViewConnection view_con)
{
  ZMapViewConnectionStepList step_list = view_con->step_list;
  gboolean result = FALSE;

  if (step_list->current)
    {
      ZMapViewConnectionStep curr_step ;

      curr_step = (ZMapViewConnectionStep)(step_list->current->data) ;

      switch (curr_step->state)
	{
	case STEPLIST_PENDING:
	  {
	    /* All requests are in STEPLIST_PENDING state and after dispatching will go into STEPLIST_DISPATCHED. */

            /* Call users dispatch func. */
	    if ((step_list->dispatch_func))
	      result = step_list->dispatch_func(view_con, (ZMapServerReqAny)(curr_step->connection_req->request_data)) ;

	    if (result || !(step_list->dispatch_func))
	      {
		zMapThreadRequest(view_con->thread, curr_step->connection_req->request_data) ;

		curr_step->connection_req->state = STEPLIST_DISPATCHED ;
		curr_step->state = STEPLIST_DISPATCHED ;
	      }
	    else
	      {
		curr_step->connection_req->has_failed = TRUE ;
	      }

	    break ;
	  }
	case STEPLIST_DISPATCHED:
	  {
	    if (curr_step->connection_req->state != STEPLIST_DISPATCHED)
	      curr_step->state = STEPLIST_FINISHED ;

	    break ;
	  }
	case STEPLIST_INVALID:  // if a step is not used then skip over it
	case STEPLIST_FINISHED:
	  {
	    /* Move to next step, do not dispatch, that is our callers decision.
	     * If this is the last step then current will be set to NULL. */
	    step_list->current = step_list->current->next ;

	    break ;
	  }
	default:
	  {
	    zMapLogFatalLogicErr("switch(), unknown value: %d", curr_step->state) ;

	    break ;
	  }
	}
    }

  return ;
}



/* Find a connections request in a given request step. */
ZMapViewConnectionRequest zmapViewStepListFindRequest(ZMapViewConnectionStepList step_list,
						      ZMapServerReqType request_type, ZMapViewConnection connection)
{
  ZMapViewConnectionRequest request = NULL ;

  if (step_list)
    {
      StepListFindStruct step_find = {ZMAP_SERVERREQ_INVALID} ;

      step_find.request_type = request_type ;
      step_find.request = NULL ;

      g_list_foreach(step_list->steps, stepFindReq, &step_find) ;

      request = step_find.request ;
    }
  else
    {
      zMapLogWarning("%s", "Program error: step_list is NULL");
    }

  return request ;
}


/* Process data from request, gets called from main connection loop of the View which itself is
 * called whenever a connection thread returns data.
 *  */
void zmapViewStepListStepProcessRequest(ZMapViewConnection view_con, ZMapViewConnectionRequest request)
{
  gboolean result = TRUE ;
  ZMapViewConnectionStepList step_list = view_con->step_list;

  /* Call users request processing func, if this signals the connection has failed then
   * remove it from the step list. */
  if ((step_list->process_func))
    result = step_list->process_func(view_con, (ZMapServerReqAny)(request->request_data)) ;

  if (result)
    {
      request->state = STEPLIST_FINISHED ;
    }

  return ;
}


/*                Step list funcs.                             */

/* Free the list of steps. */

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
void zmapViewStepListDestroy(ZMapViewConnection view_con)
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
void zmapViewStepListDestroy(ZMapViewConnectionStepList step_list)
{
  g_list_foreach(step_list->steps, stepDestroy, step_list) ;
  g_list_free(step_list->steps) ;
  step_list->steps = NULL ;
  g_free(step_list) ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  view_con->step_list = NULL;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* THIS IS COMPLETELY AND UTTERLY THE WRONG PLACE TO DO THIS....AGGGGGHHHHHHH...... */

  if(view_con->request_data)
    {
      g_free(view_con->request_data) ;
      view_con->request_data = NULL ;
    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return ;
}



/* Test to see if there are any more steps to perform, does not look to see if there are requests. */
gboolean zmapViewStepListIsNext(ZMapViewConnectionStepList step_list)
{
  gboolean more = FALSE ;

  if (step_list->current)
    more = TRUE ;

  return more ;
}





/*
 *               Internal routines.
 */


static void stepFindReq(gpointer data, gpointer user_data)
{
  ZMapViewConnectionStep step = (ZMapViewConnectionStep)data ;
  StepListFind step_find = (StepListFind)user_data ;

  if (!(step_find->request_type) || step->request == step_find->request_type)
    {
      step_find->step = step ;
      step_find->request = step->connection_req;
    }

  return ;
}



/* Find a connections request in a given request step. */
static ZMapViewConnectionStep stepListFindStep(ZMapViewConnectionStepList step_list, ZMapServerReqType request_type)
{
  ZMapViewConnectionStep step = NULL ;
  StepListFindStruct step_find = {ZMAP_SERVERREQ_INVALID} ;

  step_find.request_type = request_type ;
  step_find.step = NULL ;

  g_list_foreach(step_list->steps, stepFindReq, &step_find) ;

  step = step_find.step ;

  return step ;
}




/* Free a step. */
static void stepDestroy(gpointer data, gpointer user_data)
{
  ZMapViewConnectionStep step = (ZMapViewConnectionStep)data ;
  ZMapViewConnectionStepList step_list = (ZMapViewConnectionStepList)user_data ;

  if(step->connection_req)
    {
      /* Call users free func. */
      if ((step_list->free_func))
	step_list->free_func((ZMapServerReqAny)(step->connection_req->request_data)) ;

      g_free(step->connection_req) ;
    }

  step->connection_req = NULL ;

  g_free(step) ;

  return ;
}


