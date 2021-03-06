/*  File: zmapString.c
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
 * Description: Ideally we wouldn't have this but instead use an
 *              existing library but that's not always so easy.
 *
 * Exported functions: See ZMap/zmapString.h
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>

#include <ZMap/zmapUtilsDebug.hpp>
#include <ZMap/zmapString.hpp>
#include <string.h>





static int findMatch(char *target, char *query, gboolean caseSensitive) ;






//
//             External functions
//

// Code to do this is repeated in a number of places in our code....this should be the one place.
//
// HOMEWORK: implement using the C++ String class....
const char *zMapStringAbbrevStr(const char *orig_str,
                                const char *optional_abbrev_chars, const unsigned int optional_max_orig_len)
{
  const char *abbrev_str ;
  const char *abbrev_chars = optional_abbrev_chars ;
  unsigned int max_len = optional_max_orig_len ;
  
  zMapReturnValIfFail((orig_str && *orig_str), NULL) ;
  zMapReturnValIfFail((optional_abbrev_chars && *optional_abbrev_chars), NULL) ;
  zMapReturnValIfFail((optional_max_orig_len), NULL) ;
  

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if (optional_abbrev_chars)
    abbrev_chars = optional_abbrev_chars ;
  
  if (optional_max_orig_len)
    max_len = optional_max_orig_len ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

  if (strlen(orig_str) <= max_len)
    {
      abbrev_str = NULL ;
    }
  else
    {
      GString *tmp ;

      tmp = g_string_new_len(orig_str, max_len) ;

      tmp = g_string_overwrite(tmp, (max_len - strlen(abbrev_chars)), abbrev_chars) ;

      abbrev_str = g_string_free(tmp, FALSE) ;
    }

  return abbrev_str ;
}






/* This code was taken from acedb (www.acedb.org)

 match to template with wildcards.   Authorized wildchars are * ? #
     ? represents any single char
     * represents any set of chars
   case sensitive.   Example: *Nc*DE# fits abcaNchjDE23

   returns 0 if not found
           1 + pos of first sigificant match (i.e. not a *) if found
*/
int zMapStringFindMatch(char *target, char *query)
{
  int result ;

  result = findMatch(target, query, FALSE) ;

  return result ;
}

int zMapStringFindMatchCase(char *target, char *query, gboolean caseSensitive)
{
  int result ;

  result = findMatch(target, query, caseSensitive) ;

  return result ;
}


/* Is the string empty or white space ?....surely this should be a standard function ?? */
gboolean zMapStringBlank(char *string_arg)
{
  gboolean result = TRUE ;
  char *curr = string_arg ;

  while (*curr)
    {
      if (!g_ascii_isspace(*curr))
        {
          result = FALSE ;
          break ;
        }
      else
        {
          curr++ ;
        }
    }

  return result ;
}


/* Takes a string and translates all multiple whitespace into single
 * whitespaces. This is done in place so the resulting string will
 * be shorter but still uses the same amount of memory.
 *
 * Note: although the operation is done in place the function returns
 * the string so it can be used in expressions.
 */
char *zMapStringCompress(char *txt)
{
  char *compressed_txt = txt ;

  if (txt && *txt)
    {
      char *curr ;
      int len ;

      /* Use glib function to strip leading and trailing space. */
      compressed_txt = g_strstrip(compressed_txt) ;


      /* Remove internal multiple spaces by finding them and then moving
       * the remaining string to overlay them. */
      len = strlen(compressed_txt) + 1 ;    /* + 1 to make sure our memcpy's
       include the terminating NULL. */
      curr = compressed_txt ;
      while (*curr)
        {
          if (!g_ascii_isspace(*curr))
            {
              curr++ ;
              len-- ;
            }
          else
            {
              char *curr_space, *next_char ;

              next_char = curr_space = curr ;

              /* chug through the space. */
              while (g_ascii_isspace(*next_char))
                {
                  next_char++ ;
                  len-- ;
                }

              /* Only do memcpy if there are multiple spaces. */
              if (next_char > (curr_space + 1))
                {
                  memcpy((curr_space + 1), next_char, len) ;
                }

              /* bump us on to char after this space. */
              curr = (curr_space + 1) ;
            }
        }
    }

  return compressed_txt ;
}





