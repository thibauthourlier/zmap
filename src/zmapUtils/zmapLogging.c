/*  File: zmapLogging.c
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
 * originally written by:
 *
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *   Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description: Log functions for zmap app. Allows start/stop, switching
 *              on/off. Currently there is just one global log for the
 *              whole application.
 *
 * Exported functions: See zmapUtilsLog.h
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.h>

#include <string.h>


#ifdef HAVE_CONFIG_H
#include <config.h>		/* config generated by configure */
#endif

#ifdef HAVE_GNUC_BACKTRACE
#include <execinfo.h>		/* backtrace() & backtrace_symbols_fd() */
#endif /* HAVE_GNUC_BACKTRACE */

#include <stdio.h>
#include <sys/param.h>					    /* MAXHOSTNAMLEN */
#include <unistd.h>					    /* for pid stuff. STDIN_FILENO too. */
#include <signal.h>		/* signals... */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <ZMap/zmapConfigDir.h>
#include <ZMap/zmapConfigIni.h>
#include <ZMap/zmapConfigStrings.h>
#include <ZMap/zmapUtils.h>


#define ZMAPLOG_MAX_TRACE_SIZE 50

#define FOO_LOG 0


/* Common struct for all log handlers, we use these to turn logging on/off. */
typedef struct
{
  guint cb_id ;						    /* needed by glib to install/uninstall
							       handler routines. */
  GLogFunc log_cb ;					    /* handler routine must change to turn
							       logging on/off. */

  /* log file name/file etc. */
  char *log_path ;
  GIOChannel* logfile ;

} zmapLogHandlerStruct, *zmapLogHandler ;



/* State for the logger as a whole. */
typedef struct  _ZMapLogStruct
{
  GMutex*  log_lock ;					    /* Ensure only single threading in log
							       handler routine. */

  /* Logging action. */
  gboolean logging ;					    /* logging on or off ? */
  zmapLogHandlerStruct active_handler ;			    /* Used when logging is on. */
  zmapLogHandlerStruct inactive_handler ;		    /* Used when logging is off. */
  gboolean log_to_file ;				    /* log to file or leave glib to log to
							       stdout/err ? */
  /* Log record content. */
  gchar *userid ;
  gchar *nodeid ;
  int pid ;
  gboolean show_code_details ;				    /* If TRUE then the file, function and
							       line are displayed for every message. */
  gboolean show_time;                                 /* if TRUE the the time is included */

  gboolean catch_glib;
  gboolean echo_glib;   /* to stdout if caught */

} ZMapLogStruct ;




static ZMapLog createLog(void) ;
static void destroyLog(ZMapLog log) ;

static gboolean configureLog(ZMapLog log) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
static gboolean getLogConf(ZMapLog log) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


static gboolean startLogging(ZMapLog log) ;
static gboolean stopLogging(ZMapLog log, gboolean remove_all_handlers) ;

static gboolean openLogFile(ZMapLog log) ;
static gboolean closeLogFile(ZMapLog log) ;

static void writeStartOrStopMessage(gboolean start) ;

static void nullLogger(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message,
		       gpointer user_data) ;
static void fileLogger(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message,
		       gpointer user_data) ;
static void glibLogger(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message,
		       gpointer user_data);
static gboolean backtrace2fd(unsigned int remove, int fd);
#if FOO_LOG
static void logTime(int what, int how) ;
#endif




/* We only ever have one log so its kept internally here. */
static ZMapLog log_G = NULL ;

static GTimer *zmap_log_timer_G = NULL;

static gboolean enable_core_dumping_G = TRUE;



void foo_logger(char *x)
{
	/* can't call this from foo as it's a macro */
	zMapLogWarning(x,"");
}


/* This function is NOT thread safe, you should not call this from individual threads. The log
 * package expects you to call this outside of threads then you should be safe to use the log
 * routines inside threads.
 * Note that the package expects the caller to have called  g_thread_init() before calling this
 * function. */
gboolean zMapLogCreate(char *logname)
{
  gboolean result = FALSE ;
  ZMapLog log = log_G ;

  zMapAssert(!log) ;

#if FOO_LOG	// log timing stats from foo
		// have to take this out to get xremote to compile for perl
		// should be ok when we get the new xremote

  extern void (*foo_log)(char *x);


  if (zmap_timing_G)
    {
      extern void (*foo_timer)(int,int);

      foo_timer = logTime;
    }

  if (zmap_debug_G)
    {
      extern void (*foo_log_stack)(void);

      foo_log_stack = zMapPrintStack;
    }

  foo_log = foo_logger;

#endif


  log_G = log = createLog() ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if (!configureLog(log))
    {
      destroyLog(log) ;
      log_G = log = NULL ;
    }
  else
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
    {
      zmap_log_timer_G = g_timer_new();
      result = TRUE ;
    }

  return result ;
}


