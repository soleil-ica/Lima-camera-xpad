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
#include "XpadCamera.h"
#include <sstream>
#include <iostream>
#include <string>
#include <math.h>

using namespace lima;
using namespace lima::Xpad;
using namespace std;

//---------------------------
//- Ctor
//---------------------------
Camera::Camera(string xpad_model) : 
m_buffer_cb_mgr(m_buffer_alloc_mgr),
m_buffer_ctrl_mgr(m_buffer_cb_mgr),
m_maximage_size_cb_active(false)
{
	DEB_CONSTRUCTOR();

	//- default values:
    m_modules_mask      = 0x00;
    m_chip_number       = 7; 
    m_pixel_depth       = B2; //- 16 bits
    m_nb_frames         = 1;

    m_status            = Camera::Ready;
    m_acquisition_type	= Camera::SYNC;
    m_current_nb_frames = 0;

    m_time_between_images_usec		= 5000;
    m_ovf_refresh_time_usec			= 4000;
    m_time_before_start_usec		= 0;
    m_shutter_time_usec             = 0;
    m_busy_out_sel                  = 0;
    m_calibration_adjusting_number  = 1;
    m_geom_corr                     = 0;
	m_specific_param_GP1			= 0;
	m_specific_param_GP2			= 0;
	m_specific_param_GP3			= 0;
	m_specific_param_GP4			= 0;
	
	m_doublepixel_corr				= false;
	m_norm_factor					= 2.5;

    if		(xpad_model == "BACKPLANE") 	m_xpad_model = BACKPLANE;
	else if	(xpad_model == "HUB")	        m_xpad_model = HUB;
    else if	(xpad_model == "IMXPAD_S70")	m_xpad_model = IMXPAD_S70;
    else if	(xpad_model == "IMXPAD_S140")	m_xpad_model = IMXPAD_S140;
    else if	(xpad_model == "IMXPAD_S340")	m_xpad_model = IMXPAD_S340;
	else if	(xpad_model == "IMXPAD_S420")	m_xpad_model = IMXPAD_S420;
    else if	(xpad_model == "IMXPAD_S540")	m_xpad_model = IMXPAD_S540;
    else
    	throw LIMA_HW_EXC(Error, "Xpad Model not supported");

    //-------------------------------------------------------------
    //- Init the xpix driver
    if(xpci_init(0,m_xpad_model) != 0)
    {
        DEB_TRACE() << "PCIe board UNsuccessfully initialized";
        throw LIMA_HW_EXC(Error, "PCIe board UNsuccessfully initialized");
    }
    else
    {
        DEB_TRACE() << "PCIe board successfully initialized";
	    //- Get Modules that are ready
	    if (xpci_modAskReady(&m_modules_mask) == 0)
	    {
		    DEB_TRACE() << "Ask modules that are ready: OK (modules mask = 0x" << std::hex << m_modules_mask << ")" ;
		    m_module_number = xpci_getModNb(m_modules_mask);
            
		    if (m_module_number != 0)
		    {
			    DEB_TRACE() << "--> Number of Modules	= " << m_module_number ;			
		    }
		    else
		    {
			    DEB_ERROR() << "No modules found: retry to Init" ;
			    //- Test if PCIe is OK
			    if(xpci_isPCIeOK() == 0) 
			    {
				    DEB_TRACE() << "PCIe hardware check is OK" ;
			    }
			    else
			    {
				    DEB_ERROR() << "PCIe hardware check has FAILED:" ;
				    DEB_ERROR() << "1. Check if green led is ON (if not go to p.3)" ;
				    DEB_ERROR() << "2. Reset PCIe board" ;
				    DEB_ERROR() << "3. Power off and power on PC (do not reboot, power has to be cut off)\n" ;
				    throw LIMA_HW_EXC(Error, "PCIe hardware check has FAILED!");
			    }
			    throw LIMA_HW_EXC(Error, "No modules found: retry to Init");			
		    }	
	    }
	    else
	    {
		    DEB_ERROR() << "Ask modules that are ready: FAILED" ;
		    throw LIMA_HW_EXC(Error, "No Modules are ready");
	    }

	    // ATTENTION: Modules should be ordered! 
		m_image_size = Size(CHIP_NB_COLUMN * m_chip_number ,CHIP_NB_ROW * m_module_number);
        
	    DEB_TRACE() << "--> Number of chips 		 = " << std::dec << m_chip_number ;
	    DEB_TRACE() << "--> Image width 	(pixels) = " << std::dec << m_image_size.getWidth() ;
	    DEB_TRACE() << "--> Image height	(pixels) = " << std::dec << m_image_size.getHeight() ;
		go(2000);

        //- allocate the dacl array: not used yet
        //m_dacl = new unsigned short[m_image_size.getWidth() * m_image_size.getHeight()];
    }
}

//---------------------------
//- Dtor
//---------------------------
Camera::~Camera()
{
	DEB_DESTRUCTOR();

	//- close the xpix driver
	xpci_close(0);
	DEB_TRACE() << "XPCI Lib closed";

    //delete [] m_dacl;
}

//---------------------------
//- Camera::start()
//---------------------------
void Camera::start()
{
	DEB_MEMBER_FUNCT();

    m_stop_asked = false;
	unsigned long local_nb_frames = 0;

	DEB_TRACE() << "m_acquisition_type = " << m_acquisition_type ;

	DEB_TRACE() << "Setting Exposure parameters with values: ";
	DEB_TRACE() << "\tm_exp_time_usec 		= " << m_exp_time_usec;
	DEB_TRACE() << "\tm_imxpad_trigger_mode = " << m_imxpad_trigger_mode;

	DEB_TRACE() << "\tm_nb_frames (before live mode check)			= " << m_nb_frames;
	DEB_TRACE() << "\tm_imxpad_format 		= " << m_imxpad_format;

	//- Check if live mode
	if (m_nb_frames == 0) //- ie live mode
		local_nb_frames = 1;
	else
		local_nb_frames = m_nb_frames;

	DEB_TRACE() << "\tlocal_nb_frames (after live mode check)       = " << local_nb_frames;
		
	//- call the setExposureParameters
	setExposureParameters(	m_exp_time_usec,
							m_time_between_images_usec,
							m_time_before_start_usec,
							m_shutter_time_usec,
							m_ovf_refresh_time_usec,
							m_imxpad_trigger_mode,
							m_specific_param_n,
							m_specific_param_p,
							local_nb_frames,
							m_busy_out_sel,
							m_imxpad_format,
							XPIX_NOT_USED_YET,
							m_specific_param_GP1,
							m_specific_param_GP2,
							m_specific_param_GP3,
							m_specific_param_GP4);


	if (m_nb_frames == 0) //- aka live mode
    {
        //- Post XPAD_DLL_START_LIVE_ACQ_MSG msg
		this->post(new yat::Message(XPAD_DLL_START_LIVE_ACQ_MSG), kPOST_MSG_TMO);
    }
    else if(m_acquisition_type == Camera::SYNC)
	{
		//- Post XPAD_DLL_START_SYNC_MSG msg
		this->post(new yat::Message(XPAD_DLL_START_SYNC_MSG), kPOST_MSG_TMO);
	}
	else if (m_acquisition_type == Camera::ASYNC)
	{
		//- Post XPAD_DLL_START_ASYNC_MSG msg
		this->post(new yat::Message(XPAD_DLL_START_ASYNC_MSG), kPOST_MSG_TMO);
	}
	else
	{
		DEB_ERROR() << "Acquisition type not supported" ;
		throw LIMA_HW_EXC(Error, "Acquisition type not supported: possible values are:\n0->SYNC\n1->ASYNC");
	}
}

//---------------------------
//- Camera::stop()
//---------------------------
void Camera::stop()
{
	DEB_MEMBER_FUNCT();

	//- call the abort fct from xpix lib
	xpci_modAbortExposure();
    m_stop_asked = true;

	m_status = Camera::Ready;
}

