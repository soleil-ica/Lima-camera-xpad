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
#include "XpadInterface.h"
#include <algorithm>

using namespace lima;
using namespace lima::Xpad;

/*******************************************************************
 * \brief Hw Interface constructor
 *******************************************************************/
Interface::Interface(Camera& cam)
	: m_cam(cam),m_det_info(cam), m_buffer(cam),m_sync(cam)
{
	DEB_CONSTRUCTOR();

	HwDetInfoCtrlObj *det_info = &m_det_info;
	m_cap_list.push_back(HwCap(det_info));

	HwBufferCtrlObj *buffer = &m_buffer;
	m_cap_list.push_back(HwCap(buffer));
	
	HwSyncCtrlObj *sync = &m_sync;
	m_cap_list.push_back(HwCap(sync));

    HwEventCtrlObj *my_event = &m_event;
	m_cap_list.push_back(HwCap(my_event));
}

//-----------------------------------------------------
//
//-----------------------------------------------------
Interface::~Interface()
{
	DEB_DESTRUCTOR();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::getCapList(HwInterface::CapList &cap_list) const
{
	DEB_MEMBER_FUNCT();
	cap_list = m_cap_list;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::reset(ResetLevel reset_level)
{
	DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(reset_level);

	stopAcq();

	Size image_size;
	m_det_info.getMaxImageSize(image_size);
	ImageType image_type;
	m_det_info.getDefImageType(image_type);
	FrameDim frame_dim(image_size, image_type);
	m_buffer.setFrameDim(frame_dim);

	m_buffer.setNbConcatFrames(1);
	m_buffer.setNbBuffers(1);
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::prepareAcq()
{
	DEB_MEMBER_FUNCT();
    m_cam.prepare();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::startAcq()
{
	DEB_MEMBER_FUNCT();
	m_cam.start();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::stopAcq()
{
	DEB_MEMBER_FUNCT();
	m_cam.stop();
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Interface::getStatus(StatusType& status)
{
	Camera::Status xpad_status = Camera::Ready;
	m_cam.getStatus(xpad_status);
	switch (xpad_status)
	{
		case Camera::Ready:
			status.det = DetIdle;
            status.acq = AcqReady;
			break;
		case Camera::Exposure:
			status.det = DetExposure;
			status.acq = AcqRunning;
			break;
		case Camera::Readout:
			status.det = DetReadout;
			status.acq = AcqRunning;
			break;
		case Camera::Fault:
		  	status.det = DetFault;
		  	status.acq = AcqFault;
			break;
        case Camera::Calibrating:
		  	status.det = DetExposure;
		  	status.acq = AcqConfig;
			break;
	}
	status.det_mask = DetExposure | DetReadout ;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
int Interface::getNbHwAcquiredFrames()
{
	DEB_MEMBER_FUNCT();
	int nb_acq_frames;
	nb_acq_frames = m_cam.getNbHwAcquiredFrames();
	return nb_acq_frames;
}