void zMapWriteStartMsg(void)
{
  ZMapLog log = log_G ;

  zMapAssert(log) ;

  writeStartOrStopMessage(TRUE) ;

  return ;
}

void zMapWriteStopMsg(void)
{
  ZMapLog log = log_G ;

  zMapAssert(log) ;

  writeStartOrStopMessage(FALSE) ;

  return ;
}


/* Configure the log. */
gboolean zMapLogConfigure(gboolean logging, gboolean log_to_file,
			  gboolean show_code_details, gboolean show_time,
			  gboolean catch_glib, gboolean echo_glib,
			  char *logfile_path)
{
  gboolean result = FALSE ;
  ZMapLog log = log_G ;

  /* Log at all ? */
  log->logging = logging ;

  /* logging to file ? */
  log->log_to_file = log_to_file ;

  /* How much detail to show in log records ? */
  log->show_code_details = show_code_details ;

  log->show_time = show_time ;

  /* glib error message handling, catch GLib errors, else they stay on stdout */
  log->catch_glib = catch_glib ;
  log->echo_glib = echo_glib ;

  /* user specified dir, default to config dir */
  log->active_handler.log_path = logfile_path ;

  result = configureLog(log) ;

  return result ;
}


/* The log and Start and Stop routines write out a record to the log to show start and stop
 * of the log but there is a window where a thread could get in and write to the log
 * before/after they do. We'll just have to live with this... */
gboolean zMapLogStart()
{
  gboolean result = FALSE ;
  ZMapLog log = log_G ;

  zMapAssert(log) ;

  g_mutex_lock(log->log_lock) ;

  result = startLogging(log) ;

  g_mutex_unlock(log->log_lock) ;

  return result ;
}



void zMapLogTime(int what, int how, long data, char *string)
{
  static double times[N_TIMES];
  static double when[N_TIMES];
  double x,e;

  /* THERE IS A WHOLE ENUMS MECHANISM TO AVOID HAVING TO DO THIS....SEE zmapEnum.h */
  /* these mirror the #defines in zmapUtilsDebug.h */
  char *which[] = { "none", "foo-expose", "foo-update",
		    "foo-draw", "draw_context", "revcomp", "zoom", "bump", "setvis", "load", 0 } ;

  if (zmap_timing_G)
    {
      e = zMap_elapsed();

      switch(how)
	{
	case TIMER_CLEAR:
	  times[what] = 0;
	  zMapLogMessage("Timed: %s %s (%ld) Clear",which[what],string,data);
	  break;
	case TIMER_START:
	  when[what] = e;
	  zMapLogMessage("Timed: %s %s (l%d) Start  %.3f",which[what],string,data,e);
	  break;
	case TIMER_STOP:
	  x = e - when[what];
	  times[what] += x;
	  zMapLogMessage("Timed: %s %s (%ld) Stop %.3f = %.3f",which[what],string,data,x,times[what]);
	  break;
	case TIMER_ELAPSED:
	  zMapLogMessage("Timed: %s %s (%ld) Elasped %.3f",which[what],string,data,e);
	  break;
	}
    }

  return ;
}

int zMapLogFileSize(void)
{
  int size = -1 ;
  ZMapLog log = log_G ;
  struct stat file_stats ;

  zMapAssert(log) ;

  if (log->log_to_file)
    {
      if (g_stat(log->active_handler.log_path, &file_stats) == 0)
	{
	  size = file_stats.st_size ;
	}
    }

  return size ;
}


