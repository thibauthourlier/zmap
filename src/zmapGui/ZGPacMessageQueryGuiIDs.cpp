/*  File: ZGPacMessageQueryGuiIDs.cpp
 *  Author: Steve Miller (sm23@sanger.ac.uk)
 *  Copyright (c) 2006-2016: Genome Research Ltd.
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
 * This file is part of the ZMap genome database user interface
 * and was written by
 *
 * Steve Miller (sm23@sanger.ac.uk)
 *
 * Description:
 *-------------------------------------------------------------------
 */

#include "ZGPacMessageQueryGuiIDs.h"
#include <new>

namespace ZMap
{

namespace GUI
{

template <>
const std::string Util::ClassName<ZGPacMessageQueryGuiIDs>::m_name("ZGPacMessageQueryGuiIDs") ;
const ZGPacMessageType ZGPacMessageQueryGuiIDs::m_specific_type (ZGPacMessageType::QueryGuiIDs) ;

ZGPacMessageQueryGuiIDs::ZGPacMessageQueryGuiIDs()
    : ZGPacMessage(m_specific_type),
       Util::InstanceCounter<ZGPacMessageQueryGuiIDs>(),
       Util::ClassName<ZGPacMessageQueryGuiIDs>()
{
}

ZGPacMessageQueryGuiIDs::ZGPacMessageQueryGuiIDs(ZInternalID message_id)
    : ZGPacMessage(m_specific_type, message_id),
        Util::InstanceCounter<ZGPacMessageQueryGuiIDs>(),
      Util::ClassName<ZGPacMessageQueryGuiIDs>()
{
}

ZGPacMessageQueryGuiIDs::ZGPacMessageQueryGuiIDs(const ZGPacMessageQueryGuiIDs& message)
    : ZGPacMessage(message),
         Util::InstanceCounter<ZGPacMessageQueryGuiIDs>(),
         Util::ClassName<ZGPacMessageQueryGuiIDs>()
{
}

ZGPacMessageQueryGuiIDs& ZGPacMessageQueryGuiIDs::operator=(const ZGPacMessageQueryGuiIDs& message)
{
    if (this != &message)
    {
        ZGPacMessage::operator=(message) ;
    }
    return *this ;
}

ZGPacMessage* ZGPacMessageQueryGuiIDs::clone() const
{
    return new (std::nothrow) ZGPacMessageQueryGuiIDs(*this ) ;
}

ZGPacMessageQueryGuiIDs::~ZGPacMessageQueryGuiIDs()
{
}


} // namespace GUI

} // namespace ZMap

