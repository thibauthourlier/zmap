/*  File: zmapRemoteCommand.h
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2010-2014: Genome Research Ltd.
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
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *      Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *
 * Description: Defines interface for processing remote control 
 *              commands.
 *              These functions handle the sending/receiving of messages
 *              and check that requests/responses have matching ids,
 *              are the right protocol version.
 *
 * Created: Thu Dec 15 17:39:32 2011 (edgrif)
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_REMOTE_COMMAND_H
#define ZMAP_REMOTE_COMMAND_H

#include <glib.h>

#include <ZMap/zmapEnum.h>
#include <ZMap/zmapXML.h>
#include <ZMap/zmapRemoteControl.h>



/* Using the XXX_LIST macros to declare enums means we get lots of enum functions for free (see zmapEnum.h). */


/* Return code values from command processing function. */
#define REMOTE_COMMAND_RC_LIST(_)                                                           \
_(REMOTE_COMMAND_RC_OK,            , "ok",             "Command succeeded.",            "") \
_(REMOTE_COMMAND_RC_FAILED,        , "cmd_failed",     "Command failed.",               "") \
_(REMOTE_COMMAND_RC_BAD_ARGS,      , "bad_args",       "Command args are wrong.",       "") \
_(REMOTE_COMMAND_RC_VIEW_UNKNOWN,  , "view_unknown",   "Command target view id is unknown.", "") \
_(REMOTE_COMMAND_RC_CMD_UNKNOWN,   , "cmd_unknown",    "Command is unknown.",           "") \
_(REMOTE_COMMAND_RC_BAD_XML,       , "bad_xml",        "Command XML is malformed.",     "")

ZMAP_DEFINE_ENUM(RemoteCommandRCType, REMOTE_COMMAND_RC_LIST) ;


/* Return codes for message validation. */
#define REMOTE_VALIDATE_RC_LIST(_)                                                                     \
_(REMOTE_VALIDATE_RC_OK,               , "ok",               "Validation succeeded.",              "") \
_(REMOTE_VALIDATE_RC_BODY_COMMAND,     , "body_command",     "Error in message body command.",     "") \
_(REMOTE_VALIDATE_RC_BODY_CONTENT,     , "body_content",     "Error in message body content.",     "") \
_(REMOTE_VALIDATE_RC_BODY_XML,         , "body_xml",         "Error in message body xml.",         "") \
_(REMOTE_VALIDATE_RC_ENVELOPE_CONTENT, , "envelope_content", "Error in message envelope content.", "") \
_(REMOTE_VALIDATE_RC_ENVELOPE_XML,     , "envelope_xml",     "Error in message envelope xml.",     "")

ZMAP_DEFINE_ENUM(RemoteValidateRCType, REMOTE_VALIDATE_RC_LIST) ;





GArray *zMapRemoteCommandCreateRequest(ZMapRemoteControl remote_control,
                                       char *command, int timeout_secs, ...) ;
GArray *zMapRemoteCommandCreateReplyFromRequest(ZMapRemoteControl remote_control,
                                                char *original_request,
                                                RemoteCommandRCType return_code, char *reason,
                                                ZMapXMLUtilsEventStack reply,
                                                char **error_out) ;
GArray *zMapRemoteCommandCreateReplyEnvelopeFromRequest(ZMapRemoteControl remote_control,
							char *xml_request,
							RemoteCommandRCType return_code, char *reason,
							ZMapXMLUtilsEventStack reply,
							char **error_out) ;
ZMapXMLUtilsEventStack zMapRemoteCommandCreateElement(char *element, char *attribute, char *attribute_value) ;
ZMapXMLUtilsEventStack zMapRemoteCommandMessage2Element(char *message) ;
GArray *zMapRemoteCommandAddBody(GArray *request_in_out, char *req_or_reply,
				 ZMapXMLUtilsEventStack request_body) ;
char *zMapRemoteCommandStack2XML(GArray *xml_stack, char **error_out) ;

RemoteValidateRCType zMapRemoteCommandValidateEnvelope(ZMapRemoteControl remote_control,
						       char *xml_request, char **error_out) ;
RemoteValidateRCType zMapRemoteCommandValidateRequest(ZMapRemoteControl remote_control,
						      char *request, char **error_out) ;
RemoteValidateRCType zMapRemoteCommandValidateReply(ZMapRemoteControl remote_control,
						    char *original_request, char *reply, char **error_out) ;
gboolean zMapRemoteCommandRequestIsCommand(char *request, char *command) ;
char *zMapRemoteCommandRequestGetCommand(char *request) ;

const char *zMapRemoteCommandGetCurrCommand(ZMapRemoteControl remote_control) ;

gboolean zMapRemoteCommandReplyGetAttributes(char *reply,
					     char **command_out,
					     RemoteCommandRCType *return_code_out, char **reason_out,
					     char **reply_body_out,
					     char **error_out) ;
gboolean zMapRemoteCommandGetAttribute(char *message,
				       char *element, char *attribute, char **attribute_value_out,
				       char **error_out) ;

ZMAP_ENUM_AS_NAME_STRING_DEC(zMapRemoteCommandRC2Str, RemoteCommandRCType) ;
ZMAP_ENUM_FROM_STRING_DEC(zMapRemoteCommandStr2RC, RemoteCommandRCType) ;

ZMAP_ENUM_TO_SHORT_TEXT_DEC(zMapRemoteCommandRC2Desc, RemoteValidateRCType) ;

#endif /* ZMAP_REMOTE_COMMAND_H */