void zMapLogMsg(char *domain, GLogLevelFlags log_level,
		char *file, const char *function, int line,
		char *format, ...)
{
  ZMapLog log = log_G ;
  va_list args ;
  GString *format_str ;
  char *msg_level = NULL ;


  zMapAssert(log) ;

  zMapAssert(domain && *domain && file && *file && format && *format) ;

  format_str = g_string_sized_new(2000) ;		    /* Not too many records longer than this. */

  /* All messages have the nodeid and pid as qualifiers to help with logfile analysis. */
//  g_string_append_printf(format_str, "%s[%s:%s:%d]",
//			 ZMAPLOG_PROCESS_TUPLE, log->userid, log->nodeid, log->pid) ;

  /* include a timestamp? */
  if (log->show_time)
    {
      char tbuf[32];
#if 0
      // this is a mess of incompatable structs....
      // Glib does not do time!
      // they provide a 'portable' interface to gettimeofday  but then don't provide any functions to use it
      struct timeval time;
      struct timezone tz = {0,0};

      gettimeofday(&time,&tz);
      strftime(tbuf,32,"%H:%M:%S",&time);
      g_string_append_printf(format_str, "%s",tbuf);
#else

      if (zmap_log_timer_G)
	{
	  double t = g_timer_elapsed(zmap_log_timer_G,NULL);

	  sprintf(tbuf," %.3fsec",t);
	  g_string_append_printf(format_str, "%s",tbuf);
	}

#endif
    }

  /* If code details are wanted then output them in the log. */
  if (log->show_code_details)
    {
      char *file_basename ;

      file_basename = g_path_get_basename(file) ;

      g_string_append_printf(format_str, "\t%s[%s:%s%s:%d]",
			     ZMAPLOG_CODE_TUPLE,
			     file_basename,
			     (function ? function : ""),
			     (function ? "()" : ""),
			     line) ;

      g_free(file_basename) ;
    }


  switch(log_level)
    {
    case G_LOG_LEVEL_MESSAGE:
      msg_level = "Information" ;
      break ;
    case G_LOG_LEVEL_WARNING:
      msg_level = "Warning" ;
      break ;
    case G_LOG_LEVEL_CRITICAL:
      msg_level = "Critical" ;
      break ;
    case G_LOG_LEVEL_ERROR:
      msg_level = "Fatal" ;
      break ;
    default:
      zMapAssertNotReached() ;
      break ;
    }
  g_string_append_printf(format_str, "\t%s[%s:%s]\n",
			 ZMAPLOG_MESSAGE_TUPLE, msg_level, format) ;

  va_start(args, format) ;
  g_logv(domain, log_level, format_str->str, args) ;
  va_end(args) ;

  g_string_free(format_str, TRUE) ;

  return ;
}

/* Given access to gnu glibc with backtrace and backtrace_symbols_fd
 * this function will log the current stack to the log file. An
 * example of the output is

 ./zmap(zMapLogStack+0x10d)[0x808226e]
 ./zmap[0x807600a]
 ./zmap(zMapFeatureContextExecuteComplete+0xa2)[0x8075752]
 ./zmap[0x80a282e]
 ./zmap(zmapWindowDrawFeatures+0x536)[0x80a1bb2]
 ./zmap[0x8090db2]
 /software/acedb/lib/libgtk-x11-2.0.so.0[0x40173ff0]
 /software/acedb/lib/libgobject-2.0.so.0(g_closure_invoke+0xd5)[0x406de6f5]
 /software/acedb/lib/libgobject-2.0.so.0[0x406f099f]
 /software/acedb/lib/libgobject-2.0.so.0(g_signal_emit_valist+0x52c)[0x406ef85c]
 /software/acedb/lib/libgobject-2.0.so.0(g_signal_emit+0x26)[0x406efcf6]
 /software/acedb/lib/libgtk-x11-2.0.so.0[0x402958a7]
 /software/acedb/lib/libgtk-x11-2.0.so.0(gtk_main_do_event+0x2ba)[0x401719ba]

*/

void zMapLogStack(void)
{
  ZMapLog log = log_G;
  int log_fd = 0;
  gboolean logged = FALSE;

  zMapAssert(log);

  /* we get recurstion aborts */
  /*  zMapLogMessage("%s (static symbol names _not_ available):", "Logging Stack");*/

  g_mutex_lock(log->log_lock);

  zMapAssert(log->logging);

  if (log->active_handler.logfile &&
     (log_fd = g_io_channel_unix_get_fd(log->active_handler.logfile)))
    {
      logged = backtrace2fd(1, log_fd);
    }

  g_mutex_unlock(log->log_lock);

  /*  if (!logged)
      zMapLogWarning("Failed to log stack trace to fd %d. "
      "Check availability of function backtrace()",
      log_fd);
  */
  return ;
}



/* As zMapLogStack(), but to stderr instead. */
void zMapPrintStack(void)
{
  backtrace2fd(1, STDERR_FILENO);
  return ;
}

