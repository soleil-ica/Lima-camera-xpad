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

//- Yat Task
#include <yat/threading/Task.h>
#define kLO_WATER_MARK      128
#define kHI_WATER_MARK      512
#define kPOST_MSG_TMO       2
const size_t  XPAD_DLL_START_SYNC_MSG		=	(yat::FIRST_USER_MSG + 100);
const size_t  XPAD_DLL_START_ASYNC_MSG		=	(yat::FIRST_USER_MSG + 101);
const size_t  XPAD_DLL_START_LIVE_ACQ_MSG	=	(yat::FIRST_USER_MSG + 102);
const size_t  XPAD_DLL_GET_ASYNC_IMAGES_MSG	=	(yat::FIRST_USER_MSG + 103);
const size_t  XPAD_DLL_CALIBRATE_OTN_SLOW   =	(yat::FIRST_USER_MSG + 104);
const size_t  XPAD_DLL_CALIBRATE_OTN_MEDIUM =	(yat::FIRST_USER_MSG + 105);
const size_t  XPAD_DLL_CALIBRATE_OTN_FAST   =	(yat::FIRST_USER_MSG + 106);
const size_t  XPAD_DLL_CALIBRATE_BEAM       =	(yat::FIRST_USER_MSG + 107);
const size_t  XPAD_DLL_CALIBRATE_OTN        =	(yat::FIRST_USER_MSG + 108);
const size_t  XPAD_DLL_UPLOAD_CALIBRATION   =	(yat::FIRST_USER_MSG + 109);


//- Xpix Xpad
#include <xpci_interface.h>
#include <xpci_interface_expert.h>
#include <xpci_time.h>
#include <xpci_calib_imxpad.h>
#include <xpci_imxpad.h>

//- std
#include <stdlib.h>
#include <limits>

//- Lima
#include "lima/HwMaxImageSizeCallback.h"
#include "lima/HwBufferMgr.h"
#include "lima/Event.h"

//- Tools / Defs / Consts
#define SET(var, bit) ( var|=  (1 << bit)  )       /* positionne le bit numero 'bit' a 1 dans une variable*/
#define CLR(var, bit) ( var&= ~(1 << bit)  )       /* positionne le bit numero 'bit' a 0 dans une variable*/
#define GET(var, bit) ((var&   (1 << bit))?1:0 )   /* retourne la valeur du bit numero 'bit' dans une variable*/
const int FIRST_TIMEOUT = 8000;
#define XPIX_NOT_USED_YET 0
#define XPIX_V1_COMPATIBILITY 0

const int CHIP_NB_ROW       = 120;
const int CHIP_NB_COLUMN    = 80;

//- image sizes used for corrections 
const int I1_ROW    = 240;
const int I1_COLUMN = 560;

const int I2_ROW    = 240;
const int I2_COLUMN = 578;

const int S140_CORRECTED_NB_ROW		= 243;
const int S140_CORRECTED_NB_COLUMN  = 578;

const int S540_CORRECTED_NB_ROW		= 1157;  //- hope this will no more change
const int S540_CORRECTED_NB_COLUMN  = 582; //- hope this will no more change


namespace lima
{
namespace Xpad
{
	
	/*******************************************************************
	* \class Camera
	* \brief object controlling the xpad detector via xpix driver
	*******************************************************************/
	class Camera : public yat::Task , public EventCallbackGen, public HwMaxImageSizeCallbackGen
	{
		DEB_CLASS_NAMESPC(DebModCamera, "Camera", "Xpad");

	public:

		enum Status {
			        Ready,
                    Exposure,
                    Readout,
                    Fault,
                    Calibrating
		};

        enum XpadAcqType {
	                SYNC = 0,
	                ASYNC
		};

        //- CTOR/DTOR
        Camera(std::string xpad_type);
		~Camera();

        //- Starting/Stopping
        void prepare();
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
		void setLatTime(double  lat_time);
		void getLatTime(double& lat_time);
		
		//- Status
		void getStatus(Camera::Status& status);
	