//-----------------------------------------------------
//- Camera::getImageSize(Size& size)
//-----------------------------------------------------
void Camera::getImageSize(Size& size)
{
	DEB_MEMBER_FUNCT();

	if (m_doublepixel_corr)
		m_image_size = Size(S140_CORRECTED_NB_COLUMN,S140_CORRECTED_NB_ROW);//- For S140 only
	else if (m_geom_corr)
		m_image_size = Size(S540_CORRECTED_NB_COLUMN,S540_CORRECTED_NB_ROW); //- For S540 only
	else
		m_image_size = Size(CHIP_NB_COLUMN * m_chip_number ,CHIP_NB_ROW * m_module_number);

	size = m_image_size;
}

//-----------------------------------------------------
//- Camera::getPixelSize(double& x_size,double& y_size)
//-----------------------------------------------------
void Camera::getPixelSize(double& x_size,double& y_size)
{
	DEB_MEMBER_FUNCT();
	x_size = 130; // pixel size is 130 micron
    y_size = 130;
}

//-----------------------------------------------------
//- Camera::setPixelDepth(ImageType pixel_depth)
//-----------------------------------------------------
void Camera::setPixelDepth(ImageType pixel_depth)
{
	DEB_MEMBER_FUNCT();
	switch( pixel_depth )
	{
	case Bpp16:
		m_pixel_depth = B2;
        m_imxpad_format = 0;
		break;

	case Bpp32:
		m_pixel_depth = B4;
        m_imxpad_format = 1;
		break;

	default:
		DEB_ERROR() << "Pixel Depth is unsupported: only 16 or 32 bits is supported" ;
		throw LIMA_HW_EXC(Error, "Pixel Depth is unsupported: only 16 or 32 bits is supported");
		break;
	}
}

//-----------------------------------------------------
//- Camera::getPixelDepth(ImageType& pixel_depth)
//-----------------------------------------------------
void Camera::getPixelDepth(ImageType& pixel_depth)
{
	DEB_MEMBER_FUNCT();
	switch( m_imxpad_format )
	{
	case 0:
		pixel_depth = Bpp16;
		break;

	case 1:
		pixel_depth = Bpp32;
		break;	
	}
}

//-----------------------------------------------------
//- Camera::getDetectorType(string& type)
//-----------------------------------------------------
void Camera::getDetectorType(string& type)
{
	DEB_MEMBER_FUNCT();
	type = "XPAD";
}

//-----------------------------------------------------
//- Camera::getDetectorModel(string& type)
//-----------------------------------------------------
void Camera::getDetectorModel(string& type)
{
	DEB_MEMBER_FUNCT();
	if		(m_xpad_model == BACKPLANE)	type = "BACKPLANE";
	else if	(m_xpad_model == HUB)			type = "HUB";
	else if	(m_xpad_model == IMXPAD_S70)	type = "IMXPAD_S70";
	else if	(m_xpad_model == IMXPAD_S140)	type = "IMXPAD_S140";
	else if	(m_xpad_model == IMXPAD_S340)	type = "IMXPAD_S340";
	else if	(m_xpad_model == IMXPAD_S420)	type = "IMXPAD_S420";
	else if	(m_xpad_model == IMXPAD_S540)	type = "IMXPAD_S540";
    else throw LIMA_HW_EXC(Error, "Xpad Type not supported");
}

