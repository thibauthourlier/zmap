/*  File: zmapUtilsUsers.c
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
 * Description: Contains routines for various handling of users.
 *
 * Exported functions: See zmapUtils.h
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>

#include <string.h>
#include <sys/utsname.h>
#include <netdb.h>

#include <zmapUtils_P.hpp>



/* This is all kept as static because after all there is just one user at a time.... */

/* If true then various extra menu options and functions are turned on. */
static gboolean developer_status_G = FALSE ;


/* Currently developers are limited to certain ids in certain domains. */
static const char *developers_G[] = {"edgrif", "zmap", "jh13", "gb10", "sm23", NULL} ;      /* MH17: i need to see what the users get */
static const char *domain_G[] = {"local", "localhost", "sanger.ac.uk", NULL} ;


/* GLib 2.16 has the GChecksum package which we could use to encrypt this, otherwise we need
 * to include another library... */
static const char *passwd_G = "rubbish" ;





/*
 *               External Interface functions
 */



/*!
 * Initialises user package with current user/host/domain name information.
 * Must be called before other zMapUserXXX() functions otherwise
 * their results are undefined.
 *
 * @param   void  None.
 * @return        void
 *  */
void zMapUtilsUserInit(void)
{
  char *user_name ;
  const char *host_name, *domain_name ;
  struct hostent *domain_data ;
  int result ;
  struct utsname name ;
  gboolean status = FALSE ;
  gboolean name_status, domain_status;

  result = uname(&name) ;

  /* We assume that these cannot fail, not unreasonable. */
  user_name = (char *)g_get_user_name() ;

  host_name = (char *)g_get_host_name() ;
  if ((domain_data = gethostbyname(host_name)))
    {
      domain_name = domain_data->h_name ;
    }
  else
    {
      /* I don't think this works; is the log set up yet?  jgrg */
      // zMapLogWarning("Cannot find hostname from \"%s\".", host_name) ;
      domain_name = "localhost" ;
    }

  name_status = domain_status = FALSE;

  /* Is the current user a developer ? */
  if (result == 0)
    {
      const char **curr_user ;

      curr_user = &developers_G[0] ;
      while (curr_user && *curr_user)
	{
	  if (strcmp(user_name, *curr_user) == 0)
	    {
	      name_status = TRUE ;

	      break ;
	    }

	  curr_user++ ;
	}
    }

  /* Is this a supported domain ? */
  if (name_status)
    {
      const char **curr_domain ;

      curr_domain = &domain_G[0] ;
      while (curr_domain && *curr_domain)
	{
	  if (g_str_has_suffix(domain_name, *curr_domain))
	    {
	      domain_status = TRUE ;

	      break ;
	    }

	  curr_domain++ ;
	}
    }

  /* Currently developers are limited to certain ids in certain domains. */
  if (name_status && domain_status)
    status = TRUE ;

  developer_status_G = status ;
  if(!zmap_development_G)
      zmap_development_G = status;

  return ;
}


/*!
 * Tests whether current user has developer status.
 *
 * @param   void
 * @return  gboolean  TRUE if user has developer status.
 *  */
gboolean zMapUtilsUserIsDeveloper(void)
{
  return developer_status_G ;
}


/*!
 * Sets current user to have developer status for this process.
 * Fails if password is not correct.
 *
 * @param   void
 * @return  gboolean  TRUE if developer status set, FALSE otherwise.
 *  */
gboolean zMapUtilsUserSetDeveloper(char *passwd)
{
  gboolean result = FALSE ;

  if (strcmp(passwd, passwd_G) == 0)
    result = developer_status_G = TRUE ;

  return result ;
}





/*! @} end of zmaputils docs. */








/*
 *               Internal functions
 */