static void signal_write(int fd, char *buffer, int size, gboolean *result_in_out)
{
  if (result_in_out && (*result_in_out == TRUE))
    {
      ssize_t expected = size, actual = 0;
      if ((write(fd, (void *)NULL, 0)) == 0)
	{
	  actual = write(fd, (void *)buffer, size);
	  if (actual == 0 || actual == -1)
	    *result_in_out = FALSE;
	  else if (actual == expected)
	    *result_in_out = TRUE;
	}
      else
	*result_in_out = FALSE;
    }

  return ;
}

void zMapSignalHandler(int sig_no)
{
  char *sig_name = NULL;
  int sig_name_len = 0;
  gboolean write_result = TRUE;

  /* ensure we don't get back in here. */
  /* If we return rather than exit, we'll also produce a core file. */
  signal(sig_no, SIG_DFL);

  switch(sig_no)
    {
    case SIGSEGV:
      sig_name = "SIGSEGV";
      sig_name_len = 7;
      break;
    case SIGBUS:
      sig_name = "SIGBUS";
      sig_name_len = 6;
      break;
    case SIGABRT:
      sig_name = "SIGABRT";
      sig_name_len = 7;
      break;
    default:
      break;
    }

  /* Only do stuff segv, abrt & bus signals */
  switch(sig_no)
    {
    case SIGSEGV:
    case SIGBUS:
    case SIGABRT:
      /*                           123456789012345678901234567890123456789012345678901234567890 */
      signal_write(STDERR_FILENO, "=== signal handler - caught signal ", 35, &write_result);
      signal_write(STDERR_FILENO, sig_name, sig_name_len, &write_result);
      signal_write(STDERR_FILENO, " - ZMap ", 8, &write_result);
      signal_write(STDERR_FILENO, zMapGetAppVersionString(), strlen(zMapGetAppVersionString()), &write_result);
      signal_write(STDERR_FILENO, " ===\nStack:\n", 12, &write_result);
      /*                           123456789012345678901234567890123456789012345678901234567890 */
      if (!backtrace2fd(0, STDERR_FILENO))
	signal_write(STDERR_FILENO, "*** no backtrace() available ***\n", 33, &write_result);

      /* If we exit, no core file is dumped. */
      if (!enable_core_dumping_G)
	_exit(EXIT_FAILURE);
      break;
    default:
      break;
    }

  return ;
}

gboolean zMapLogStop(void)
{
  gboolean result = FALSE ;
  ZMapLog log = log_G ;

  zMapAssert(log) ;

  g_mutex_lock(log->log_lock) ;

  result = stopLogging(log, FALSE) ;

  g_mutex_unlock(log->log_lock) ;

  return result ;
}


/* This routine should be thread safe, it locks the mutex and then removes our handler so none
 * of our logging handlers can be called any more, it then unlocks the mutex and destroys it.
 * This should be ok because logging calls in the code go via g_log and know nothing about
 * the ZMapLog log struct.
 */
void zMapLogDestroy(void)
{
  ZMapLog log = log_G ;

  zMapAssert(log) ;

  g_mutex_lock(log->log_lock) ;

  stopLogging(log, TRUE) ;

  g_mutex_unlock(log->log_lock) ;

  destroyLog(log) ;

  log_G = NULL ;

  return ;
}




/*
 *                Internal routines.
 */




static ZMapLog createLog(void)
{
  ZMapLog log ;

  log = g_new0(ZMapLogStruct, 1) ;

  /* Must lock access to log as may come from serveral different threads. */
  log->log_lock = g_mutex_new() ;

  /* Default will be logging active. */
  log->logging = TRUE ;
  log->log_to_file = TRUE ;

  /* By default we log to a file, but if we don't specify a callback, glib logging will default
   * to stdout/err so logging can still be active without a file. Note that most of this gets
   * filled in later. */
  log->active_handler.cb_id = 0 ;
  log->active_handler.log_cb = fileLogger ;
  log->active_handler.log_path = NULL ;
  log->active_handler.logfile = NULL ;

  /* inactive handler will be our nullLogger routine. */
  log->inactive_handler.cb_id = 0 ;
  log->inactive_handler.log_cb = nullLogger ;
  log->inactive_handler.log_path = NULL ;
  log->inactive_handler.logfile = NULL ;

  log->userid = g_strdup(g_get_user_name()) ;

  log->nodeid = g_malloc0(MAXHOSTNAMELEN + 2) ;		    /* + 2 for safety, interface not clear. */
  if (gethostname(log->nodeid, (MAXHOSTNAMELEN + 1)) == -1)
    {
      zMapLogCritical("%s", "Cannot retrieve hostname for this computer.") ;
      sprintf(log->nodeid, "%s", "**NO-NODEID**") ;
    }
  log->pid = getpid() ;

  return log ;
}