//-----------------------------------------------------
//
//-----------------------------------------------------
BufferCtrlMgr& Camera::getBufferMgr()
{
	return m_buffer_ctrl_mgr;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setTrigMode(TrigMode mode)
{
	DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(mode);

	switch( mode )
	{
	case IntTrig:
        m_imxpad_trigger_mode = 0;
		break;
	case ExtGate:
        m_imxpad_trigger_mode = 1;
		break;
	case ExtTrigSingle:
		m_imxpad_trigger_mode = 2; //- 1 trig externe declenche N gates internes (les gate etant regl� par soft)
		break;
    case ExtTrigMult:
		m_imxpad_trigger_mode = 3; //- N trig externes declenchent N gates internes (les gate etant regl� par soft) 
		break;
	default:
		DEB_ERROR() << "Error: Trigger mode unsupported: only IntTrig, ExtGate, ExtTrigSingle or ExtTrigMult" ;
		throw LIMA_HW_EXC(Error, "Trigger mode unsupported: only IntTrig, ExtGate, ExtTrigSingle or ExtTrigMult");
		break;
	}
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getTrigMode(TrigMode& mode)
{
	DEB_MEMBER_FUNCT();
	switch( m_imxpad_trigger_mode )
	{
	case 0:
		mode = IntTrig;
		break;
	case 1:
		mode = ExtGate;
		break;
	case 2:
		mode = ExtTrigSingle; //- 1 trig externe declenche N gates internes (les gate etant regl� par soft)
		break;
    case 3:
		mode = ExtTrigMult; //- N trig externes declenchent N gates internes (les gate etant regl� par soft) 
		break;
	default:
		break;
	}
	DEB_RETURN() << DEB_VAR1(mode);
}


//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setExpTime(double exp_time_sec)
{
	DEB_MEMBER_FUNCT();

	DEB_PARAM() << DEB_VAR1(exp_time_sec);

    m_exp_time_usec = exp_time_sec * 1e6;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getExpTime(double& exp_time_sec)
{
	DEB_MEMBER_FUNCT();

	exp_time_sec = m_exp_time_usec / 1e6;
	
	DEB_RETURN() << DEB_VAR1(exp_time_sec);
}


//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setNbFrames(int nb_frames)
{
	DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(nb_frames);
	m_nb_frames = nb_frames;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getNbFrames(int& nb_frames)
{
	DEB_MEMBER_FUNCT();
	nb_frames = m_nb_frames;
	DEB_RETURN() << DEB_VAR1(nb_frames);
}

//---------------------------------------------------------------------------------------
// Camera::getNbHwAcquiredFrames()
//---------------------------------------------------------------------------------------
int Camera::getNbHwAcquiredFrames()
{
	return m_current_nb_frames + 1; //- because m_current_nb_frames start at 0 
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getStatus(Camera::Status& status)
{
	DEB_MEMBER_FUNCT();
	status = m_status;
	DEB_RETURN() << DEB_VAR1(DEB_HEX(status));
}
 
//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::handle_message( yat::Message& msg )  throw( yat::Exception )
{
	DEB_MEMBER_FUNCT();
	try
	{
		switch ( msg.type() )
		{
			//-----------------------------------------------------	
		case yat::TASK_INIT:
			{
				DEB_TRACE() <<"Camera::->TASK_INIT";          
			}
			break;
			//-----------------------------------------------------    
		case yat::TASK_EXIT:
			{
				DEB_TRACE() <<"Camera::->TASK_EXIT";                
			}
			break;
			//-----------------------------------------------------    
		case yat::TASK_TIMEOUT:
			{
				DEB_TRACE() <<"Camera::->TASK_TIMEOUT";       
			}
			break;
			//-----------------------------------------------------    
		case XPAD_DLL_START_SYNC_MSG:
			{
				DEB_TRACE() <<"Camera::->XPAD_DLL_START_SYNC_MSG";

				//- A mettre dans le prepareAcq?
                
				//- Declare local temporary image buffer
				void**	image_array;

				// allocate multiple buffers
				DEB_TRACE() <<"Allocating images array (" << m_nb_frames << " images)";
				if(m_imxpad_format == 0) //- aka 16 bits
					image_array = reinterpret_cast<void**>(new uint16_t* [ m_nb_frames ]);
				else //- aka 32 bits
					image_array = reinterpret_cast<void**>(new uint32_t* [ m_nb_frames ]);
                
                DEB_TRACE() <<"Allocating every image pointer of the images array";
				for( int i=0 ; i < m_nb_frames ; i++ )
				{
					if(m_imxpad_format == 0) //- aka 16 bits
						image_array[i] = new uint16_t [ m_image_size.getWidth() * m_image_size.getHeight() ];//we allocate a number of pixels
					else //- aka 32 bits
						image_array[i] = new uint32_t [ m_image_size.getWidth() * m_image_size.getHeight() ];//we allocate a number of pixels
				}

				m_status = Camera::Exposure;

				//- Start the img sequence
				DEB_TRACE() <<"Start acquiring a sequence of images";

                Timestamp start_sec = Timestamp::now();
                //yat::int64 start = Time::microsecs();

				if ( xpci_getImgSeq(	m_pixel_depth, 
					                    m_modules_mask,
					                    m_chip_number,
                            			m_nb_frames,
                            			(void**)image_array,
                            			// next are ignored in V2:
                            			XPIX_V1_COMPATIBILITY,
					                    XPIX_V1_COMPATIBILITY,
					                    XPIX_V1_COMPATIBILITY,
					                    XPIX_V1_COMPATIBILITY) == -1)
				{
					DEB_ERROR() << "Error: xpci_getImgSeq as returned an error..." ;

					DEB_TRACE() << "Freeing each image pointer of the images array";
					for(int i=0 ; i < m_nb_frames ; i++)
					{
						if(m_pixel_depth == B2)
							delete[] reinterpret_cast<uint16_t*>(image_array[i]);
						else
							delete[] reinterpret_cast<uint32_t*>(image_array[i]);
					}

					DEB_TRACE() << "Freeing the images array";
					if(m_imxpad_format == 0) //- aka 16 bits
						delete[] reinterpret_cast<uint16_t**>(image_array);
					else //- aka 32 bits
						delete[] reinterpret_cast<uint32_t**>(image_array);

					m_status = Camera::Fault;
                    throw LIMA_HW_EXC(Error, "xpci_getImgSeq as returned an error ! ");
				}

                Timestamp end_sec = Timestamp::now() - start_sec;
                DEB_TRACE() << "Time for xpci_getImgSeq (sec) = " << end_sec;

				m_status = Camera::Readout;

				DEB_TRACE() 	<< "\n#######################"
								<< "\nall images are acquired"
								<< "\n#######################" ;

				int	i=0;

				//- Publish each image and call new frame ready for each frame
				StdBufferCbMgr& buffer_mgr = m_buffer_cb_mgr;
				DEB_TRACE() <<"Publish each acquired image through newFrameReady()";
                start_sec = Timestamp::now();
				for(i=0; i<m_nb_frames; i++)
				{
                    m_current_nb_frames = i;
					int buffer_nb, concat_frame_nb;
					buffer_mgr.setStartTimestamp(Timestamp::now());
					buffer_mgr.acqFrameNb2BufferNb(i, buffer_nb, concat_frame_nb);

					void* lima_img_ptr;
					if(m_imxpad_format == 0) //- aka 16 bits
						lima_img_ptr = (uint16_t*)(buffer_mgr.getBufferPtr(buffer_nb,concat_frame_nb));
					else //- aka 32 bits
						lima_img_ptr = (uint32_t*)(buffer_mgr.getBufferPtr(buffer_nb,concat_frame_nb));

					//- copy image in the lima buffer
					if(m_imxpad_format == 0) //- aka 16 bits
					{
						if(m_doublepixel_corr) //- For S140 only
						{	
							uint16_t corrected_image[S140_CORRECTED_NB_ROW][S140_CORRECTED_NB_COLUMN];
							doublePixelCorrection<uint16_t>((uint16_t *)image_array[i],corrected_image);
							memcpy((uint16_t *)lima_img_ptr, (uint16_t *)corrected_image,m_image_size.getWidth() * m_image_size.getHeight() * sizeof(uint16_t));
						}
						else //- no double pix correction
							memcpy((uint16_t *)lima_img_ptr, (uint16_t *)image_array[i],m_image_size.getWidth() * m_image_size.getHeight() * sizeof(uint16_t));
					}
					else //- aka 32 bits
					{
						if(m_doublepixel_corr)
						{
							uint32_t corrected_image[S140_CORRECTED_NB_ROW][S140_CORRECTED_NB_COLUMN];
							doublePixelCorrection<uint32_t>((uint32_t *)image_array[i],corrected_image);
							memcpy((uint32_t *)lima_img_ptr, (uint32_t *)corrected_image,m_image_size.getWidth() * m_image_size.getHeight() * sizeof(uint32_t));
						}
						else
							memcpy((uint32_t *)lima_img_ptr, (uint32_t *)image_array[i],m_image_size.getWidth() * m_image_size.getHeight() * sizeof(uint32_t));
					}

					HwFrameInfoType frame_info;
					frame_info.acq_frame_nb = i;
					//- raise the image to Lima
					buffer_mgr.newFrameReady(frame_info);
                    DEB_TRACE() << "image " << i <<" published with newFrameReady()" ;
				}

                end_sec = Timestamp::now() - start_sec;
                DEB_TRACE() << "Time for publishing images to Lima (sec) = " << end_sec;


                start_sec = Timestamp::now();
				DEB_TRACE() <<"Freeing every image pointer of the images array";
                //- they were allocated by the xpci_getImgSeq function
				for(i=0 ; i < m_nb_frames ; i++)
					delete[] image_array[i];
				DEB_TRACE() <<"Freeing images array";
				delete[] image_array;
				m_status = Camera::Ready;
                end_sec = Timestamp::now() - start_sec;
                DEB_TRACE() << "Time for freeing memory: now Ready! (sec) = " << end_sec;
				DEB_TRACE() <<"m_status is Ready";

			}
			break;

            //-----------------------------------------------------    
			case XPAD_DLL_START_LIVE_ACQ_MSG:
			{
				DEB_TRACE() << "=========================================";
                DEB_TRACE() <<"Camera::->XPAD_DLL_START_LIVE_ACQ_MSG";

				int one_frame = 1;

				//- Declare local temporary image buffer
				void**	image_array;

				// allocate multiple buffers
				DEB_TRACE() <<"Allocating images array (" << one_frame << " images)";
				if(m_imxpad_format == 0) //- aka 16 bits
				{
					image_array = reinterpret_cast<void**>(new uint16_t* [ one_frame ]);
					image_array[0] = new uint16_t [ m_image_size.getWidth() * m_image_size.getHeight() ];//we allocate a number of pixels
				}
				else //- aka 32 bits
				{
					image_array = reinterpret_cast<void**>(new uint32_t* [ one_frame ]);
					image_array[0] = new uint32_t [ m_image_size.getWidth() * m_image_size.getHeight() ];//we allocate a number of pixels
				}

				m_status = Camera::Exposure;

				//- Start the img sequence
				DEB_TRACE() <<"Start acquiring an image";

				if ( xpci_getImgSeq(	m_pixel_depth,  //- use the CPPM type
					                    m_modules_mask,
					                    m_chip_number,
                            			one_frame,
                            			(void**)image_array,
                            			// next are ignored in V2:
                            			XPIX_V1_COMPATIBILITY,
					                    XPIX_V1_COMPATIBILITY,
					                    XPIX_V1_COMPATIBILITY,
					                    XPIX_V1_COMPATIBILITY) == -1)
				{
					DEB_ERROR() << "Error: getImgSeq as returned an error..." ;
					DEB_TRACE() << "Freeing the image pointer of the image array";
					
					if(m_imxpad_format == 0) //- aka 16 bits
						delete[] reinterpret_cast<uint16_t*>(image_array[0]);
					else //- aka 32 bits
						delete[] reinterpret_cast<uint32_t*>(image_array[0]);

					DEB_TRACE() << "Freeing the image array";
					if(m_imxpad_format == 0) //- aka 16 bits
						delete[] reinterpret_cast<uint16_t**>(image_array);
					else //- aka 32 bits
						delete[] reinterpret_cast<uint32_t**>(image_array);

					m_status = Camera::Fault;
                    throw LIMA_HW_EXC(Error, "xpci_getImgSeq as returned an error ! ");
				}

				m_status = Camera::Readout;

				DEB_TRACE() 	<< "\n#######################"
								<< "\nall images are acquired"
								<< "\n#######################" ;

				//- Publish the image and call new frame ready for each frame
				StdBufferCbMgr& buffer_mgr = m_buffer_cb_mgr;
				DEB_TRACE() <<"Publish the acquired image through newFrameReady()";
				
                m_current_nb_frames = 0;
				int buffer_nb, concat_frame_nb;
				buffer_mgr.setStartTimestamp(Timestamp::now());
				buffer_mgr.acqFrameNb2BufferNb(m_current_nb_frames, buffer_nb, concat_frame_nb);

				void* lima_img_ptr;
				if(m_imxpad_format == 0) //- aka 16 bits
					lima_img_ptr = (uint16_t*)(buffer_mgr.getBufferPtr(buffer_nb,concat_frame_nb));
				else //- aka 32 bits
					lima_img_ptr = (uint32_t*)(buffer_mgr.getBufferPtr(buffer_nb,concat_frame_nb));

				//- copy image in the lima buffer
				if(m_imxpad_format == 0) //- aka 16 bits
				{
					if(m_doublepixel_corr) //- For S140 only
					{	
						uint16_t corrected_image[S140_CORRECTED_NB_ROW][S140_CORRECTED_NB_COLUMN];
						doublePixelCorrection<uint16_t>((uint16_t *)image_array[0],corrected_image);
						memcpy((uint16_t *)lima_img_ptr, (uint16_t *)corrected_image,m_image_size.getWidth() * m_image_size.getHeight() * sizeof(uint16_t));
					}
					else //- no double pix correction
						memcpy((uint16_t *)lima_img_ptr, (uint16_t *)image_array[0],m_image_size.getWidth() * m_image_size.getHeight() * sizeof(uint16_t));
				}
				else //- aka 32 bits
				{
					if(m_doublepixel_corr)
					{
						uint32_t corrected_image[S140_CORRECTED_NB_ROW][S140_CORRECTED_NB_COLUMN];
						doublePixelCorrection<uint32_t>((uint32_t *)image_array[0],corrected_image);
						memcpy((uint32_t *)lima_img_ptr, (uint32_t *)corrected_image,m_image_size.getWidth() * m_image_size.getHeight() * sizeof(uint32_t));
					}
					else
						memcpy((uint32_t *)lima_img_ptr, (uint32_t *)image_array[0],m_image_size.getWidth() * m_image_size.getHeight() * sizeof(uint32_t));
				}

				DEB_TRACE() << "image cleaned" ;
				HwFrameInfoType frame_info;
				frame_info.acq_frame_nb = 0;
				//- raise the image to Lima
				buffer_mgr.newFrameReady(frame_info);
				
				DEB_TRACE() <<"Freeing every image pointer of the images array";
				
				if(m_imxpad_format == 0) //- aka 16 bits
					delete[] reinterpret_cast<uint16_t*>(image_array[0]);
				else //- aka 32 bits
					delete[] reinterpret_cast<uint32_t*>(image_array[0]);

				DEB_TRACE() << "Freeing images array";
				if(m_imxpad_format == 0) //- aka 16 bits
					delete[] reinterpret_cast<uint16_t**>(image_array);
				else //- aka 32 bits
					delete[] reinterpret_cast<uint32_t**>(image_array);

				m_status = Camera::Ready;
				DEB_TRACE() <<"m_status is Ready";

                if (m_stop_asked == false)
                {
                    //- Post XPAD_DLL_START_LIVE_ACQ_MSG msg
		            this->post(new yat::Message(XPAD_DLL_START_LIVE_ACQ_MSG), kPOST_MSG_TMO);
                }
			}
			break;          

			//-----------------------------------------------------    
			case XPAD_DLL_START_ASYNC_MSG:
			{
				DEB_TRACE() << "=========================================";
                DEB_TRACE() << "Camera::->XPAD_DLL_START_ASYNC_MSG";

                m_status = Camera::Exposure;

                //- Start the img sequence
                DEB_TRACE() <<"Start acquiring asynchronously a sequence of image(s)";

                m_start_sec = Timestamp::now();

                if ( xpci_getImgSeqAsync(   m_pixel_depth, 
                                            m_modules_mask,
                                            m_nb_frames
                                        ) == -1)              
                {
                    DEB_ERROR() << "Error: xpci_getImgSeqAsync has returned an error..." ;
                    m_status = Camera::Fault;
                    throw LIMA_HW_EXC(Error, "xpci_getImgSeqAsync has returned an error ! ");
                }

                //- Post XPAD_DLL_GET_ASYNC_IMAGES_MSG msg
                this->post(new yat::Message(XPAD_DLL_GET_ASYNC_IMAGES_MSG), kPOST_MSG_TMO);
            }
            break;

            //-----------------------------------------------------    
        case XPAD_DLL_GET_ASYNC_IMAGES_MSG:
            {
                DEB_TRACE() <<"Camera::->XPAD_DLL_GET_ASYNC_IMAGES_MSG";
                
                //- TODO: manage the corrected img 
                void	*one_image;
				float	*one_corrected_image;

                int		image_counter = 0;
                int		nb_last_aquired_image = 0;

				if(m_imxpad_format == 0) //- aka 16 bits
				{
					if(m_doublepixel_corr) //- the image returned by xpix is not double pixel corrected !!
						one_image = new uint16_t [ (m_image_size.getWidth()-18) * (m_image_size.getHeight()-3) ];
					else
						one_image = new uint16_t [ m_image_size.getWidth() * m_image_size.getHeight() ];
				}
                else //- aka 32 bits
                    one_image = new uint32_t [ m_image_size.getWidth() * m_image_size.getHeight() ];

				//- the geometric corrected image is returned by the xpix lib
				if(m_geom_corr) //- only for swing S540 xpad (578 * 1156) 
					one_corrected_image = new float [ m_image_size.getWidth() * m_image_size.getHeight() ];
				
                //- workaround to a bug in xpci_getNumberLastAcquiredAsyncImage(): have to wait a little before calling it
                yat::ThreadingUtilities::sleep(1+m_exp_time_usec / 1e6);  //- wait at least exp time in sec + 1 sec

                DEB_TRACE() << "m_nb_frames         = " << m_nb_frames;

                while (image_counter < m_nb_frames)
                {
                    nb_last_aquired_image = xpci_getNumberLastAcquiredAsyncImage();

                    //- FL: hacked from imxpad ... don't know what is it
                    if(nb_last_aquired_image < 0)
                        break;

                    if (image_counter < nb_last_aquired_image)
                    {               	
                        DEB_TRACE() << "nb_last_aquired_image = " << nb_last_aquired_image;
                    	DEB_TRACE() << "image_counter         = " << image_counter;
                    
                        if ( xpci_getAsyncImage(    m_pixel_depth, 
                                                    m_modules_mask,
                                                    m_chip_number,
                                                    m_nb_frames,
                                                    (void*)one_image, //- base img
                                                    image_counter, //- image index to get
                                                    (void*)one_corrected_image, //- corrected img
                                                    m_geom_corr //- flag for activating correction
                                                ) == -1)           

                        {
                            DEB_ERROR() << "Error: xpci_getAsyncImage has returned an error..." ;

                            DEB_TRACE() << "Freeing the image";
                            delete[] one_image;
							if(m_geom_corr)
								delete[] one_corrected_image;

                            m_status = Camera::Fault;
                            throw LIMA_HW_EXC(Error, "xpci_getAsyncImage has returned an error ! ");
                        }

                        //- Publish each image and call new frame ready for each frame
                        StdBufferCbMgr& buffer_mgr = m_buffer_cb_mgr;
                        DEB_TRACE() << "Publishing image : " << image_counter << " through newFrameReady()";
                        m_start_sec = Timestamp::now();
                        
                        m_current_nb_frames = image_counter;
                        int buffer_nb, concat_frame_nb;
                        buffer_mgr.setStartTimestamp(Timestamp::now());
                        buffer_mgr.acqFrameNb2BufferNb(m_current_nb_frames, buffer_nb, concat_frame_nb);

                        void* lima_img_ptr;
						if(m_geom_corr) //- For S540 only
						{
							lima_img_ptr = (float*)(buffer_mgr.getBufferPtr(buffer_nb,concat_frame_nb));
                            memcpy((float*)lima_img_ptr, (float*)one_corrected_image, m_image_size.getWidth() * m_image_size.getHeight() * sizeof(float));
						}
						else
						{
							if(m_imxpad_format == 0) //- aka 16 bits
								lima_img_ptr = (uint16_t*)(buffer_mgr.getBufferPtr(buffer_nb,concat_frame_nb));
							else //- aka 32 bits
								lima_img_ptr = (uint32_t*)(buffer_mgr.getBufferPtr(buffer_nb,concat_frame_nb));

							//- copy image in the lima buffer
							if(m_imxpad_format == 0) //- aka 16 bits
							{
								if(m_doublepixel_corr) //- For S140 only
								{	
									uint16_t corrected_image[S140_CORRECTED_NB_ROW][S140_CORRECTED_NB_COLUMN]; 
									doublePixelCorrection<uint16_t>((uint16_t *)one_image,corrected_image);
									memcpy((uint16_t *)lima_img_ptr, (uint16_t *)corrected_image,m_image_size.getWidth() * m_image_size.getHeight() * sizeof(uint16_t));
								}
								else //- no double pix correction
									memcpy((uint16_t *)lima_img_ptr, (uint16_t *)one_image,m_image_size.getWidth() * m_image_size.getHeight() * sizeof(uint16_t));
							}
							else //- aka 32 bits
							{
								if(m_doublepixel_corr)
								{
									uint32_t corrected_image[S140_CORRECTED_NB_ROW][S140_CORRECTED_NB_COLUMN]; 
									doublePixelCorrection<uint32_t>((uint32_t *)one_image,corrected_image);
									memcpy((uint32_t *)lima_img_ptr, (uint32_t *)corrected_image,m_image_size.getWidth() * m_image_size.getHeight() * sizeof(uint32_t));
								}
								else
									memcpy((uint32_t *)lima_img_ptr, (uint32_t *)one_image,m_image_size.getWidth() * m_image_size.getHeight() * sizeof(uint32_t));
							}
						}

                        HwFrameInfoType frame_info;
                        frame_info.acq_frame_nb = image_counter;
						//- raise the image to Lima
                        buffer_mgr.newFrameReady(frame_info);
                        DEB_TRACE() << "image " << image_counter <<" published with newFrameReady()" ;
                        image_counter++;
                    }
                }   
                
                DEB_TRACE() << "End: nb_last_aquired_image = " << nb_last_aquired_image;
                DEB_TRACE() << "End: image_counter         = " << image_counter;

                //- Finished
                m_start_sec = Timestamp::now();
                DEB_TRACE() << "Freeing last image pointer";
                delete[] one_image;
				if(m_geom_corr)
					delete[] one_corrected_image;

                m_status = Camera::Ready;
                m_end_sec = Timestamp::now() - m_start_sec;
                DEB_TRACE() << "Time for freeing memory: now Ready! (sec) = " << m_end_sec;
                DEB_TRACE() << "m_status is Ready";
            }
            break;

			//-----------------------------------------------------    
			case XPAD_DLL_CALIBRATE_OTN_SLOW:
				{
					DEB_TRACE() <<"Camera::->XPAD_DLL_CALIBRATE_OTN_SLOW";

					m_status = Camera::Calibrating;


					if(imxpad_calibration_OTN_slow(m_modules_mask,(char*)m_calibration_path.c_str(),m_calibration_adjusting_number) == 0)
					{
						DEB_TRACE() << "imxpad_calibration_OTN_slow -> OK" ;
					}
					else
					{
						Event *my_event = new Event(Hardware, Event::Error, Event::Camera, Event::Default, "imxpad_calibration_OTN_slow() : error for path: " + m_calibration_path);
						//DEB_EVENT(*my_event) << DEB_VAR1(*my_event);
						reportEvent(my_event);

						m_status = Camera::Fault;
						//- TODO: get the xpix error 
						throw LIMA_HW_EXC(Error, "Error in imxpad_calibration_OTN_slow!");
					}
					m_status = Camera::Ready;
				}
				break;
			//-----------------------------------------------------    
			case XPAD_DLL_CALIBRATE_OTN_MEDIUM:
				{
					DEB_TRACE() <<"Camera::->XPAD_DLL_CALIBRATE_OTN_MEDIUM";

					m_status = Camera::Calibrating;


					if(imxpad_calibration_OTN_medium(m_modules_mask,(char*)m_calibration_path.c_str(),m_calibration_adjusting_number) == 0)
					{
						DEB_TRACE() << "imxpad_calibration_OTN_medium -> OK" ;
					}
					else
					{
						Event *my_event = new Event(Hardware, Event::Error, Event::Camera, Event::Default, "imxpad_calibration_OTN_medium() : error for path: " + m_calibration_path);
						//DEB_EVENT(*my_event) << DEB_VAR1(*my_event);
						reportEvent(my_event);

						m_status = Camera::Fault;
						//- TODO: get the xpix error 
						throw LIMA_HW_EXC(Error, "Error in imxpad_calibration_OTN_medium!");
					}
					m_status = Camera::Ready;
				}
				break;

			//-----------------------------------------------------    
			case XPAD_DLL_CALIBRATE_OTN_FAST:
				{
					DEB_TRACE() <<"Camera::->XPAD_DLL_CALIBRATE_OTN_FAST";

					m_status = Camera::Calibrating;


					if(imxpad_calibration_OTN_fast(m_modules_mask,(char*)m_calibration_path.c_str(),m_calibration_adjusting_number) == 0)
					{
						DEB_TRACE() << "imxpad_calibration_OTN_fast -> OK" ;
					}
					else
					{
						Event *my_event = new Event(Hardware, Event::Error, Event::Camera, Event::Default, "imxpad_calibration_OTN_fast() : error for path: " + m_calibration_path);
						//DEB_EVENT(*my_event) << DEB_VAR1(*my_event);
						reportEvent(my_event);

						m_status = Camera::Fault;
						//- TODO: get the xpix error 
						throw LIMA_HW_EXC(Error, "Error in imxpad_calibration_OTN_fast!");
					}
					m_status = Camera::Ready;
				}
				break;

			//-----------------------------------------------------    
			case XPAD_DLL_CALIBRATE_BEAM:
				{
					DEB_TRACE() <<"Camera::->XPAD_DLL_CALIBRATE_BEAM";

					m_status = Camera::Calibrating;

					if(imxpad_calibration_BEAM(m_modules_mask,(char*)m_calibration_path.c_str(),m_calib_texp,m_calib_ithl_max,m_calib_itune,m_calib_imfp) == 0)
					{
						DEB_TRACE() << "imxpad_calibration_BEAM -> OK" ;
					}
					else
					{
						Event *my_event = new Event(Hardware, Event::Error, Event::Camera, Event::Default, "imxpad_calibration_BEAM() : error for path: " + m_calibration_path);
						//DEB_EVENT(*my_event) << DEB_VAR1(*my_event);
						reportEvent(my_event);

						m_status = Camera::Fault;
						//- TODO: get the xpix error 
						throw LIMA_HW_EXC(Error, "Error in imxpad_calibration_BEAM!");
					}
					m_status = Camera::Ready;
				}
				break;
			//-----------------------------------------------------    
			case XPAD_DLL_CALIBRATE_OTN:
				{
					DEB_TRACE() <<"Camera::->XPAD_DLL_CALIBRATE_OTN";

					m_status = Camera::Calibrating;

					if(imxpad_calibration_OTN(m_modules_mask,(char*)m_calibration_path.c_str(),m_calibration_adjusting_number,m_calib_itune,m_calib_imfp) == 0)
					{
						DEB_TRACE() << "imxpad_calibration_OTN -> OK" ;
					}
					else
					{

						Event *my_event = new Event(Hardware, Event::Error, Event::Camera, Event::Default, "imxpad_calibration_OTN() : error for path: " + m_calibration_path);
						//DEB_EVENT(*my_event) << DEB_VAR1(*my_event);
						reportEvent(my_event);

						m_status = Camera::Fault;
						//- TODO: get the xpix error 
						throw LIMA_HW_EXC(Error, "Error in imxpad_calibration_OTN!");
					}
					m_status = Camera::Ready;
				}
				break;

            //-----------------------------------------------------	
            case XPAD_DLL_UPLOAD_CALIBRATION:
                {
                    DEB_TRACE() <<"Camera::->XPAD_DLL_UPLOAD_CALIBRATION";    

                    if(imxpad_uploadCalibration(m_modules_mask,(char*)m_calibration_path.c_str()) == 0)
                    {
                        DEB_TRACE() << "imxpad_uploadCalibration -> OK" ;
                    }
                    else
                    {
                        m_status = Camera::Fault;
                        //- TODO: get the xpix error 
                        throw LIMA_HW_EXC(Error, "Error in imxpad_uploadCalibration!");
                    }
                    m_status = Camera::Ready;
                }
                break;
		}
  }
  catch( yat::Exception& ex )
  {
	  DEB_ERROR() << "Error : " << ex.errors[0].desc;
  }
}


//-----------------------------------------------------
//      setExposureParam
//-----------------------------------------------------
void Camera::setExposureParameters( unsigned Texp,unsigned Twait,unsigned Tinit,
			                         unsigned Tshutter,unsigned Tovf,unsigned trigger_mode, unsigned n,unsigned p,
			                         unsigned nbImages,unsigned BusyOutSel,unsigned formatIMG,unsigned postProc,
			                         unsigned GP1,unsigned GP2,unsigned GP3,unsigned GP4)
{
	DEB_MEMBER_FUNCT();

    if (xpci_modExposureParam(m_modules_mask, Texp, Twait, Tinit,
	                          Tshutter, Tovf, trigger_mode,  n, p,
	                          nbImages, BusyOutSel, formatIMG, postProc,
	                          GP1, GP2, GP3, GP4) == 0)
	{
		DEB_TRACE() << "setExposureParameters (xpci_modExposureParam) -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in setExposureParameters! (xpci_modExposureParam)");
	}
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setAcquisitionType(short acq_type)
{
	DEB_MEMBER_FUNCT();

    m_acquisition_type = (Camera::XpadAcqType)acq_type;

	//- in SYNC mode: Geom corrections for S540 are not supported
	if(m_acquisition_type == Camera::SYNC)
	{
		m_geom_corr = 0;

		if (m_maximage_size_cb_active) 
		{
			// only if the callaback is active
			// inform lima about the size change
			ImageType pixel_depth;
			Size image_size;
			getPixelDepth(pixel_depth); //- ie Bpp16 ...
			getImageSize(image_size); //- size of image
			maxImageSizeChanged(image_size, pixel_depth);       
		}
	}

	DEB_TRACE() << "m_acquisition_type = " << m_acquisition_type  ;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::loadFlatConfig(unsigned flat_value)
{
	DEB_MEMBER_FUNCT();

	unsigned int all_chips_mask = 0x7F;
	if (xpci_modLoadFlatConfig(m_modules_mask, all_chips_mask, flat_value) == 0)
	{
		DEB_TRACE() << "loadFlatConfig, with value: " <<  flat_value << " -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in loadFlatConfig!");
	}
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::loadAllConfigG(unsigned long modNum, unsigned long chipId , unsigned long* config_values)
{
	DEB_MEMBER_FUNCT();

    //- Transform the module number into a module mask on 8 bits
    //- eg: if modNum = 4, mask_local = 8
    unsigned long mask_local_module = 0x00;
    SET(mask_local_module,(modNum-1));// minus 1 because modNum start at 1
    unsigned long mask_local_chip = 0x00;
    SET(mask_local_chip,(chipId-1));// minus 1 because chipId start at 1

	if(xpci_modLoadAllConfigG(mask_local_module,mask_local_chip, 
			                                    config_values[0],//- CMOS_TP
			                                    config_values[1],//- AMP_TP, 
			                                    config_values[2],//- ITHH, 
			                                    config_values[3],//- VADJ, 
			                                    config_values[4],//- VREF, 
			                                    config_values[5],//- IMFP, 
			                                    config_values[6],//- IOTA, 
			                                    config_values[7],//- IPRE, 
			                                    config_values[8],//- ITHL, 
			                                    config_values[9],//- ITUNE, 
			                                    config_values[10]//- IBUFFER
		                        ) == 0)
	{
		DEB_TRACE() << "loadAllConfigG for module " << modNum  << ", and chip " << chipId << " -> OK" ;
		DEB_TRACE() << "(loadAllConfigG for mask_local_module " << mask_local_module  << ", and mask_local_chip " << mask_local_chip << " )" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in loadAllConfigG!");
	}
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::loadConfigG(unsigned long modNum, unsigned long chipId , unsigned long reg_id, unsigned long reg_value )
{
	DEB_MEMBER_FUNCT();

	//- Transform the module number into a module mask on 8 bits
    //- eg: if modNum = 4, mask_local = 8
    unsigned long mask_local_module = 0x00;
    SET(mask_local_module,(modNum-1));// -1 because modNum start at 1
    unsigned long mask_local_chip = 0x00;
    SET(mask_local_chip,(chipId-1));// -1 because chipId start at 1

	if(xpci_modLoadConfigG(mask_local_module, mask_local_chip, reg_id, reg_value)==0)
	{
		DEB_TRACE() << "loadConfigG: Register 0x" << std::hex << reg_id << ", with value: " << std::dec << reg_value << " -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in loadConfigG!");
	}
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::loadAutoTest(unsigned long known_value)
{
	DEB_MEMBER_FUNCT();

    unsigned int mode = 0; //- 0 -> flat
	if(xpci_modLoadAutoTest(m_modules_mask, known_value, mode)==0)
	{
        DEB_TRACE() << "loadAutoTest with value: " << known_value << " ; in mode: " << mode << " -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in loadAutoTest!");
	}
}

//-----------------------------------------------------
//		Save the config L (DACL) to XPAD RAM
//-----------------------------------------------------
void Camera::saveConfigL(unsigned long modNum, unsigned long calibId, unsigned long chipId, unsigned long curRow, unsigned long* values)
{
    DEB_MEMBER_FUNCT();

    //- Transform the module number into a module mask on 8 bits
    //- eg: if modNum = 4, mask_local = 8
    unsigned long mask_local = 0x00;
    SET(mask_local,(modNum-1));// -1 because modNum start at 1
    //- because start at 1 at high level and 0 at low level
    chipId = chipId - 1;

    //- Call the xpix fonction
    if(xpci_modSaveConfigL(mask_local,calibId,chipId,curRow,(unsigned int*) values) == 0)
	{
        DEB_TRACE() << "saveConfigL for module: " << modNum << " | chip: " << chipId << " | row: " << curRow << " -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in xpci_modSaveConfigL!");
	}
}

//-----------------------------------------------------
//		Save the config G to XPAD RAM
//-----------------------------------------------------
void Camera::saveConfigG(unsigned long modNum, unsigned long calibId, unsigned long reg,unsigned long* values)
{
    DEB_MEMBER_FUNCT();

    //- Transform the module number into a module mask on 8 bits
    //- eg: if modNum = 4, mask_local = 8
    unsigned long mask_local = 0x00;
    SET(mask_local,(modNum-1)); // -1 because modNum start at 1

    //- Call the xpix fonction
    if(xpci_modSaveConfigG(mask_local,calibId,reg,(unsigned int*) values) == 0)
	{
        DEB_TRACE() << "saveConfigG for module: " << modNum << " | reg: " << reg << " -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in xpci_modSaveConfigG!");
	}
}

//-----------------------------------------------------
//		Load the config to detector chips
//-----------------------------------------------------
void Camera::loadConfig(unsigned long modNum, unsigned long calibId)
{
    DEB_MEMBER_FUNCT();

    //- Transform the module number into a module mask on 8 bits
    //- eg: if modNum = 4, mask_local = 8
    unsigned long mask_local = 0x00;
    SET(mask_local,(modNum-1));// -1 because modNum start at 1

    //- Call the xpix fonction
    if(xpci_modDetLoadConfig(mask_local,calibId) == 0)
	{
        DEB_TRACE() << "loadConfig for module: " << modNum << " | calibID: " << calibId << " -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in xpci_modDetLoadConfig!");
	}
}

//-----------------------------------------------------
//		Get the modules config (Local aka DACL)
//-----------------------------------------------------
unsigned short*& Camera::getModConfig()
{
    DEB_MEMBER_FUNCT();

    /*DEB_TRACE() << "Lima::Camera::getModConfig -> xpci_getModConfig -> 1" ;
    //- Call the xpix fonction
    if(xpci_getModConfig(m_modules_mask,m_chip_number,m_dacl) == 0)
	{
        DEB_TRACE() << "Lima::Camera::getModConfig -> xpci_getModConfig -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in xpci_getModConfig!");
	}
    DEB_TRACE() << "Lima::Camera::getModConfig -> xpci_getModConfig -> 2" ;

    return m_dacl;*/
}

//-----------------------------------------------------
//		Reset the detector
//-----------------------------------------------------
void Camera::reset()
{
    DEB_MEMBER_FUNCT();
	unsigned int ALL_MODULES = 0xFF;
    if(xpci_modRebootNIOS(ALL_MODULES) == 0)
	{
        DEB_TRACE() << "reset -> xpci_modRebootNIOS -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in xpci_modRebootNIOS!");
	}
}

//-----------------------------------------------------
//		calibrate over the noise Slow
//-----------------------------------------------------
void Camera::calibrateOTNSlow ( string path)
{
    DEB_MEMBER_FUNCT();

    m_calibration_path = path;

    this->post(new yat::Message(XPAD_DLL_CALIBRATE_OTN_SLOW), kPOST_MSG_TMO);
}

//-----------------------------------------------------
//		calibrate over the noise Medium
//-----------------------------------------------------
void Camera::calibrateOTNMedium ( string path)
{
    DEB_MEMBER_FUNCT();

    m_calibration_path = path;

    this->post(new yat::Message(XPAD_DLL_CALIBRATE_OTN_MEDIUM), kPOST_MSG_TMO);
}

//-----------------------------------------------------
//		calibrate over the noise Fast
//-----------------------------------------------------
void Camera::calibrateOTNFast ( string path)
{
    DEB_MEMBER_FUNCT();

    m_calibration_path = path;

    this->post(new yat::Message(XPAD_DLL_CALIBRATE_OTN_FAST), kPOST_MSG_TMO);
}

//-----------------------------------------------------
//		calibrate over the noise Beam
//-----------------------------------------------------
void Camera::calibrateBeam ( string path, unsigned int texp, unsigned int ithl_max, unsigned int itune,unsigned int imfp)
{
    DEB_MEMBER_FUNCT();

    m_calibration_path	= path;
	m_calib_texp		= texp;
	m_calib_ithl_max	= ithl_max;
	m_calib_itune		= itune;
	m_calib_imfp		= imfp;

    this->post(new yat::Message(XPAD_DLL_CALIBRATE_BEAM), kPOST_MSG_TMO);
}

//-----------------------------------------------------
//		calibrate over the noise
//-----------------------------------------------------
void Camera::calibrateOTN ( string path, unsigned int itune,unsigned int imfp)
{
    DEB_MEMBER_FUNCT();

    m_calibration_path	= path;
	m_calib_itune		= itune;
	m_calib_imfp		= imfp;

    this->post(new yat::Message(XPAD_DLL_CALIBRATE_OTN), kPOST_MSG_TMO);
}

//-----------------------------------------------------
//		upload a calibration
//-----------------------------------------------------
void Camera::uploadCalibration(string path)
{
    DEB_MEMBER_FUNCT();

    m_calibration_path = path;

    this->post(new yat::Message(XPAD_DLL_UPLOAD_CALIBRATION), kPOST_MSG_TMO);
}

//-----------------------------------------------------
//		upload the wait times between images
//-----------------------------------------------------
void Camera::uploadExpWaitTimes(unsigned long *pWaitTime, unsigned size)
{
    DEB_MEMBER_FUNCT();

    //- Check the number of values
    if (size != m_nb_frames)
    {
        throw LIMA_HW_EXC(Error, "Error in uploadExpWaitTimes: number of values does not correspond to number of images");
    }

    if(imxpad_uploadExpWaitTimes(m_modules_mask,(unsigned int*)pWaitTime,size) == 0)
	{
        DEB_TRACE() << "uploadExpWaitTimes -> imxpad_uploadExpWaitTimes -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in imxpad_uploadExpWaitTimes!");
	}
}

//-----------------------------------------------------
//		increment the ITHL
//-----------------------------------------------------
void Camera::incrementITHL()
{
    DEB_MEMBER_FUNCT();

    if(imxpad_incrITHL(m_modules_mask) == 0)
	{
        DEB_TRACE() << "incrementITHL -> imxpad_incrITHL -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in imxpad_incrITHL!");
	}
}

