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
Camera::Camera(string xpad_model): 	m_buffer_cb_mgr(m_buffer_alloc_mgr),
m_buffer_ctrl_mgr(m_buffer_cb_mgr),
m_modules_mask(0x00),
m_chip_number(7),
m_pixel_depth(B4),
m_nb_frames(1)
{
	DEB_CONSTRUCTOR();

	m_status = Camera::Ready;

	//- default value:
    m_acquisition_type		= Camera::SYNC; // SYNC

    if		(xpad_model == "BACKPLANE") 	m_xpad_model = BACKPLANE;
    else if	(xpad_model == "IMXPAD_S70")	m_xpad_model = IMXPAD_S70;
    else if	(xpad_model == "IMXPAD_S140")	m_xpad_model = IMXPAD_S140;
    else if	(xpad_model == "IMXPAD_S340")	m_xpad_model = IMXPAD_S340;
    else if	(xpad_model == "IMXPAD_S540")	m_xpad_model = IMXPAD_S540;
    else
    	throw LIMA_HW_EXC(Error, "Xpad Model not supported");
    //- temporary as a workaround to the pogo6 bug
    ////m_xpad_model = IMXPAD_S140;

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
		    DEB_TRACE() << "Ask modules that are ready: OK (modules mask = " << std::hex << m_modules_mask << ")" ;
		    m_module_number = xpci_getModNb(m_modules_mask);
            
		    if (m_module_number !=0)
		    {
			    DEB_TRACE() << "--> Number of Modules 		 = " << m_module_number ;			
		    }
		    else
		    {
			    /*DEB_TRACE() << "Reseting detector ... ";
			    this->reset();*/
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
	    m_image_size = Size(80 * m_chip_number ,120 * m_module_number); //- MODIF-NL-ICA
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

    //-	((80 colonnes * 7 chips) * taille du pixel) * 120 lignes * nb_modules
	if (m_pixel_depth == B2)
	{
		m_full_image_size_in_bytes = ((80 * m_chip_number) * 2)  * (120 * m_module_number);
	} 
	else if(m_pixel_depth == B4)
	{
		m_full_image_size_in_bytes = ((80 * m_chip_number) * 4 )  * (120 * m_module_number);
	} 

	DEB_TRACE() << "m_acquisition_type = " << m_acquisition_type ;

	DEB_TRACE() << "Setting Exposure parameters with values: ";
	DEB_TRACE() << "\tm_exp_time_usec 		= " << m_exp_time_usec;
	DEB_TRACE() << "\tm_imxpad_trigger_mode = " << m_imxpad_trigger_mode;
	DEB_TRACE() << "\tm_nb_frames 			= " << m_nb_frames;
	DEB_TRACE() << "\tm_imxpad_format 		= " << m_imxpad_format;

	//- call the setExposureParameters
    //- hard coded params (until needed)
	unsigned long time_between_images_usec = 5000;
	unsigned long ovf_refresh_time_usec = 4000;
    unsigned long time_before_start_usec = 0;
    unsigned long shutter_time_usec = 0;

//m_xpad_model parameter must be 1 (in our detector type IMXPAD_S140) or XPIX_NOT_USED_YET
//maybe library must manage this, we can provide IMXPAD_Sxx to this function if necessary
	setExposureParameters(	m_exp_time_usec,
							time_between_images_usec,
							time_before_start_usec,
							shutter_time_usec,
							ovf_refresh_time_usec,
							m_imxpad_trigger_mode,
							XPIX_NOT_USED_YET,
							XPIX_NOT_USED_YET,
							m_nb_frames,
							XPIX_NOT_USED_YET,
							m_imxpad_format,
							(m_xpad_model == IMXPAD_S140)?1:XPIX_NOT_USED_YET,/**/
							XPIX_NOT_USED_YET,
							XPIX_NOT_USED_YET,
							XPIX_NOT_USED_YET,
							XPIX_NOT_USED_YET);

    if(m_acquisition_type == Camera::SYNC)
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

	m_status = Camera::Ready;
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
	if(m_xpad_model == BACKPLANE) type = "BACKPLANE";
	else if(m_xpad_model == IMXPAD_S70) type = "IMXPAD_S70";
	else if(m_xpad_model == IMXPAD_S140) type = "IMXPAD_S140";
	else if(m_xpad_model == IMXPAD_S340) type = "IMXPAD_S340";
	else if(m_xpad_model == IMXPAD_S540) type = "IMXPAD_S540";
    else throw LIMA_HW_EXC(Error, "Xpad Type not supported");
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
        m_imxpad_trigger_mode = 0;
		break;
	case ExtGate:
        m_imxpad_trigger_mode = 1;
		break;
	case ExtTrigSingle:
		m_imxpad_trigger_mode = 2;
		break;
	default:
		DEB_ERROR() << "Error: Trigger mode unsupported: only IntTrig, ExtGate or ExtTrigSingle" ;
		throw LIMA_HW_EXC(Error, "Trigger mode unsupported: only IntTrig, ExtGate or ExtTrigSingle");
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
		mode = ExtTrigSingle;
		break;
	case 2:
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
				if(m_pixel_depth == B2)
					image_array = reinterpret_cast<void**>(new uint16_t* [ m_nb_frames ]);
				else//B4
					image_array = reinterpret_cast<void**>(new uint32_t* [ m_nb_frames ]);

				DEB_TRACE() <<"Allocating every image pointer of the images array (1 image full size = "<< m_full_image_size_in_bytes << ") ";
				for( int i=0 ; i < m_nb_frames ; i++ )
				{
					if(m_pixel_depth == B2)
						image_array[i] = new uint16_t [ m_full_image_size_in_bytes / 2 ];//we allocate a number of pixels
					else //B4
						image_array[i] = new uint32_t [ m_full_image_size_in_bytes / 4 ];//we allocate a number of pixels
				}

				m_status = Camera::Exposure;

				//- Start the img sequence
				DEB_TRACE() <<"Start acquiring a sequence of images";

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
					DEB_ERROR() << "Error: getImgSeq as returned an error..." ;

					DEB_TRACE() << "Freeing every image pointer of the images array";
					for(int i=0 ; i < m_nb_frames ; i++)
					{
						if(m_pixel_depth == B2)
							delete[] reinterpret_cast<uint16_t*>(image_array[i]);
						else
							delete[] reinterpret_cast<uint32_t*>(image_array[i]);
					}

					DEB_TRACE() << "Freeing images array";
					if(m_pixel_depth == B2)
						delete[] reinterpret_cast<uint16_t**>(image_array);
					else//B4
						delete[] reinterpret_cast<uint32_t**>(image_array);

					m_status = Camera::Fault;
                    throw LIMA_HW_EXC(Error, "xpci_getImgSeq as returned an error ! ");
				}

				m_status = Camera::Readout;

				DEB_TRACE() 	<< "\n#######################"
								<< "\nall images are acquired"
								<< "\n#######################" ;

				int	i=0;

				//- Publish each image and call new frame ready for each frame
				StdBufferCbMgr& buffer_mgr = m_buffer_cb_mgr;
				DEB_TRACE() <<"Publish each acquired image through newFrameReady()";
				for(i=0; i<m_nb_frames; i++)
				{
					int buffer_nb, concat_frame_nb;
					buffer_mgr.setStartTimestamp(Timestamp::now());
					buffer_mgr.acqFrameNb2BufferNb(i, buffer_nb, concat_frame_nb);

					void* lima_img_ptr;
					if(m_pixel_depth == B2)
						lima_img_ptr = (uint16_t*)(buffer_mgr.getBufferPtr(buffer_nb,concat_frame_nb));
					else//B4
						lima_img_ptr = (uint32_t*)(buffer_mgr.getBufferPtr(buffer_nb,concat_frame_nb));

					//- copy image in the lima buffer
					if(m_pixel_depth == B2)
						memcpy((uint16_t *)lima_img_ptr, (uint16_t *)image_array[i],m_full_image_size_in_bytes);
					else//B4
						memcpy((uint32_t *)lima_img_ptr, (uint32_t *)image_array[i],m_full_image_size_in_bytes);

					DEB_TRACE() << "image# " << i <<" cleaned" ;
					HwFrameInfoType frame_info;
					frame_info.acq_frame_nb = i;
					//- raise the image to Lima
					buffer_mgr.newFrameReady(frame_info);
				}

				DEB_TRACE() <<"Freeing every image pointer of the images array";
				for(i=0 ; i < m_nb_frames ; i++)
					delete[] image_array[i];
				DEB_TRACE() <<"Freeing images array";
				delete[] image_array;
				m_status = Camera::Ready;
				DEB_TRACE() <<"m_status is Ready";

			}
			break;

			//-----------------------------------------------------    
			case XPAD_DLL_START_ASYNC_MSG:
			{
				/*************
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
				  cout << "Parametres pour getImgSeqAs: "<< endl;
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
					  10000, //- callback timeout: 10s
					  m_trigger_type,
					  m_exp_time,
					  m_time_unit,
					  m_nb_frames,
					  (void**)pSeqImage,
					  FIRST_TIMEOUT,
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
							  int offset = ((120 * (OneLine[1] )) + (OneLine[4] - 1));

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
			
***************/
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
			                         unsigned Tshutter,unsigned Tovf,unsigned mode, unsigned n,unsigned p,
			                         unsigned nbImages,unsigned BusyOutSel,unsigned formatIMG,unsigned postProc,
			                         unsigned GP1,unsigned GP2,unsigned GP3,unsigned GP4)
{

	DEB_MEMBER_FUNCT();

    if (xpci_modExposureParam(m_modules_mask, Texp, Twait, Tinit,
	                          Tshutter, Tovf, mode,  n, p,
	                          nbImages, BusyOutSel, formatIMG, postProc,
	                          GP1, GP2, GP3, GP4) == 0)
	{
		DEB_TRACE() << "setExposureParameters -> OK" ;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Error in setExposureParameters!");
	}
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