static void destroyLog(ZMapLog log)
{
  if (log->active_handler.log_path)
    g_free(log->active_handler.log_path) ;

  g_free(log->userid) ;

  g_free(log->nodeid) ;

  g_mutex_free(log->log_lock) ;

  g_free(log) ;

  return ;
}



/* We start and stop logging by swapping from a logging routine that writes to file with one
 * that does nothing. */
static gboolean startLogging(ZMapLog log)
{
  gboolean result = FALSE ;

  if (!(log->logging))
    {
      /* If logging inactive then remove our inactive handler. */
      if (log->inactive_handler.cb_id)
	{
	  g_log_remove_handler(ZMAPLOG_DOMAIN, log->inactive_handler.cb_id) ;
	  log->inactive_handler.cb_id = 0 ;
	}

      /* Only need to do something if we are logging to a file. */
      if (log->log_to_file)
	{
	  if (openLogFile(log))
	    {
	      log->active_handler.cb_id = g_log_set_handler(ZMAPLOG_DOMAIN,
							    G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
							    | G_LOG_FLAG_RECURSION,
							    log->active_handler.log_cb,
							    (gpointer)log) ;

	      /* try to get the glib critical errors handled */
	      if (log->catch_glib)
		g_log_set_default_handler(glibLogger, (gpointer) log);
	      result = TRUE ;
	    }
	}
      else
	result = TRUE ;

      if (result)
	log->logging = TRUE ;
    }

  return result ;
}

/* Error handling is crap here.... */
static gboolean stopLogging(ZMapLog log, gboolean remove_all_handlers)
{
  gboolean result = FALSE ;

  if (log->logging)
    {
      /* If we are logging to a file then remove our handler routine and close the file. */
      if (log->log_to_file)
	{
	  g_log_remove_handler(ZMAPLOG_DOMAIN, log->active_handler.cb_id) ;
	  log->active_handler.cb_id = 0 ;

	  /* try to get the glib critical errors handled */
	  if (log->catch_glib)
	    g_log_set_default_handler(g_log_default_handler, NULL);

	  /* Need to close the log file here. */
	  if (!closeLogFile(log))
	    result = FALSE ;
	}


      /* If we just want to suspend logging then we install our own null handler which does
       * nothing, hence logging stops. If we want to stop all our logging then we do not install
       * our null handler. */
      if (!remove_all_handlers)
	{
	  log->inactive_handler.cb_id = g_log_set_handler(ZMAPLOG_DOMAIN,
							  G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
							  | G_LOG_FLAG_RECURSION,
							  log->inactive_handler.log_cb,
							  (gpointer)log) ;

	  /* try to get the glib critical errors handled */
	  if (log->catch_glib)
	    g_log_set_default_handler(g_log_default_handler, NULL);
	}

      if (result)
	log->logging = FALSE ;
    }
  else
    {
      if (remove_all_handlers && log->inactive_handler.cb_id)
	{
	  g_log_remove_handler(ZMAPLOG_DOMAIN, log->inactive_handler.cb_id) ;
	  log->inactive_handler.cb_id = 0 ;
	  /* try to get the glib critical errors handled */

	  if (log->catch_glib)
	    g_log_set_default_handler(g_log_default_handler, NULL);

	}
    }

  return result ;
}