//-----------------------------------------------------
//		decrement the ITHL
//-----------------------------------------------------
void Camera::decrementITHL()
{
    DEB_MEMBER_FUNCT();

    if(imxpad_decrITHL(m_modules_mask) == 0)
	{
        DEB_TRACE() << "decrementITHL -> imxpad_decrITHL -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in imxpad_decrITHL!");
	}
}

//-----------------------------------------------------
//		set Calibration Adjusting Number
//-----------------------------------------------------
void Camera::setCalibrationAdjustingNumber(unsigned calibration_adjusting_number)
{
    DEB_MEMBER_FUNCT();

    m_calibration_adjusting_number = calibration_adjusting_number;
}

//-----------------------------------------------------
//		Set the deadtime
//-----------------------------------------------------
void Camera::setDeadTime(unsigned int dead_time_ms)
{
	DEB_MEMBER_FUNCT();

	//- Parameters checking
	if (m_pixel_depth == B2) //- 16 bits
	{
		if (dead_time_ms * 1000 < 2000)
			throw LIMA_HW_EXC(Error, "deadtime should be at least 2000 usec in 16 bits");
	}
	else //- 32 bits
	{
		if (dead_time_ms * 1000 < 5000)
			throw LIMA_HW_EXC(Error, "deadtime should be at least 5000 usec in 32 bits");
	}

	m_time_between_images_usec  = dead_time_ms * 1000; //- Temps entre chaque image
}

