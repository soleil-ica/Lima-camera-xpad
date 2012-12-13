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
	* \class Camera
	* \brief object controlling the xpad detector via xpix driver
	*******************************************************************/
	class Camera : public yat::Task
	{
		DEB_CLASS_NAMESPC(DebModCamera, "Camera", "Xpad");

	public:

		enum Status {
			Ready, Exposure, Readout, Fault, Calibrating
		};

        enum XpadAcqType {
	                SYNC = 0,
	                ASYNC
		};

        enum CalibrationType {
	                OTN_SLOW = 0,
	                OTN_MEDIUM,
			OTN_HIGH,
			BEAM,
			UPLOAD
		};

        //- CTOR/DTOR
		Camera(string xpad_type);
		~Camera();

        //- Starting/Stopping
        void prepare(){} //- nothing yet: will be done later
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
        int getNbHwAcquiredFrames();

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
        //! Calibrate over the noise Slow and save dacl and configg files in path
        void calibrateOTNSlow (string path);
        //! Calibrate over the noise Medium and save dacl and configg files in path
        void calibrateOTNMedium (string path);
        //! Calibrate over the noise High and save dacl and configg files in path
        void calibrateOTNHigh (string path);
        //! upload the calibration (dacl + config) that is stored in path
        void uploadCalibration(string path);
        //! upload the wait times between each images in case of a sequence of images (Twait from setExposureParameters should be 0)
        void uploadExpWaitTimes(unsigned long *pWaitTime, unsigned size);
        //! increment the ITHL
        void incrementITHL();
        //! decrement the ITHL
        void decrementITHL();
        //! set the specific parameters (deadTime,init time, shutter ...
        void setSpecificParameters( unsigned deadtime, unsigned init,
								    unsigned shutter, unsigned ovf,
								    unsigned n,       unsigned p,
								    unsigned GP1,     unsigned GP2,    unsigned GP3,      unsigned GP4);




		//- yat::Task implementation
	protected: 
		virtual void handle_message( yat::Message& msg )throw (yat::Exception);
	private:
		//- lima stuff
		SoftBufferAllocMgr 	m_buffer_alloc_mgr;
		StdBufferCbMgr 		m_buffer_cb_mgr;
		BufferCtrlMgr 		m_buffer_ctrl_mgr;

		//- img stuff
		int 			m_nb_frames;		
        int 			m_current_nb_frames;
		Size			m_image_size;
		IMG_TYPE		m_pixel_depth;
        unsigned int    m_imxpad_format;
        unsigned int    m_imxpad_trigger_mode;
        unsigned int    m_exp_time_usec;
		int         	m_timeout_ms;
        bool            m_stop_asked;


		//---------------------------------
		//- xpad stuff 
        Camera::XpadAcqType		m_acquisition_type;
        Camera::CalibrationType m_calibration_type;
        unsigned int	        m_modules_mask;
        int				        m_module_number;
        unsigned int	        m_chip_number;
        int				        m_full_image_size_in_bytes;
        vector<long>	        m_all_config_g;
        unsigned short 			m_xpad_model;
        string                  m_calibration_path;
        //unsigned short*         m_dacl;
        //- Specific xpad stuff
        unsigned int m_time_between_images_usec; //- Temps entre chaque image
        unsigned int m_time_before_start_usec;     //- Temps initial
        unsigned int m_shutter_time_usec;
	    unsigned int m_ovf_refresh_time_usec;
        unsigned int m_specific_param_n;
        unsigned int m_specific_param_p;
        unsigned int m_specific_param_GP1;
	    unsigned int m_specific_param_GP2;
	    unsigned int m_specific_param_GP3;
	    unsigned int m_specific_param_GP4;

        //---------------------------------
        Camera::Status	m_status;
	};

} // namespace xpad
} // namespace lima


#endif // XPADCAMERA_H
