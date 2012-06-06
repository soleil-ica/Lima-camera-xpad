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
#ifndef XPADCAMERA_H
#define XPADCAMERA_H

///////////////////////////////////////////////////////////
// YAT::TASK 
///////////////////////////////////////////////////////////
#include <yat/threading/Task.h>

#define kLO_WATER_MARK      128
#define kHI_WATER_MARK      512

#define kPOST_MSG_TMO       2

const size_t  XPAD_DLL_START_SYNC_MSG		=	(yat::FIRST_USER_MSG + 100);
const size_t  XPAD_DLL_START_ASYNC_MSG		=	(yat::FIRST_USER_MSG + 101);


///////////////////////////////////////////////////////////

//- Xpix
#include <xpci_interface.h>
#include <xpci_interface_expert.h>
#include <xpci_time.h>

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
	* \class Camera
	* \brief object controlling the xpad detector via xpix driver
	*******************************************************************/
	class Camera : public HwMaxImageSizeCallbackGen, public yat::Task
	{
		DEB_CLASS_NAMESPC(DebModCamera, "Camera", "Xpad");

	public:

		enum Status {
			Ready, Exposure, Readout,Fault
		};

        enum XpadAcqType {
	                SYNC = 0,
	                ASYNC
		};

		Camera(string xpad_type);
		~Camera();

		void start();
		void stop();

		//- Det info
		void getImageSize(Size& size);
		void setPixelDepth(ImageType pixel_depth);
		void getPixelDepth(ImageType& pixel_depth);
		void getPixelSize(double& x_size,double& y_size);
		void getDetectorType(std::string& type);
		void getDetectorModel(std::string& model);

		//- Buffer
		BufferCtrlMgr& getBufferMgr();
		void setNbFrames(int  nb_frames);
		void getNbFrames(int& nb_frames);

		//- Sync 
		void setTrigMode(TrigMode  mode);
		void getTrigMode(TrigMode& mode);
		void setExpTime(double  exp_time);
		void getExpTime(double& exp_time);
		
		//- Status
		void getStatus(Camera::Status& status);
	
		//---------------------------------------------------------------
		//- XPAD Stuff
		//! Set all the config G
		void setAllConfigG(const vector<long>& allConfigG);
		//!	Set the Acquisition type between fast and slow
		void setAcquisitionType(short acq_type);
		//!	Load of flat config of value: flat_value (on each pixel)
		void loadFlatConfig(unsigned flat_value);
		//! Load all the config G 
		void loadAllConfigG(unsigned long modNum, unsigned long chipId , unsigned long* config_values);
		//! Load a wanted config G with a wanted value
		void loadConfigG(const vector<unsigned long>& reg_and_value);
		//! Load a known value to the pixel counters
		void loadAutoTest(unsigned known_value);
        //! Save the config L (DACL) to XPAD RAM
        void saveConfigL(unsigned long modMask, unsigned long calibId, unsigned long chipId, unsigned long curRow,unsigned long* values);
        //! Save the config G to XPAD RAM
        void saveConfigG(unsigned long modMask, unsigned long calibId, unsigned long reg,unsigned long* values);
	    //! Load the config to detector chips
        void loadConfig(unsigned long modMask, unsigned long calibId);
        //! Get the modules config (Local aka DACL)
        unsigned short*& getModConfig();
        //! Reset the detector
        void reset();
        //! Set the exposure parameters
        void setExposureParameters( unsigned Texp,unsigned Twait,unsigned Tinit,
			                        unsigned Tshutter,unsigned Tovf,unsigned mode, unsigned n,unsigned p,
		                            unsigned nbImages,unsigned BusyOutSel,unsigned formatIMG,unsigned postProc,
		                            unsigned GP1,unsigned GP2,unsigned GP3,unsigned GP4);

	protected:
		virtual void setMaxImageSizeCallbackActive(bool cb_active);	

		//- yat::Task implementation
	protected: 
		virtual void handle_message( yat::Message& msg )throw (yat::Exception);
	private:
		//- lima stuff
		SoftBufferAllocMgr 	m_buffer_alloc_mgr;
		StdBufferCbMgr 		m_buffer_cb_mgr;
		BufferCtrlMgr 		m_buffer_ctrl_mgr;
		bool 				m_mis_cb_act;

		//- img stuff
		int 			m_nb_frames;		
		Size			m_image_size;
		IMG_TYPE		m_pixel_depth;
        unsigned int    m_imxpad_format;
        unsigned int    m_imxpad_trigger_mode;
        unsigned int    m_exp_time_usec;
		int         	m_timeout_ms;


		//---------------------------------
		//- xpad stuff 
        Camera::XpadAcqType		m_acquisition_type;
        unsigned int	        m_modules_mask;
        int				        m_module_number;
        unsigned int	        m_chip_number;
        int				        m_full_image_size_in_bytes;
        vector<long>	        m_all_config_g;
        unsigned short 			m_xpad_model;
        //unsigned short*         m_dacl;

        //---------------------------------
        Camera::Status	m_status;
	};

} // namespace xpad
} // namespace lima


#endif // XPADCAMERA_H