//-----------------------------------------------------
//		Set the init time
//-----------------------------------------------------
void Camera::setInitTime(unsigned int init_time_ms)
{
	DEB_MEMBER_FUNCT();

	m_time_before_start_usec  = init_time_ms * 1000; //- Temps initial
}

//-----------------------------------------------------
//		Set the shutter time
//-----------------------------------------------------
void Camera::setShutterTime(unsigned int shutter_time_ms)
{
	DEB_MEMBER_FUNCT();

	m_shutter_time_usec  = shutter_time_ms * 1000;
}

//-----------------------------------------------------
//		Set the overflow time
//-----------------------------------------------------
void Camera::setOverflowTime(unsigned int overflow_time)
{
	DEB_MEMBER_FUNCT();

	if (overflow_time < 4000)
		throw LIMA_HW_EXC(Error, "overflow should be at least 4000 usec");

	m_ovf_refresh_time_usec  = overflow_time;
}

//-----------------------------------------------------
//		Set the n parameter
//-----------------------------------------------------
void Camera::setNParameter(unsigned int n)
{
	DEB_MEMBER_FUNCT();

	m_specific_param_n  = n;
}

//-----------------------------------------------------
//		Set the p parameter
//-----------------------------------------------------
void Camera::setPParameter(unsigned int p)
{
	DEB_MEMBER_FUNCT();

	m_specific_param_p  = p;
}