/* Takes a string and removes all spaces and control chars. This is
 * done in place so the resulting string will be shorter but still
 * uses the same amount of memory.
 *
 */
char *zMapStringFlatten(char *query_txt)
{
  char *query_ptr;
  int len ;
  int i, n ;

  len = strlen(query_txt) ;

  /* Use glib function to strip leading and trailing space. */
  query_ptr = query_txt = g_strstrip(query_txt);

  /* step through the string replacing spaces with the next non-space
   * character. */

  for(i = 0, n = 0 ; i < len ; i++, n++, query_ptr++)
    {
      if (g_ascii_isspace(*query_ptr) || g_ascii_iscntrl(*query_ptr))
        n++ ;

      /* Is the replacement a non space? */
      if (n < len && !(g_ascii_isspace(query_txt[n]) || g_ascii_iscntrl(query_txt[n])))
        {
          *query_ptr = query_txt[n];

          /* should we set the intervening chars to space? */
          if (query_ptr != &query_txt[n])
            {
              char *tmp = query_ptr;

              query_ptr++;

              while(query_ptr < &query_txt[n])
                {
                  *query_ptr = ' ';
                  query_ptr++;
                }

              query_ptr = tmp;
              query_txt[n] = ' ';
              n--;
            }
        }
      else if (n >= len)/* reached the end? */
        {
          int j ;

          for (j = i; j < len; j++)
            {
              query_txt[j] = ' ';
            }

          break;
        }
      else if (i != 0)
        {
          /* Keep the pointer back at the last non-space */
          query_ptr--;
          n--;
        }
    }

  /* Use glib function to strip off trailing space... */
  query_txt = g_strstrip(query_txt);

  return query_txt;
}


/*
 *                Internal functions.
 */



static int findMatch(char *target, char *query, gboolean caseSensitive)
{
  int result = 0 ;
  char *c=target, *t=query;
  char *ts = 0, *cs = 0, *s = 0 ;
  int star=0;

  while (TRUE)
    {
      switch(*t)
        {
        case '\0':
          if (!*c)
            return  ( s ? 1 + (s - target) : 1) ;

          if (!star)
            return 0 ;

          /* else not success yet go back in template */
          t=ts; c=cs+1;
          if(ts == query) s = 0 ;
          break ;

        case '?':
          if (!*c)
            return 0 ;
          if(!s) s = c ;
          t++ ;  c++ ;
          break;

        case '*':
          ts=t;
          while( *t == '?' || *t == '*')
            t++;

          if (!*t)
            return s ? 1 + (s-target) : 1 ;

          if (!caseSensitive)
            {

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
              while (g_ascii_toupper(*c) != g_ascii_toupper(*t))
                {
                  if(*c)
                    c++;
                  else
                    return 0 ;
                }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

              while (g_ascii_toupper(*c) != g_ascii_toupper(*t))
                {
                  c++;

                  if (!(*c))
                    return 0 ;
                }
            }
          else
            {
              while (*c != *t)
                {
                  if(*c)
                    c++;
                  else
                    return 0 ;
                }
            }

          star=1;
          cs=c;
          if(!s) s = c ;
          break;

        default:
          if (!caseSensitive)
            {
              if (g_ascii_toupper(*t++) != g_ascii_toupper(*c++))
                {
                  if(!star)
                    return 0 ;

                  t=ts; c=cs+1;

                  if(ts == query)
                    s = 0 ;
                }
              else
                {
                  if(!s)
                    s = c - 1 ;
                }
            }
          else
            {
              if (*t++ != *c++)
                {
                  if(!star)
                    return 0 ;

                  t=ts; c=cs+1;
                  if(ts == query)
                    s = 0 ;
                }
              else
                {
                  if(!s) s = c - 1 ;
                }
            }
          break;
        }
    }

  return result ;
}

