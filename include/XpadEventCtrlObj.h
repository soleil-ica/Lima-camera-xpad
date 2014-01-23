//###########################################################################
// This file is part of LImA, a Library for Image Acquisition
//
// Copyright (C) : 2009-2011
// European Synchrotron Radiation Facility
// BP 220, Grenoble 38043
// FRANCE
//
// This is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//###########################################################################
#ifndef XPADEVENTCTRLOBJ_H
#define XPADEVENTCTRLOBJ_H

#include "XpadCamera.h"
#include "HwEventCtrlObj.h"

namespace lima
{
    namespace Xpad
    {
        //*******************************************************************
        // \class EventCtrlObj
        // \brief Control object providing Xpad detector event interface
        //*******************************************************************
        class EventCtrlObj : public HwEventCtrlObj
        {
            DEB_CLASS_NAMESPC(DebModCamera, "EventCtrlObj","Xpad");

        public:
            EventCtrlObj();
            virtual ~EventCtrlObj();

        };
    } // namespace Xpad
} // namespace lima

#endif // XPADEVENTCTRLOBJ_H