//-----------------------------------------------------
//		Set the busy out selection
//-----------------------------------------------------
void Camera::setBusyOutSel(unsigned int busy_out_sel)
{
	DEB_MEMBER_FUNCT();

	m_busy_out_sel = busy_out_sel;
}

//-----------------------------------------------------
//		enable/disable geom correction
//-----------------------------------------------------
void Camera::setGeomCorrection(bool geom_corr)
{
	DEB_MEMBER_FUNCT();

	if((m_xpad_model != IMXPAD_S540) || (m_acquisition_type != Camera::ASYNC))
		throw LIMA_HW_EXC(Error, "Geometrical correction is only available for S540 Xpad in Asynchrone mode");

	m_geom_corr  = (unsigned int)geom_corr;

	if (m_maximage_size_cb_active) 
	{
		// only if the callaback is active
		// inform lima about the size change
		ImageType pixel_depth;
		Size image_size;
		getPixelDepth(pixel_depth); //- ie Bpp16 ...
		getImageSize(image_size); //- size of image
		maxImageSizeChanged(image_size, pixel_depth);       
	}
}

//-----------------------------------------------------
//		enable/disable double pixel correction
//-----------------------------------------------------
void Camera::setDoublePixelCorrection(bool doublepixel_corr)
{
	DEB_MEMBER_FUNCT();

	if(m_xpad_model != IMXPAD_S140)
		throw LIMA_HW_EXC(Error, "Double pixel correction is only available for S140 Xpad");

	m_doublepixel_corr  = doublepixel_corr;

	if (m_maximage_size_cb_active) 
	{
		// only if the callaback is active
		// inform lima about the size change
		ImageType pixel_depth;
		Size image_size;
		getPixelDepth(pixel_depth); //- ie Bpp16 ...
		getImageSize(image_size); //- size of image
		maxImageSizeChanged(image_size, pixel_depth);       
	}
}

