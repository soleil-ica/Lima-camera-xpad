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
#ifndef XPADTASK_H
#define XPADTASK_H

///////////////////////////////////////////////////////////
// YAT::TASK 
///////////////////////////////////////////////////////////
#include <yat/threading/Task.h>

#define kLO_WATER_MARK      128
#define kHI_WATER_MARK      512

#define kPOST_MSG_TMO       2

const size_t  XPAD_DLL_START_SYNC_MSG		=	(yat::FIRST_USER_MSG + 100);
const size_t  XPAD_DLL_START_ASYNC_MSG		=	(yat::FIRST_USER_MSG + 101);
const size_t  XPAD_DLL_START_LIVE_ACQ_MSG	=	(yat::FIRST_USER_MSG + 102);
const size_t  XPAD_DLL_CALIBRATE		    =	(yat::FIRST_USER_MSG + 103);



///////////////////////////////////////////////////////////

//- Xpix
#include <xpci_interface.h>
#include <xpci_interface_expert.h>
#include <xpci_time.h>
#include <xpci_calib_imxpad.h>
#include <xpci_imxpad.h>

#include <stdlib.h>
#include <limits>

#include "HwMaxImageSizeCallback.h"
#include "HwBufferMgr.h"

using namespace std;

#define SET(var, bit) ( var|=  (1 << bit)  )       /* positionne le bit numero 'bit' a 1 dans une variable*/
#define CLR(var, bit) ( var&= ~(1 << bit)  )       /* positionne le bit numero 'bit' a 0 dans une variable*/
#define GET(var, bit) ((var&   (1 << bit))?1:0 )   /* retourne la valeur du bit numero 'bit' dans une variable*/
const int FIRST_TIMEOUT = 8000;
#define XPIX_NOT_USED_YET 0
#define XPIX_V1_COMPATIBILITY 0


namespace lima
{
namespace Xpad
{
    /*******************************************************************
	* \class XpadTask
	* \brief Task for Xpad acquisition and other stuff (calibrations ...)
	*******************************************************************/
    class XpadTask : public yat::Task
    {
        public:
            //- [ctor]
            XpadTask();
        	
            //- [dtor]
            ~XpadTask();

        protected: //- [yat::Task implementation]
            virtual void handle_message( yat::Message& msg )throw (yat::Exception);
    };

}// namespace Xpad
}// namespace lima


#endif // XPADTASK_H
