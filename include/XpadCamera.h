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

const size_t  XPAD_DLL_START_FAST_MSG		=	(yat::FIRST_USER_MSG + 100);
const size_t  XPAD_DLL_START_FAST_ASYNC_MSG	=	(yat::FIRST_USER_MSG + 101);
const size_t  XPAD_DLL_START_SLOW_B2_MSG	=	(yat::FIRST_USER_MSG + 103);
const size_t  XPAD_DLL_START_SLOW_B4_MSG	=	(yat::FIRST_USER_MSG + 104);

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

#define SET(var, bit) ( var|=  (1 << bit)  )       /* positionne le bit num�ro 'bit' � 1 dans une variable*/
#define CLR(var, bit) ( var&= ~(1 << bit)  )       /* positionne le bit num�ro 'bit' � 0 dans une variable*/
#define GET(var, bit) ((var&   (1 << bit))?1:0 )   /* retourne la valeur du bit num�ro 'bit' dans une variable*/

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
	                SLOW_16 = 0, 
                    FAST_16, 
                    SLOW_32,
                    FAST_ASYNC_16
		};

		Camera();
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
		//! Set the F parameters
		void setFParameters(unsigned deadtime, unsigned init,
			unsigned shutter, unsigned ovf,    unsigned mode,
			unsigned n,       unsigned p,
			unsigned GP1,     unsigned GP2,    unsigned GP3,      unsigned GP4);
		//!	Set the Acquisition type between fast and slow
		void setAcquisitionType(short acq_type);
		//!	Load of flat config of value: flat_value (on each pixel)
		void loadFlatConfig(unsigned flat_value);
		//! Load all the config G with predefined values (on each chip)
		void loadAllConfigG();
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
		unsigned short	m_trigger_type;
		unsigned int	m_exp_time;
        double      	m_exp_time_sec;

		uint16_t**	pSeqImage;
		uint16_t*	pOneImage;

		//---------------------------------
		//- xpad stuff 
        Camera::XpadAcqType		m_acquisition_type;
        unsigned int	        m_modules_mask;
        int				        m_module_number;
        unsigned int	        m_chip_number;
        int				        m_full_image_size_in_bytes;
        unsigned int	        m_time_unit;
        vector<long>	        m_all_config_g;
        unsigned short*         m_dacl;

        //- FParameters
        unsigned		m_fparameter_deadtime;
        unsigned		m_fparameter_init;
        unsigned		m_fparameter_shutter;
        unsigned		m_fparameter_ovf;
        unsigned		m_fparameter_mode;
        unsigned		m_fparameter_n;
        unsigned		m_fparameter_p;
        unsigned		m_fparameter_GP1;
        unsigned		m_fparameter_GP2;
        unsigned		m_fparameter_GP3;
        unsigned		m_fparameter_GP4;

        //---------------------------------
        Camera::Status	m_status;
        bool			m_stop_asked;
	};

} // namespace xpad
} // namespace lima


#endif // XPADCAMERA_H
