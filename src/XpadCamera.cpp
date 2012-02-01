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


//- Const.
static const int 	BOARDNUM 	= 0;


//---------------------------
//- Ctor
//---------------------------
Camera::Camera(): 	m_buffer_cb_mgr(m_buffer_alloc_mgr),
m_buffer_ctrl_mgr(m_buffer_cb_mgr),
m_modules_mask(0x00),
m_chip_number(7),
m_exp_time(1000),
m_pixel_depth(B4),
m_trigger_type(INTERN_GATE),
m_nb_frames(1)
{
	DEB_CONSTRUCTOR();
	m_status = Camera::Ready;

	//- Hardcoded temporarly
	m_time_unit				= MILLISEC_GATE; // MICROSEC_GATE; MILLISEC_GATE; SECONDS_GATE

	//- FParameters
	m_fparameter_deadtime	= 0;
	m_fparameter_init		= 0;
	m_fparameter_shutter	= 0;
	m_fparameter_ovf		= 0;
	m_fparameter_mode		= 0;
	m_fparameter_n			= 0;
	m_fparameter_p			= 0;
	m_fparameter_GP1		= 0;
	m_fparameter_GP2		= 0;
	m_fparameter_GP3		= 0;
	m_fparameter_GP4		= 0;

    m_acquisition_type		= Camera::SLOW_32; // Slow B4

	//-------------------------------------------------------------
	//- Get Modules that are ready
	if (xpci_modAskReady(&m_modules_mask) == 0)
	{
		DEB_TRACE() << "Ask modules that are ready: OK (modules mask = " << std::hex << m_modules_mask << ")" ;
		m_module_number = xpci_getModNb(m_modules_mask);
        
		if (m_module_number !=0)
		{
			DEB_TRACE() << "--> Number of Modules 		 = " << m_module_number ;			
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


	//ATTENTION: Modules should be ordered! 
	m_image_size = Size(80 * m_chip_number ,120 * m_module_number); //-MODIF-NL-ICA
	DEB_TRACE() << "--> Number of chips 		 = " << std::dec << m_chip_number ;
	DEB_TRACE() << "--> Image width 	(pixels) = " << std::dec << m_image_size.getWidth() ;
	DEB_TRACE() << "--> Image height	(pixels) = " << std::dec << m_image_size.getHeight() ;

    //- allocate the dacl array
    m_dacl = new unsigned short[m_image_size.getWidth() * m_image_size.getHeight()];
}

Camera::Camera(const Camera &other_cam) :
  m_buffer_cb_mgr(m_buffer_alloc_mgr),
  m_buffer_ctrl_mgr(m_buffer_cb_mgr)
{
  *this = other_cam;
}

Camera& Camera::operator=(const Camera& other_cam)
{
  m_nb_frames 			= other_cam.m_nb_frames;
  m_image_size 			= other_cam.m_image_size;
  m_pixel_depth 		= other_cam.m_pixel_depth;
  m_trigger_type 		= other_cam.m_trigger_type;
  m_exp_time 			= other_cam.m_exp_time;
  m_exp_time_sec 		= other_cam.m_exp_time_sec;
  
  
  //---------------------------------
  //- xpad stuff 
  m_acquisition_type 		= other_cam.m_acquisition_type;
  m_modules_mask 		= other_cam.m_modules_mask;
  m_module_number 		= other_cam.m_module_number;
  m_chip_number 		= other_cam.m_chip_number;
  m_full_image_size_in_bytes 	= other_cam.m_full_image_size_in_bytes;
  m_time_unit 			= other_cam.m_time_unit;
  m_all_config_g 		= other_cam.m_all_config_g;
  
        //- FParameters
  m_fparameter_deadtime 	= other_cam.m_fparameter_deadtime;
  m_fparameter_init 		= other_cam.m_fparameter_init;
  m_fparameter_shutter 		= other_cam.m_fparameter_shutter;
  m_fparameter_ovf 		= other_cam.m_fparameter_ovf;
  m_fparameter_mode 		= other_cam.m_fparameter_mode;
  m_fparameter_n 		= other_cam.m_fparameter_n;
  m_fparameter_p 		= other_cam.m_fparameter_p;
  m_fparameter_GP1 		= other_cam.m_fparameter_GP1;
  m_fparameter_GP2 		= other_cam.m_fparameter_GP2;
  m_fparameter_GP3 		= other_cam.m_fparameter_GP3;
  m_fparameter_GP4 		= other_cam.m_fparameter_GP4;
  
        //---------------------------------
  m_status 			= other_cam.m_status;
  m_stop_asked 			= other_cam.m_stop_asked;
  
  return *this;
}

//---------------------------
//- Dtor
//---------------------------
Camera::~Camera()
{
	DEB_DESTRUCTOR();

    delete [] m_dacl;
}

//---------------------------
//- Camera::start()
//---------------------------
void Camera::start()
{
	DEB_MEMBER_FUNCT();

    //- Check the image type compatibility
    if((m_pixel_depth == B2 && m_acquisition_type == Camera::SLOW_32) || (m_pixel_depth == B4 && m_acquisition_type != Camera::SLOW_32))
    {
        DEB_ERROR() << "Image type does not correspond to acquisition type: 16 bits vs 32 bits (check properties)" ;
		throw LIMA_HW_EXC(Error, "Image type does not correspond to acquisition type: 16 bits vs 32 bits (check properties)");
    }

    //- Check integration time compatibility
    if((m_acquisition_type == Camera::FAST_16 || m_acquisition_type == Camera::FAST_ASYNC_16 ) && (m_exp_time_sec > 0.100))
    {
        DEB_ERROR() << "For exposure above 100 ms, you cannot use fast acquisition types, use instead slow acquisition types" ;
		throw LIMA_HW_EXC(Error, "For exposure above 100 ms, you cannot use fast acquisition types, use instead slow acquisition types");
    }

    //-	((80 colonnes * 7 chips) * taille du pixel + 6 word de control ) * 120 lignes * nb_modules
	if (m_pixel_depth == B2)
	{
		m_full_image_size_in_bytes = ((80 * m_chip_number) * 2 + 6*2)  * 120 * m_module_number;
	} 
	else if(m_pixel_depth == B4)
	{
		m_full_image_size_in_bytes = ((80 * m_chip_number) * 4 + 6*2)  * 120 * m_module_number;
	} 

	m_stop_asked = false;

	DEB_TRACE() << "m_acquisition_type = " << m_acquisition_type ;

    if(m_acquisition_type == Camera::SLOW_16)
	{
		//- Post XPAD_DLL_START_SLOW_B2_MSG msg (aka getOneImage 16)
		this->post(new yat::Message(XPAD_DLL_START_SLOW_B2_MSG), kPOST_MSG_TMO);
	}
	else if (m_acquisition_type == Camera::FAST_16)
	{
		//- Post XPAD_DLL_START_FAST_MSG msg (aka getImgSeq)
		this->post(new yat::Message(XPAD_DLL_START_FAST_MSG), kPOST_MSG_TMO);
	}
	else if (m_acquisition_type == Camera::SLOW_32)
	{
		//- Post XPAD_DLL_START_SLOW_MSG_B4 msg (aka getOneImage 32)
		this->post(new yat::Message(XPAD_DLL_START_SLOW_B4_MSG), kPOST_MSG_TMO);
	}
	else if (m_acquisition_type == Camera::FAST_ASYNC_16)
	{
		//- Post XPAD_DLL_START_FAST_ASYNC_MSG msg (aka getImgSeqAs)
		this->post(new yat::Message(XPAD_DLL_START_FAST_ASYNC_MSG), kPOST_MSG_TMO);
	}
	else
	{
		DEB_ERROR() << "Acquisition type not supported" ;
		throw LIMA_HW_EXC(Error, "Acquisition type not supported: possible values are:\n0->SLOW 16 bits\n1->FAST 16 bits\n2->SLOW 32 bits\n3->FAST ASYNC 16 bits");
	}
}

//---------------------------
//- Camera::stop()
//---------------------------
void Camera::stop()
{
	DEB_MEMBER_FUNCT();

	m_status = Camera::Ready;
	m_stop_asked = true;
}

//-----------------------------------------------------
//- Camera::getImageSize(Size& size)
//-----------------------------------------------------
void Camera::getImageSize(Size& size)
{
	DEB_MEMBER_FUNCT();

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
		break;

	case Bpp32:
		m_pixel_depth = B4;
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
	switch( m_pixel_depth )
	{
	case B2:
		pixel_depth = Bpp16;
		break;

	case B4:
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
	type = "PCIe-3.2";	
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setMaxImageSizeCallbackActive(bool cb_active)
{  
	m_mis_cb_act = cb_active;
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
		m_trigger_type = INTERN_GATE; //- IntTrig
		break;
	case ExtTrigSingle:
		m_trigger_type = TRIGGER_IN; //- ExtTrigSingle
		break;
	case ExtGate:
		m_trigger_type = EXTERN_GATE; //- ExtGate
		break;
	default:
		DEB_ERROR() << "Error: Trigger mode unsupported: only INTERN_GATE, EXTERN_TRIG or EXTERN_GATE" ;
		throw LIMA_HW_EXC(Error, "Trigger mode unsupported: only INTERN_GATE, EXTERN_TRIG or EXTERN_GATE");
		break;
	}
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getTrigMode(TrigMode& mode)
{

	DEB_MEMBER_FUNCT();
	switch( m_trigger_type )
	{
	case INTERN_GATE:
		mode = IntTrig;
		break;
	case TRIGGER_IN:
		mode = ExtTrigSingle;
		break;
	case EXTERN_GATE:
		mode = ExtGate;
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

    m_exp_time_sec = exp_time_sec;

	if (exp_time_sec < 0.001)
	{
		m_time_unit = MICROSEC_GATE;
	}
	else if ((exp_time_sec >= 0.001) && (exp_time_sec <= 32))
	{
		m_time_unit = MILLISEC_GATE;
	}
	else if (exp_time_sec > 32)
	{
		m_time_unit = SECONDS_GATE;		
	}

	//- 
	switch(m_time_unit)
	{
	case MICROSEC_GATE:
		m_exp_time = (unsigned) (exp_time_sec * 1000000);
		break;

	case MILLISEC_GATE:
		m_exp_time = (unsigned) (exp_time_sec * 1000);
		break;

	case SECONDS_GATE:
		m_exp_time = (unsigned) (exp_time_sec * 1);
		break;
	}
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::getExpTime(double& exp_time_sec)
{
	DEB_MEMBER_FUNCT();

	double m_exp_time_temp = m_exp_time;
	
	switch(m_time_unit)
	{
	case MICROSEC_GATE:
		exp_time_sec = (double) (m_exp_time_temp / 1000000);
		break;

	case MILLISEC_GATE:
		exp_time_sec = (double) (m_exp_time_temp / 1000);
		break;

	case SECONDS_GATE:
		exp_time_sec = (double) (m_exp_time_temp / 1);
		break;
	}

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
		case XPAD_DLL_START_SLOW_B2_MSG:
			{
				DEB_TRACE() <<"Camera::->XPAD_DLL_START_SLOW_B2_MSG";       

				m_status = Camera::Exposure;

				for( int i=0 ; i < m_nb_frames ; i++ )
				{
					if (m_stop_asked == true)
					{
						DEB_TRACE() <<"Stop asked: exit without allocating new images..." ;
						m_status = Camera::Ready;
						return;
					}


					pOneImage = new uint16_t[ m_full_image_size_in_bytes / 2 ]; //- Divided by 2 because of uint16

					if (xpci_getOneImage(	m_pixel_depth,
						m_modules_mask,
						m_chip_number,
						(uint16_t *)pOneImage,
						m_trigger_type,
						m_exp_time,
						m_time_unit,
						30000)==-1)
					{
						DEB_ERROR()<< "Error: xpci_getOneImage as returned an error..." ;
       
					    DEB_TRACE() << "Freeing image";
					    delete[] pOneImage;			

					    m_status = Camera::Fault;
					    throw LIMA_HW_EXC(Error, "xpci_getOneImage as returned an error...");
					}

					m_status = Camera::Readout;

					DEB_TRACE() <<"Image# "<< i << " acquired" ;

					//- clean the image and call new frame for each frame
					StdBufferCbMgr& buffer_mgr = m_buffer_cb_mgr;
					DEB_TRACE() <<"-> clean acquired image and publish it through newFrameReady()";

					int buffer_nb, concat_frame_nb;
					buffer_mgr.setStartTimestamp(Timestamp::now());
					buffer_mgr.acqFrameNb2BufferNb(i, buffer_nb, concat_frame_nb);

					uint16_t *ptr = (uint16_t*)(buffer_mgr.getBufferPtr(buffer_nb,concat_frame_nb));
					//clean the ptr with zero memory, pixels of a not available module are set to "0" 
					memset((uint16_t *)ptr,0,m_image_size.getWidth() * m_image_size.getHeight() * 2);

					//iterate on all lines of all modules returned by xpix API 
					int k=0;
					for(int j = 0; j < 120 * m_module_number; j++) 
					{
						uint16_t	OneLine[6+80*m_chip_number];

						//copy entire line with its header and footer
						for(k = 0; k < (6+80*m_chip_number); k++)
							OneLine[k] = pOneImage[j*(6+80*m_chip_number)+k];

						//compute "offset line" where to copy OneLine[] in the *ptr, to ensure that the lines are copied in order of modules
						int offset = ((120*(OneLine[1]-1))+(OneLine[4]-1)); 

						//copy cleaned line in the lima buffer
						for(k = 0; k < (80*m_chip_number); k++)
							ptr[(offset*80*m_chip_number)+k] = OneLine[5+k];
					}

					DEB_TRACE() << "image# " << i <<" cleaned" ;
					HwFrameInfoType frame_info;
					frame_info.acq_frame_nb = i;
					//- raise the image to lima
					buffer_mgr.newFrameReady(frame_info);

					DEB_TRACE() <<"free image pointer";
					delete[] pOneImage;
				}

				m_status = Camera::Ready;
			}
			break;
			//-----------------------------------------------------    
		case XPAD_DLL_START_SLOW_B4_MSG:
			{
				DEB_TRACE() <<"Camera::->XPAD_DLL_START_SLOW_B4_MSG";       

				m_status = Camera::Exposure;

				for( int i=0 ; i < m_nb_frames ;  )
				{
					if (m_stop_asked == true)
					{
						DEB_TRACE() <<"Stop asked: exit without allocating new images..." ;
						m_status = Camera::Ready;
						return;
					}

					pOneImage = new uint16_t[ m_full_image_size_in_bytes/2 ];

					if (xpci_getOneImage(	m_pixel_depth,
						m_modules_mask,
						m_chip_number,
						(uint16_t *)pOneImage,
						m_trigger_type,
						m_exp_time,
						m_time_unit,
						m_exp_time * 1050 //- timeout: cf Fred B.
						)==-1)
					{
						DEB_ERROR()<< "Error: xpci_getOneImage as returned an error..." ;
       
					    DEB_TRACE() << "Freeing image";
					    delete[] pOneImage;			

					    m_status = Camera::Fault;
					    throw LIMA_HW_EXC(Error, "xpci_getOneImage as returned an error...");
					}

					m_status = Camera::Readout;

					DEB_TRACE() <<"Image# "<< i << " acquired" ;

					//- clean the image and call new frame for each frame
					StdBufferCbMgr& buffer_mgr = m_buffer_cb_mgr;
					DEB_TRACE() <<"-> cleanning acquired image and publish it through newFrameReady()";

					int buffer_nb, concat_frame_nb;
					buffer_mgr.setStartTimestamp(Timestamp::now());
					buffer_mgr.acqFrameNb2BufferNb(i, buffer_nb, concat_frame_nb);

					uint32_t *ptr = (uint32_t*)(buffer_mgr.getBufferPtr(buffer_nb,concat_frame_nb));
                    
                    //clean the ptr with zero memory, pixels of a not available module are set to "0" 
					memset((uint32_t *)ptr,0,m_image_size.getWidth() * m_image_size.getHeight() * 4);

                    //iterate on all lines of all modules returned by xpix API 
					int k=0;
					for(int j = 0; j < 120 * m_module_number; j++) 
					{
						uint16_t	OneLine[6+(80*m_chip_number)*2];
						uint32_t	OneLinePix[80*m_chip_number];

						//copy entire line with its header and footer
						for(k = 0; k < (6+(80*m_chip_number)*2); k++)
							OneLine[k] = pOneImage[j*(6+(80*m_chip_number)*2)+k];
                 
						//copy entire line without header&footer into OneLinePix
						memcpy((uint32_t *)OneLinePix,(uint16_t *)(&OneLine[5]),80*m_chip_number*4);
                        
						//compute "offset line" where to copy OneLine[] in the *ptr, to ensure that the lines are copied in order of modules
						int offset = ((120*(OneLine[1]-1))+(OneLine[4]-1));
                        
						//copy cleaned line in the lima buffer
						for(k = 0; k < (80*m_chip_number); k++)
							ptr[(offset*80*m_chip_number)+k] = OneLinePix[k];
					}

					DEB_TRACE() << "Image# " << i <<" cleaned" ;
					HwFrameInfoType frame_info;
					frame_info.acq_frame_nb = i;

					//- raise the image to lima
					buffer_mgr.newFrameReady(frame_info);

					DEB_TRACE() <<"free image pointer";
					delete[] pOneImage;

					i++;
				}

				m_status = Camera::Ready;
			}
			break;

			//-----------------------------------------------------    
		case XPAD_DLL_START_FAST_MSG:	
			{
				DEB_TRACE() <<"Camera::->XPAD_DLL_START_FAST_MSG";

				//- A mettre dans le prepareAcq?
				// allocate multiple buffers
				DEB_TRACE() <<"allocating images array (" << m_nb_frames << " images)";
				pSeqImage = new uint16_t* [ m_nb_frames ];

				DEB_TRACE() <<"allocating every image pointer of the images array (1 image full size = "<< m_full_image_size_in_bytes << ") ";
				for( int i=0 ; i < m_nb_frames ; i++ )
					pSeqImage[i] = new uint16_t[ m_full_image_size_in_bytes / 2 ];

				m_status = Camera::Exposure;

				//- Start the img sequence
				DEB_TRACE() <<"start acquiring a sequence of images";	
				cout << " ============================================== " << endl;
				cout << "Parametres pour getImgSeq: "<< endl;
				cout << "m_pixel_depth 	= "<< m_pixel_depth << endl;
				cout << "m_modules_mask = "<< m_modules_mask << endl;
				cout << "m_chip_number 	= "<< m_chip_number << endl;
				cout << "m_trigger_type = "<< m_trigger_type << endl;
				cout << "m_exp_time 	= "<< m_exp_time << endl;
				cout << "m_time_unit 	= "<< m_time_unit << endl;
				cout << "m_nb_frames 	= "<< m_nb_frames << endl;

				if ( xpci_getImgSeq(	m_pixel_depth, 
					m_modules_mask,
					m_chip_number,
					m_trigger_type,
					m_exp_time,
					m_time_unit,
					m_nb_frames,
					(void**)pSeqImage,
					8000) == -1)
				{
					DEB_ERROR() << "Error: getImgSeq as returned an error..." ;

					DEB_TRACE() << "Freeing every image pointer of the images array";
					for(int i=0 ; i < m_nb_frames ; i++)
						delete[] pSeqImage[i];

					DEB_TRACE() << "Freeing images array";
					delete[] pSeqImage;			

					m_status = Camera::Fault;
                    throw LIMA_HW_EXC(Error, "xpci_getImgSeq as returned an error...");
				}

				m_status = Camera::Readout;

				DEB_TRACE() 	<< "\n#######################"
					<< "\nall images are acquired"
					<< "\n#######################" ;

				//- ATTENTION :
				//- Xpix acquires a buffer sized according to m_module_number.
				//- Displayed/Lima image will be ALWAYS 560*960, even if some modules are not availables ! 
				//- Image zone where a module is not available will be set to "zero"

				//XPIX LIB buffer						//Device requested buffer
				//--------------						//--------------
				//line 1 	mod1						//line 1 	mod1
				//line 1 	mod2						//line 2 	mod1
				//line 1 	mod3						//line 3 	mod1
				//...									//...
				//line 1	mod8						//line 120	mod1
				//--------------						//--------------
				//line 2 	mod1						//line 1 	mod2
				//line 2 	mod2						//line 2 	mod2
				//line 2 	mod3						//line 3 	mod2
				//...									//...
				//line 2	mod4						//line 120	mod2
				//--------------						//--------------
				//...									//...
				//--------------						//--------------
				//line 120 	mod1						//line 1 	mod8
				//line 120 	mod2						//line 2 	mod8
				//line 120 	mod3						//line 3 	mod8
				//...									//...
				//line 120	mod8						//line 120	mod8

				int	i=0, j=0, k=0;

				//- clean each image and call new frame for each frame
				StdBufferCbMgr& buffer_mgr = m_buffer_cb_mgr;
				DEB_TRACE() <<"Cleanning each acquired image and publish it through newFrameReady()";
				for(i=0; i<m_nb_frames; i++)
				{
					pOneImage = pSeqImage[i];

					int buffer_nb, concat_frame_nb;
					buffer_mgr.setStartTimestamp(Timestamp::now());
					buffer_mgr.acqFrameNb2BufferNb(i, buffer_nb, concat_frame_nb);

					uint16_t *ptr = (uint16_t*)(buffer_mgr.getBufferPtr(buffer_nb,concat_frame_nb));
					//clean the ptr with zero memory, pixels of a not available module are set to "0" 
					memset((uint16_t *)ptr,0,m_image_size.getWidth() * m_image_size.getHeight() * 2);

					//iterate on all lines of all modules returned by xpix API 
					for(j = 0; j < 120 * m_module_number; j++) 
					{
						uint16_t	OneLine[6+80*m_chip_number];

						//copy entire line with its header and footer
						for(k = 0; k < (6+80*m_chip_number); k++)
							OneLine[k] = pOneImage[j*(6+80*m_chip_number)+k];

						//compute "offset line" where to copy OneLine[] in the *ptr, to ensure that the lines are copied in order of modules
						int offset = ((120*(OneLine[1]-1))+(OneLine[4]-1)); 

						//copy cleaned line in the lima buffer
						for(k = 0; k < (80*m_chip_number); k++)
							ptr[(offset*80*m_chip_number)+k] = OneLine[5+k];
					}

					DEB_TRACE() << "image# " << i <<" cleaned" ;
					HwFrameInfoType frame_info;
					frame_info.acq_frame_nb = i;
					//- raise the image to lima
					buffer_mgr.newFrameReady(frame_info);
				}

				DEB_TRACE() <<"Freeing every image pointer of the images array";
				for(i=0 ; i < m_nb_frames ; i++)
					delete[] pSeqImage[i];
				DEB_TRACE() <<"Freeing images array";
				delete[] pSeqImage;
				m_status = Camera::Ready;
				DEB_TRACE() <<"m_status is Ready";
			}
			break;
		

//-----------------------------------------------------    
	  case XPAD_DLL_START_FAST_ASYNC_MSG:	
		  {
			  DEB_TRACE() <<"Camera::->XPAD_DLL_START_FAST_ASYNC_MSG";

			  //- A mettre dans le prepareAcq?
			  // allocate multiple buffers
			  DEB_TRACE() <<"allocating images array (" << m_nb_frames << " images)";
			  pSeqImage = new uint16_t* [ m_nb_frames ];

			  DEB_TRACE() <<"allocating every image pointer of the images array (1 image full size = "<< m_full_image_size_in_bytes << ") ";
			  for( int i=0 ; i < m_nb_frames ; i++ )
				  pSeqImage[i] = new uint16_t[ m_full_image_size_in_bytes / 2 ];

			  m_status = Camera::Exposure;

			  //- Start the img sequence
			  DEB_TRACE() <<"start acquiring a sequence of images";	
			  cout << " ============================================== " << endl;
			  cout << "Parametres pour getImgSeq: "<< endl;
			  cout << "m_pixel_depth 	= "<< m_pixel_depth << endl;
			  cout << "m_modules_mask = "<< m_modules_mask << endl;
			  cout << "m_chip_number 	= "<< m_chip_number << endl;
			  cout << "m_trigger_type = "<< m_trigger_type << endl;
			  cout << "m_exp_time 	= "<< m_exp_time << endl;
			  cout << "m_time_unit 	= "<< m_time_unit << endl;
			  cout << "m_nb_frames 	= "<< m_nb_frames << endl;


			  //- Start the acquisition in Async mode
			  if ( xpci_getImgSeqAs(	m_pixel_depth, 
				  m_modules_mask,
				  m_chip_number,
				  NULL, //- callback fct not used
				  10000,//FL: set 10 sec global timeout ??
				  m_trigger_type,
				  m_exp_time,
				  m_time_unit,
				  m_nb_frames,
				  (void**)pSeqImage,
				  8000,
				  NULL) //- user Parameter not used
				  == -1)

			  {
				  DEB_ERROR() << "Error: getImgSeq as returned an error..." ;

				  DEB_TRACE() << "Freeing every image pointer of the images array";
				  for(int i=0 ; i < m_nb_frames ; i++)
					  delete[] pSeqImage[i];

				  DEB_TRACE() << "Freeing images array";
				  delete[] pSeqImage;			

				  m_status = Camera::Fault;
                  throw LIMA_HW_EXC(Error, "xpci_getImgSeqAs as returned an error...");
			  }

			  m_status = Camera::Readout;

			  int nb_images_aquired_before = 0;
			  int nb_images_acquired = 0;
			  int current_treated_image = 0;
				
                          size_t n1 = 6 + 80 * m_chip_number;
                          size_t n2 = 80 * m_chip_number;
			  uint16_t OneLine[n1];

			  //- While acquisition is running
			  while(xpci_asyncReadStatus() || current_treated_image < m_nb_frames)
			  {
				  //- for debug
				  //yat::ThreadingUtilities::sleep(0,50000000); //50 ms

				  //DEB_TRACE() << "----------------------------";
				  //DEB_TRACE() << "Nb images treated			= " << current_treated_image;
			          do
				  {
				    nb_images_acquired = xpci_getGotImages();
				    if ( nb_images_acquired == nb_images_aquired_before ) 
					yat::ThreadingUtilities::sleep(0,48000000); //48 ms
				    else 
					break;
				  }
				  while (1);

				 // DEB_TRACE() << "Nb images acquired			= " << nb_images_acquired;
				  //DEB_TRACE() << "Nb images acquired before		= " << nb_images_aquired_before;

				  //- ATTENTION : //-MODIF-NL-ICA: THSI IS EXACTLY WHAT WE DON'T WANT!!!!!
				  //- Xpix acquires a buffer sized according to m_module_number.
				  //- Displayed/Lima image will be ALWAYS 560*960, even if some modules are not availables ! 
				  //- Image zone where a module is not available will be set to "zero"

				  //XPIX LIB buffer						//Device requested buffer
				  //--------------						//--------------
				  //line 1 	mod1						//line 1 	mod1
				  //line 1 	mod2						//line 2 	mod1
				  //line 1 	mod3						//line 3 	mod1
				  //...									//...
				  //line 1	mod8						//line 120	mod1
				  //--------------						//--------------
				  //line 2 	mod1						//line 1 	mod2
				  //line 2 	mod2						//line 2 	mod2
				  //line 2 	mod3						//line 3 	mod2
				  //...									//...
				  //line 2	mod4						//line 120	mod2
				  //--------------						//--------------
				  //...									//...
				  //--------------						//--------------
				  //line 120 	mod1						//line 1 	mod8
				  //line 120 	mod2						//line 2 	mod8
				  //line 120 	mod3						//line 3 	mod8
				  //...									//...
				  //line 120	mod8						//line 120	mod8

				  int	i=0, j=0, k=0;
				  //- clean each image and call new frame for each frame
				  StdBufferCbMgr& buffer_mgr = m_buffer_cb_mgr;
				  //DEB_TRACE() <<"Cleanning each acquired image and publish it through newFrameReady ...";
				  for( i = nb_images_aquired_before; i<nb_images_acquired; i++ )
				  {
					  nb_images_aquired_before = nb_images_acquired;

					  current_treated_image++;

					  pOneImage = pSeqImage[i];

					  int buffer_nb, concat_frame_nb;
					  buffer_mgr.setStartTimestamp(Timestamp::now());

					  buffer_mgr.acqFrameNb2BufferNb(i, buffer_nb, concat_frame_nb);

					  uint16_t *ptr = (uint16_t*)(buffer_mgr.getBufferPtr(buffer_nb,concat_frame_nb));

					  //clean the ptr with zero memory, pixels of a not available module are set to "0" 
					  memset((uint16_t *)ptr,0,m_image_size.getWidth() * m_image_size.getHeight() * 2);
					  // DEB_TRACE() << "---- Niveau ------- 5 ---- ";

					  //iterate on all lines of all modules returned by xpix API 
					  for(j = 0; j < 120 * m_module_number; j++) 
					  {
						  //DEB_TRACE() << "---- Niveau ------- 5.1 ---- ";
						  ::memset(OneLine, 0, n1 * sizeof(uint16_t));

						  //copy entire line with its header and footer
						  //DEB_TRACE() << "---- Niveau ------- 5.2 ---- ";
						  
						  for ( k = 0; k < n1; k++ )
							  OneLine[k] = pOneImage[j * n1 + k];

						  //compute "offset line" where to copy OneLine[] in the *ptr, to ensure that the lines are copied in order of modules
						  //DEB_TRACE() << "---- Niveau ------- 5.3 ---- ";
						  int offset = ((120 * (OneLine[1] - 1)) + (OneLine[4] - 1)); 

						  //copy cleaned line in the lima buffer
						  //DEB_TRACE() << "---- Niveau ------- 5.4 ---- ";
						  for(k = 0; k < n2; k++)
							  ptr[offset * n2 + k] = OneLine[5 + k]; 
					  }
					  //DEB_TRACE() << "---- Niveau ------- 6 ---- ";

					  DEB_TRACE() << "image# " << i <<" cleaned" ;
					  HwFrameInfoType frame_info;
					  //DEB_TRACE() << "---- Niveau ------- 7 ---- ";
					  frame_info.acq_frame_nb = i;
					  //- raise the image to lima
					  buffer_mgr.newFrameReady(frame_info);
					  //DEB_TRACE() << "---- Niveau ------- 8 ---- ";
				  }
			  }

			  DEB_TRACE() <<"Freeing every image pointer of the images array";
			  for(int j=0 ; j < m_nb_frames ; j++)
				  delete[] pSeqImage[j];
			  DEB_TRACE() <<"Freeing images array";
			  delete[] pSeqImage;
			  m_status = Camera::Ready;
			  DEB_TRACE() <<"m_status is Ready";
			
		
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
//
//-----------------------------------------------------
void Camera::setFParameters(unsigned deadtime, unsigned init,
								unsigned shutter, unsigned ovf,    unsigned mode,
								unsigned n,       unsigned p,
								unsigned GP1,     unsigned GP2,    unsigned GP3,      unsigned GP4)
{

	DEB_MEMBER_FUNCT();

	DEB_TRACE() << "Setting all F Parameters ..." ;
	m_fparameter_deadtime	= deadtime; //- Temps entre chaque image
	m_fparameter_init		= init;		//- Temps initial
	m_fparameter_shutter	= shutter;	//- 
	m_fparameter_ovf		= ovf;		//- 
	m_fparameter_mode		= mode;		//- mode de synchro
	m_fparameter_n			= n;
	m_fparameter_p			= p;
	m_fparameter_GP1		= GP1;
	m_fparameter_GP2		= GP2;
	m_fparameter_GP3		= GP3;
	m_fparameter_GP4		= GP4;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::setAcquisitionType(short acq_type)
{
	DEB_MEMBER_FUNCT();
    m_acquisition_type = (Camera::XpadAcqType)acq_type;

	DEB_TRACE() << "m_acquisition_type = " << m_acquisition_type  ;
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::loadFlatConfig(unsigned flat_value)
{
	DEB_MEMBER_FUNCT();

	if (xpci_modLoadFlatConfig(m_modules_mask, m_chip_number, flat_value) == 0)
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
    unsigned long mask_local = 0x00;
    SET(mask_local,(modNum-1));// -1 because modNum start at 1

	if(xpci_modLoadAllConfigG(mask_local,chipId, 
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
			DEB_TRACE() << "loadAllConfigG for module " << modNum  << ", and chip" << chipId << " -> OK" ;
		}
		else
		{
			throw LIMA_HW_EXC(Error, "Error in loadAllConfigG!");
		}
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::loadConfigG(const vector<unsigned long>& reg_and_value)
{
	DEB_MEMBER_FUNCT();

	if(xpci_modLoadConfigG(m_modules_mask, m_chip_number, reg_and_value[0], reg_and_value[1])==0)
	{
		DEB_TRACE() << "loadConfigG: " << reg_and_value[0] << ", with value: " << reg_and_value[1] << " -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in loadConfigG!");
	}
}

//-----------------------------------------------------
//
//-----------------------------------------------------
void Camera::loadAutoTest(unsigned known_value)
{
	DEB_MEMBER_FUNCT();

	if(xpci_modLoadAutoTest(m_modules_mask, known_value)==0)
	{
		DEB_TRACE() << "loadAutoTest with value: " << known_value << " -> OK" ;
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

    DEB_TRACE() << "Lima::Camera::getModConfig -> xpci_getModConfig -> 1" ;
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

    return m_dacl;
}

//-----------------------------------------------------
//		Reset the detector
//-----------------------------------------------------
void Camera::reset()
{
    DEB_MEMBER_FUNCT();
    if(xpci_hubModRebootNIOS(m_modules_mask) == 0)
	{
        DEB_TRACE() << "reset -> xpci_hubModRebootNIOS -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in xpci_hubModRebootNIOS!");
	}
}