/* Read the configuration information logging and set up the log. */
static gboolean configureLog(ZMapLog log)
{
  gboolean result = FALSE ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if ((result = getLogConf(log)))
    {
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

      /* This seems stupid, but the start/stopLogging routines need logging to be in the
       * opposite state so we negate the current logging state before calling them,
       * only pertains to initialisation. */
      log->logging = !(log->logging) ;

      if (log->logging)
	{

	  result = stopLogging(log, FALSE) ;
	}
      else
	{
	  /* If we are logging to a file then we will log via our own routine,
	   * otherwise by default glib will log to stdout/err. */
	  if (log->log_to_file)
	    {
	      log->active_handler.log_cb = fileLogger ;
	    }

	  result = startLogging(log) ;
	}

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  return result ;
}


static gboolean openLogFile(ZMapLog log)
{
  gboolean result = FALSE ;
  GError *g_error = NULL ;

  /* Must be opened for appending..... */
  if ((log->active_handler.logfile = g_io_channel_new_file(log->active_handler.log_path,
							   "a", &g_error)))
    {
      result = TRUE ;
    }
  else
    {
      result = FALSE ;

      /* We should be using the Gerror here..... */
    }


  return result ;
}



static gboolean closeLogFile(ZMapLog log)
{
  gboolean result = FALSE ;
  GIOStatus g_status ;
  GError *g_error = NULL ;

  if ((g_status = g_io_channel_shutdown(log->active_handler.logfile, TRUE, &g_error))
      != G_IO_STATUS_NORMAL)
    {
      /* disaster....need to report the error..... */


    }
  else
    {
      g_io_channel_unref(log->active_handler.logfile) ;
      result = TRUE ;
    }

  return result ;
}


/* Write out a start or stop record. */
static void writeStartOrStopMessage(int start)
{
  char *time_str ;

  time_str = zMapGetTimeString(ZMAPTIME_STANDARD, NULL) ;

  zMapLogMessage("****  %s  Session %s %s  ****", zMapGetAppTitle(), (start ? "started" : "stopped"), time_str) ;

  g_free(time_str) ;

  return ;
}



/* BIG QUESTION HERE...ARE WE SUPPOSED TO FREE THE MESSAGE FROM WITHIN THIS ROUTINE OR IS THAT
 * DONE BY THE glog calling code ?? Docs say nothing, perhaps need to look in code..... */


/* We use this routine to "stop" logging because it doesn't do anything so no log records are
 * output. When we want to restart logging we simply restore the fileLogger() routine as the
 * logging routine. */
static void nullLogger(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message,
		       gpointer user_data)
{


  return ;
}


/* This routine is called by the g_log package as a callback to do the actual logging. Note that
 * we have to lock and unlock a mutex here because g_log, although it mutex locks its own stuff
 * does not lock this routine...STUPID...they should have an option to do this if they don't
 * like having it on all the time. */
static void fileLogger(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message,
		       gpointer user_data)
{
  ZMapLog log = (ZMapLog)user_data ;
  GError *g_error = NULL ;
  gsize bytes_written = 0 ;


  /* glib logging routines are not thread safe so must lock here. */
  g_mutex_lock(log->log_lock) ;


  zMapAssert(log->logging) ;				    /* logging must be on.... */

  if ((g_io_channel_write_chars(log->active_handler.logfile, message, -1, &bytes_written, &g_error)
       != G_IO_STATUS_NORMAL)
      || (g_io_channel_flush(log->active_handler.logfile, &g_error) != G_IO_STATUS_NORMAL))
    {
      g_log_remove_handler(ZMAPLOG_DOMAIN, log->active_handler.cb_id) ;
      log->active_handler.cb_id = 0 ;

      zMapLogCritical("Unable to log to file %s, logging to this file has been turned off.",
		      log->active_handler.log_path) ;
    }


  /* now unlock. */
  g_mutex_unlock(log->log_lock) ;

  return ;
}


static void glibLogger(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message,
		       gpointer user_data)
{
  log_G->active_handler.log_cb(log_domain,log_level,message,user_data);
  if (log_G->echo_glib)
    printf("**** %s ****\n",message);

  if (log_level <= G_LOG_LEVEL_WARNING)
    zMapLogStack();   /* we really do want to know where these come from */

  return ;
}

static gboolean backtrace2fd(unsigned int remove, int fd)
{
#ifdef HAVE_GNUC_BACKTRACE
  void *stack[ZMAPLOG_MAX_TRACE_SIZE];
  size_t size, first = 0;
#endif /* HAVE_GNUC_BACKTRACE */
  gboolean traced = FALSE;

  /* zero on most machines... */
  zMapAssert(fd != STDIN_FILENO);

#ifdef HAVE_GNUC_BACKTRACE
  traced = TRUE;
  size   = backtrace(stack, ZMAPLOG_MAX_TRACE_SIZE);
  /* remove requested number of members of stack */
  if (size > remove)
    {
      size  -= remove;
      first += remove;
    }
  /* remove this function from the stack. */
  if (size > 1)
    {
      size--;
      first++;
    }
  backtrace_symbols_fd(&stack[first], size, fd);
#endif /* HAVE_GNUC_BACKTRACE */

  return traced;
}

#if FOO_LOG
static void logTime(int what, int how)
{
  int stuff[] = { TIMER_EXPOSE, TIMER_UPDATE, TIMER_DRAW };

  /* indexed by what */
  /* foo canvas does not have our defines */
  if (what >= 0 && what < 3)
    {
      what = stuff[what];
      zMapLogTime(what,how,0,"");
    }

  return ;
}

#endif