		//---------------------------------------------------------------
		//- XPAD Stuff
		//! Set all the config G
        void setAllConfigG(const std::vector<long>& allConfigG);
		//!	Set the Acquisition type between synchrone and asynchrone
		void setAcquisitionType(short acq_type);
		//!	Load of flat config of value: flat_value (on each pixel)
		void loadFlatConfig(unsigned flat_value);
		//! Load all the config G 
		void loadAllConfigG(unsigned long modNum, unsigned long chipId , unsigned long* config_values);
		//! Load a wanted config G with a wanted value
		void loadConfigG(unsigned long modNum, unsigned long chipId , unsigned long reg_id, unsigned long reg_value);
		//! Load a known value to the pixel counters
		void loadAutoTest(unsigned long known_value);
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
        void calibrateOTNSlow (const std::string& path);
        //! Calibrate over the noise Medium and save dacl and configg files in path
        void calibrateOTNMedium (const std::string& path);
        //! Calibrate over the noise Fast and save dacl and configg files in path
        void calibrateOTNFast (const std::string& path);
		//! Calibrate BEAM and save dacl and configg files in path
		void calibrateBeam ( const std::string& path, unsigned int texp, unsigned int ithl_max, unsigned int itune,unsigned int imfp);
		//! Calibrate over the noise and save dacl and configg files in path
		void calibrateOTN ( const std::string& path, unsigned int itune,unsigned int imfp);

        //! upload the calibration (dacl + config) that is stored in path
        void uploadCalibration(const std::string& path);
        //! upload the wait times between each images in case of a sequence of images (Twait from setExposureParameters should be 0)
        void uploadExpWaitTimes(unsigned long *pWaitTime, unsigned size);
        //! increment the ITHL
        void incrementITHL();
        //! decrement the ITHL
        void decrementITHL();
        //! Set the Calibration Adjusting number of iteration
        void setCalibrationAdjustingNumber(unsigned calibration_adjusting_number);
		//! Set the Minimum latency time in ms
        void setMinLatencyTimeMs(double min_latency_time_ms);

		//! Set the init time
		void setInitTime(unsigned int init_time_ms);
		//! Set the shutter time
		void setShutterTime(unsigned int shutter_time_ms);
		//! Set the overflow time
		void setOverflowTime(unsigned int overflow_time);
		//! Set the n param
		void setNParameter(unsigned int n);
		//! Set the p param
		void setPParameter(unsigned int p);
		//! Set the busy out selection
		void setBusyOutSel(unsigned int busy_out_sel);
		//! enable/disable geom correction
		void setGeomCorrection(bool geom_corr);
		//! enable/disable double pixel correction
		void setDoublePixelCorrection(bool doublepixel_corr);
		//! Set Normalization Factor (used in double pixel correction)
		void setNormalizationFactor(double norm_factor);
		//! Set GeneralPurpose Params
		void setGeneralPurposeParams( unsigned int GP1, unsigned intGP2, unsigned int GP3, unsigned int GP4);

		//! Xpix debug
        void xpixDebug(bool enable);

	protected: 
		//- yat::Task implementation
		virtual void handle_message( yat::Message& msg )throw (yat::Exception);
		virtual void setMaxImageSizeCallbackActive(bool cb_active);
		

	private:
		//- lima stuff
		SoftBufferAllocMgr 	m_buffer_alloc_mgr;
		StdBufferCbMgr 		m_buffer_cb_mgr;
		BufferCtrlMgr 		m_buffer_ctrl_mgr;
		bool				m_maximage_size_cb_active;
        Camera::Status		m_status;
		bool				m_live_mode;
		int					m_nb_live_frames;

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
        Timestamp       m_start_sec,m_end_sec;


		//---------------------------------
		//- xpad stuff 
        Camera::XpadAcqType		m_acquisition_type;
        unsigned int	        m_modules_mask;
        int				        m_module_number;
        unsigned int	        m_chip_number;
        std::vector<long>	    m_all_config_g;
        unsigned short 			m_xpad_model;
        std::string             m_calibration_path;
        unsigned int            m_calibration_adjusting_number;
		double					m_min_latency_time_ms;
        void**                  m_image_array;
		bool					m_doublepixel_corr;
		unsigned int			m_calib_texp;
		unsigned int			m_calib_ithl_max;
		unsigned int			m_calib_itune;
		unsigned int			m_calib_imfp;
		double					m_norm_factor;
        //unsigned short*         m_dacl;
        unsigned int m_time_between_images_usec; //- Temps entre chaque image
        unsigned int m_time_before_start_usec;     //- Temps initial
        unsigned int m_shutter_time_usec;
	    unsigned int m_ovf_refresh_time_usec;
        unsigned int m_specific_param_n;
        unsigned int m_specific_param_p;
        unsigned int m_busy_out_sel;
        unsigned int m_geom_corr;
        unsigned int m_specific_param_GP1;
	    unsigned int m_specific_param_GP2;
	    unsigned int m_specific_param_GP3;
	    unsigned int m_specific_param_GP4;

		//- Internal algos
		template<typename T> 
		void doublePixelCorrection(T* image_to_correct,  T corrected_image[][S140_CORRECTED_NB_COLUMN]);

	};

} // namespace xpad
} // namespace lima


#endif // XPADCAMERA_H
