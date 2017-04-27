/*  File: ZGMessageType.h
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

#ifndef ZGMESSAGETYPE_H
#define ZGMESSAGETYPE_H

#include <cstdint>

namespace ZMap
{

namespace GUI
{

//
// Types for external control messages only.
//
enum class ZGMessageType: std::uint8_t
{
    Invalid,

    ToplevelOperation,
    GuiOperation, // now replaces OpenGui, CloseGui,
    SequenceOperation, // replaces AddSequence, DeleteSequence
    SequenceToGui, // add sequence to gui, remove sequence from gui
    TrackOperation,
    SeparatorOperation,
    FeaturesetOperation,
    FeaturesetToTrack,
    SwitchOrientation,
    SetSequenceBGColor,
    SetTrackBGColor,
    SetSeparatorColor,
    SetGuiMenuBarColor,
    SetGuiSessionColor,

    AddSeparator,
    DeleteSeparator,

    DeleteFeatureset,
    DeleteFeaturesetFromTrack,
    SetFeaturesetStyle,

    SetViewOrientation,
    SetSequenceOrder,
    SetStatusLabel,
    ScaleBarOperation,
    ScaleBarPosition,

    AddFeatures,
    DeleteFeatures,
    DeleteAllFeatures,

    SetUserHome,

    ResourceImport,

    QueryGuiIDs,
    QuerySequenceIDs,
    QuerySequenceIDsAll,
    QueryFeaturesetIDs,
    QueryTrackIDs,
    QueryFeatureIDs,
    QueryFeaturesetsFromTrack,
    QueryTrackFromFeatureset,
    QueryUserHome,

    ReplyStatus,
    ReplyDataID,
    ReplyDataIDSet,
    ReplyLogMessage,
    ReplyString,

    Quit
} ;

// might need this at some point
//#include <ostream>
//std::ostream& operator<<(std::ostream & os, ZGMessageType type) ;

} // namespace GUI

} // namespace ZMap

#endif