//-----------------------------------------------------
//		Set GeneralPurpose Params
//-----------------------------------------------------
void Camera::setNormalizationFactor(double norm_factor)
{
	DEB_MEMBER_FUNCT();

	m_norm_factor = norm_factor;
}

//-----------------------------------------------------
//		Set GeneralPurpose Params
//-----------------------------------------------------
void Camera::setGeneralPurposeParams( unsigned int GP1, unsigned int GP2, unsigned int GP3, unsigned int GP4)
{
	DEB_MEMBER_FUNCT();

	m_specific_param_GP1		= GP1;
	m_specific_param_GP2		= GP2;
	m_specific_param_GP3		= GP3;
	m_specific_param_GP4		= GP4;
}

//-----------------------------------------------------
//		decrement the ITHL
//-----------------------------------------------------
void Camera::xpixDebug(bool enable)
{
    DEB_MEMBER_FUNCT();

    xpci_debugMsg(enable);
}

//-----------------------------------------------------
//		setMaxImageSizeCallbackActive
//-----------------------------------------------------
void Camera::setMaxImageSizeCallbackActive(bool cb_active)
{
    DEB_MEMBER_FUNCT();

    m_maximage_size_cb_active = cb_active;
	DEB_TRACE() << "m_maximage_size_cb_active = " << m_maximage_size_cb_active ;
}

//---------------------------------------------------------------------------
//		Double Pixel Correction for S140 Xpad (cf J Perez and C Mocuta)
//---------------------------------------------------------------------------
template<typename T> 
void Camera::doublePixelCorrection(T* image_to_correct, T corrected_image[][S140_CORRECTED_NB_COLUMN])
{
    DEB_MEMBER_FUNCT();

	DEB_TRACE() << "m_image_size.getWidth() = " << m_image_size.getWidth() ;
	DEB_TRACE() << "m_image_size.getHeight() = " << m_image_size.getHeight() ;

	//- double Pixel Correction algo (from J. Perez)
	//- copy one_image into I1 (for easy access)
	m_start_sec = Timestamp::now();
	int cpt=0;
	T I1[I1_ROW][I1_COLUMN];
	for (int j=0;j<I1_ROW;j++)
		for(int i =0 ;i<I1_COLUMN ;i++)
			I1[j][i]= ((T*)image_to_correct)[cpt++]; 
	DEB_TRACE() << "Time for copying one_image into I1 = " << Timestamp::now() - m_start_sec; //- measured = 250 ns

	DEB_TRACE() << "I1[12][31] = " << I1[12][31]; 
	DEB_TRACE() << "I1[12][29] = " << I1[12][31]; 
	
	m_start_sec = Timestamp::now();
	T I2[I2_ROW][I2_COLUMN];
	//- On remplit I2
	for(int j = 0; j<I2_ROW; j++)
	{
		I2[j][0] = I1[j][0]; //- copy 1ere colonne de I1 dans I2
		//- DEBUG:
		if (j < 10)
		{
			DEB_TRACE() << "I1[j][0] = " << I1[j][0]; 
			DEB_TRACE() << "I1[j][1] = " << I1[j][1]; 
		}
		for (int chip = 1; chip<=6; chip++) // pour tous les chips sauf le dernier
		{   
			for (int i = (chip-1)*83+1; i <= chip*83-5;i++) // 
				I2[j][i] = I1[j][i - 3*(chip-1)];

			int I1left	=I1[j][(chip*80-1)];
			int I1right	=I1[j][chip*80];
			int i;
			for (i = chip*83-4; i <= chip*83-3; i++) // 
				I2[j][i] = round(I1left / m_norm_factor) ;

			I2[j][chip*83-2] = I1left + I1right - 2*(round(I1left / m_norm_factor) + round(I1right / m_norm_factor));

			for (i = chip*83-1; i <= chip*83; i++) // 
				I2[j][i] = round(I1right / m_norm_factor);
		}
		for (int i = 499; i <= 577; i++) // 
			I2[j][i] = I1[j][i-18];		

		if (j < 10)
		{
			DEB_TRACE() << "I2[j][0] = " << I2[j][0]; 
			DEB_TRACE() << "I2[j][1] = " << I2[j][1]; 
		}
	}
	
	//- On remplit corrected_image
	for (int i = 0; i < S140_CORRECTED_NB_COLUMN; i++) //
	{
		for(int j = 0; j <=118; j++)
			corrected_image[j][i] = I2[j][i];

		int I2up	=I2[119][i];
		int I2down	=I2[120][i];
		int j;
		for(j = 119; j <= 120; j++)
			corrected_image[j][i] = round(I2up / m_norm_factor);

		corrected_image[121][i] = I2up + I2down - 2*(round(I2up / m_norm_factor)+round(I2down / m_norm_factor));

		for(j = 122; j<=123;j++)
			corrected_image[j][i] = round(I2down / m_norm_factor);
		for(j = 124; j<=242;j++)
			corrected_image[j][i] = I2[j][i-3];
	}
	DEB_TRACE() << "Time for the double pixel algo = " << Timestamp::now() - m_start_sec;//- measured = 650 ns
}
