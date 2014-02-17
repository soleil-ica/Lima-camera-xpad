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

//---------------------------
//- Ctor
//---------------------------
Camera::Camera(std::string xpad_model): 	m_buffer_cb_mgr(m_buffer_alloc_mgr),
m_buffer_ctrl_mgr(m_buffer_cb_mgr)
{
    DEB_CONSTRUCTOR();

    //- default values:
    m_modules_mask      = 0x00;
    m_chip_number       = 7; 
    m_imxpad_format     = 0; //- 16 bits
    m_nb_frames         = 1;
    
    m_status            = Camera::Ready;
    m_acquisition_type	= Camera::SYNC;
    m_current_nb_frames = 0;

    m_time_between_images_usec      = 5000;
    m_ovf_refresh_time_usec         = 4000;
    m_time_before_start_usec        = 0;
    m_shutter_time_usec             = 0;
    m_busy_out_sel                  = 0;
    m_calibration_adjusting_number  = 1;
    m_geom_corr                     = 0;

    if		(xpad_model == "BACKPLANE") 	m_xpad_model = BACKPLANE;
    else if	(xpad_model == "HUB")	        m_xpad_model = HUB;
    else if	(xpad_model == "IMXPAD_S70")	m_xpad_model = IMXPAD_S70;
    else if	(xpad_model == "IMXPAD_S140")	m_xpad_model = IMXPAD_S140;
    else if	(xpad_model == "IMXPAD_S340")	m_xpad_model = IMXPAD_S340;
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
            DEB_TRACE() << "Ask modules that are ready: OK (modules mask = " << std::hex << m_modules_mask << ")" ;
            m_module_number = xpci_getModNb(m_modules_mask);

            if (m_module_number != 0)
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
//- Camera::prepare()
//---------------------------
void Camera::prepare()
{
    DEB_MEMBER_FUNCT();

    m_stop_asked = false;
    m_image_array = 0;
    m_nb_image_done = 0;
    m_nb_last_aquired_image = 0;
    
   
    
    DEB_TRACE() << "m_acquisition_type = " << m_acquisition_type ;
    DEB_TRACE() << "Setting Exposure parameters with values: ";
    DEB_TRACE() << "\tm_exp_time_usec 		= " << m_exp_time_usec;
    DEB_TRACE() << "\tm_imxpad_trigger_mode = " << m_imxpad_trigger_mode;
    DEB_TRACE() << "\tm_nb_frames (before live mode check)			= " << m_nb_frames;
    DEB_TRACE() << "\tm_imxpad_format 		= " << m_imxpad_format;

    //- call the setExposureParameters
    //m_xpad_model parameter must be 1 (in our detector type IMXPAD_S140) or XPIX_NOT_USED_YET
    //maybe library must manage this, we can provide IMXPAD_Sxx to this function if necessary
    setExposureParameters(	m_exp_time_usec,
                            m_time_between_images_usec,
                            m_time_before_start_usec,
                            m_shutter_time_usec,
                            m_ovf_refresh_time_usec,
                            m_imxpad_trigger_mode,
                            XPIX_NOT_USED_YET,
                            XPIX_NOT_USED_YET,
                            //- if live i.e m_nb_frames==0 => force m_nb_frames =1
                            (m_nb_frames==0)?1:m_nb_frames,
                            XPIX_NOT_USED_YET,
                            m_imxpad_format,
                            XPIX_NOT_USED_YET,
                            XPIX_NOT_USED_YET,
                            XPIX_NOT_USED_YET,
                            XPIX_NOT_USED_YET,
                            XPIX_NOT_USED_YET);

    //-	((80 colonnes * 7 chips) * taille du pixel) * 120 lignes * nb_modules
    switch(m_pixel_depth)
    {
        case B2: //- aka 16 bits
            DEB_TRACE() << "Allocating 16 bits image(s) array  (" << m_nb_frames << " image(s))";
            m_full_image_size_in_bytes = ((CHIP_NB_COLUMN * m_chip_number) * TWO_BYTES)  * (CHIP_NB_ROW * m_module_number);
            m_image_array = reinterpret_cast<void**>(new uint16_t* [ m_nb_frames ]);
            break;
        case B4: //- aka 32 bits
            DEB_TRACE() << "Allocating 32 bits image(s) array (" << m_nb_frames << " image(s))";
            m_full_image_size_in_bytes = ((CHIP_NB_COLUMN * m_chip_number) * FOUR_BYTES )  * (CHIP_NB_ROW * m_module_number);
            m_image_array = reinterpret_cast<void**>(new uint32_t* [ m_nb_frames ]);
            break;
        default:
            DEB_ERROR() << "Pixel depth not supported" ;
            throw LIMA_HW_EXC(Error, "Pixel depth not supported: possible values are:\n16 or \n32");
            break;
    }

    DEB_TRACE() << "Allocating every image pointer of the image(s) array (1 image full size = "<< m_full_image_size_in_bytes << ") ";
    for( int i=0 ; i < m_nb_frames ; i++ )
    {
        switch(m_pixel_depth)
        {
            case B2: //- aka 16 bits
                m_image_array[i] = new uint16_t [ m_full_image_size_in_bytes / TWO_BYTES ];//we allocate a number of pixels
                break;
            case B4: //- aka 32 bits
                m_image_array[i] = new uint32_t [ m_full_image_size_in_bytes / FOUR_BYTES ];//we allocate a number of pixels
                break;
        }
    }
}

//---------------------------
//- Camera::start()
//---------------------------
void Camera::start()
{
    DEB_MEMBER_FUNCT();

    if((m_acquisition_type == Camera::SYNC) || (m_nb_frames == 0)) //- 0: aka live mode
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
        m_imxpad_format = 0;
        break;

    case Bpp32:
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
//- Camera::getDetectorType(std::string& type)
//-----------------------------------------------------
void Camera::getDetectorType(std::string& type)
{
    DEB_MEMBER_FUNCT();
    type = "XPAD";
}

//-----------------------------------------------------
//- Camera::getDetectorModel(std::string& type)
//-----------------------------------------------------
void Camera::getDetectorModel(std::string& type)
{
    DEB_MEMBER_FUNCT();
    if(m_xpad_model == BACKPLANE)           type = "BACKPLANE";
    else if(m_xpad_model == HUB)            type = "HUB";
    else if(m_xpad_model == IMXPAD_S70)     type = "IMXPAD_S70";
    else if(m_xpad_model == IMXPAD_S140)    type = "IMXPAD_S140";
    else if(m_xpad_model == IMXPAD_S340)    type = "IMXPAD_S340";
    else if(m_xpad_model == IMXPAD_S540)    type = "IMXPAD_S540";
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
                DEB_TRACE() << "=========================================";
                DEB_TRACE() << "Camera::->XPAD_DLL_START_SYNC_MSG";

                m_status = Camera::Exposure;

                //- Start the img sequence
                DEB_TRACE() <<"Start acquiring a sequence of image(s)";

                m_start_sec = Timestamp::now();

                if ( xpci_getImgSeq(	m_pixel_depth, 
                                        m_modules_mask,
                                        m_chip_number,
                                        //- if live i.e m_nb_frames==0 => force m_nb_frames =1
                                        (m_nb_frames==0)?1:m_nb_frames,
                                        (void**)m_image_array,
                                        // next are ignored in V2:
                                        XPIX_V1_COMPATIBILITY,
                                        XPIX_V1_COMPATIBILITY,
                                        XPIX_V1_COMPATIBILITY,
                                        XPIX_V1_COMPATIBILITY) == -1)
                {
                    DEB_ERROR() << "Error: xpci_getImgSeq has returned an error..." ;

                    DEB_TRACE() << "Freeing each image pointer of the image(s) array";
                    for(int i=0 ; i < m_nb_frames ; i++)
                    {
                        if(m_imxpad_format == 0) //- aka 16 bits
                            delete[] reinterpret_cast<uint16_t*>(m_image_array[i]);
                        else
                            delete[] reinterpret_cast<uint32_t*>(m_image_array[i]);
                    }

                    DEB_TRACE() << "Freeing the image(s) array";
                    if(m_imxpad_format == 0) //- aka 16 bits
                        delete[] reinterpret_cast<uint16_t**>(m_image_array);
                    else //- aka 32 bits
                        delete[] reinterpret_cast<uint32_t**>(m_image_array);

                    m_status = Camera::Fault;
                    throw LIMA_HW_EXC(Error, "xpci_getImgSeq has returned an error ! ");
                }

                m_end_sec = Timestamp::now() - m_start_sec;
                DEB_TRACE() << "Time for xpci_getImgSeq (sec) = " << m_end_sec;

                m_status = Camera::Readout;

                DEB_TRACE() 	<< "\n#######################"
                                << "\nall image(s) are acquired"
                                << "\n#######################" ;

                //- Publish each image and call new frame ready for each frame
                StdBufferCbMgr& buffer_mgr = m_buffer_cb_mgr;
                DEB_TRACE() << "Publishing each acquired image through newFrameReady()";
                m_start_sec = Timestamp::now();
                for(int i=0; i<m_nb_frames; i++)
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
                        memcpy((uint16_t *)lima_img_ptr, (uint16_t *)m_image_array[i],m_full_image_size_in_bytes);
                    else //- aka 32 bits
                        memcpy((uint32_t *)lima_img_ptr, (uint32_t *)m_image_array[i],m_full_image_size_in_bytes);

                    HwFrameInfoType frame_info;
                    frame_info.acq_frame_nb = i;
                    //- raise the image to Lima
                    buffer_mgr.newFrameReady(frame_info);
                    DEB_TRACE() << "image " << i <<" published with newFrameReady()" ;
                }

                m_end_sec = Timestamp::now() - m_start_sec;
                DEB_TRACE() << "Time for publishing image(s)es to Lima (sec) = " << m_end_sec;


                m_start_sec = Timestamp::now();
                DEB_TRACE() << "Freeing every image pointer of the image(s) array";
                //- they were allocated by the xpci_getImgSeq function
                for(int i=0 ; i < m_nb_frames ; i++)
                    delete[] m_image_array[i];
                DEB_TRACE() << "Freeing image(s) array";
                delete[] m_image_array;
                m_status = Camera::Ready;
                m_end_sec = Timestamp::now() - m_start_sec;
                DEB_TRACE() << "Time for freeing memory: now Ready! (sec) = " << m_end_sec;
                DEB_TRACE() << "m_status is Ready";

                //- Check if the it is live and if yes: restart
                if (m_nb_frames == 0 && m_stop_asked == false)
                {
                    //- Post XPAD_DLL_START_LIVE_ACQ_MSG msg
                    this->post(new yat::Message(XPAD_DLL_START_SYNC_MSG), kPOST_MSG_TMO);
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

                if ( xpci_getImgSeqAs(	(IMG_TYPE)m_imxpad_format, 
                                        m_modules_mask,
                                        m_chip_number,
                                        NULL,//- to be clarified by impxad
                                        XPIX_V1_COMPATIBILITY,//timeout
                                        XPIX_V1_COMPATIBILITY,
                                        XPIX_V1_COMPATIBILITY,
                                        XPIX_V1_COMPATIBILITY,
                                        m_nb_frames,
                                        (void**)m_image_array,
                                        8000, //- to be clarified by impxad
                                        NULL,//- to be clarified by impxad
                                        m_nb_frames //- to be clarified by impxad
                                        ) == -1)              
                {
                    DEB_ERROR() << "Error: xpci_getImgSeqAs has returned an error..." ;

                    DEB_TRACE() << "Freeing each image pointer of the image(s) array";
                    for(int i=0 ; i < m_nb_frames ; i++)
                    {
                        if(m_imxpad_format == 0) //- aka 16 bits
                            delete[] reinterpret_cast<uint16_t*>(m_image_array[i]);
                        else //- aka 32 bits
                            delete[] reinterpret_cast<uint32_t*>(m_image_array[i]);
                    }

                    DEB_TRACE() << "Freeing the image(s) array";
                    if(m_imxpad_format == 0) //- aka 16 bits
                        delete[] reinterpret_cast<uint16_t**>(m_image_array);
                    else //- aka 32 bits
                        delete[] reinterpret_cast<uint32_t**>(m_image_array);

                    m_status = Camera::Fault;
                    throw LIMA_HW_EXC(Error, "xpci_getImgSeqAs has returned an error ! ");
                }

                //- Post XPAD_DLL_GET_ASYNC_IMAGES_MSG msg
                this->post(new yat::Message(XPAD_DLL_GET_ASYNC_IMAGES_MSG), kPOST_MSG_TMO);
            }
            break;

            //-----------------------------------------------------    
        case XPAD_DLL_GET_ASYNC_IMAGES_MSG:
            {
                DEB_TRACE() <<"Camera::->XPAD_DLL_GET_ASYNC_IMAGES_MSG";
                
                //- TODO: set the corrected img 
                void*   m_one_image;

                if(m_imxpad_format == 0) //- aka 16 bits
                    m_one_image = new uint16_t [ m_full_image_size_in_bytes / TWO_BYTES ];
                else //- aka 32 bits
                    m_one_image = new uint32_t [ m_full_image_size_in_bytes / FOUR_BYTES ];

                while (m_nb_image_done != m_nb_frames)
                {
                    m_nb_last_aquired_image = xpci_getNumberLastAcquiredAsyncImage();
                    DEB_TRACE() << "Last acquired image = " << m_nb_last_aquired_image;

                    for(int i = m_nb_image_done; i<m_nb_last_aquired_image; i++ )
                    {
                        m_nb_image_done++;

                        if ( xpci_getAsyncImage(	(IMG_TYPE)m_imxpad_format, 
                                                    m_modules_mask,
                                                    m_chip_number,
                                                    NULL,//- to be clarified by impxad
                                                    (void*)m_one_image,
                                                    i, //- image index to get
                                                    NULL, //- corrected img
                                                    m_geom_corr //- flag for activating correction
                                                    ) == -1)           

                        {
                            DEB_ERROR() << "Error: xpci_getAsyncImage has returned an error..." ;

                            DEB_TRACE() << "Freeing the image";
                            if(m_imxpad_format == 0) //- aka 16 bits
                                delete[] reinterpret_cast<uint16_t**>(m_one_image);
                            else //- aka 32 bits
                                delete[] reinterpret_cast<uint32_t**>(m_one_image);

                            m_status = Camera::Fault;
                            throw LIMA_HW_EXC(Error, "xpci_getAsyncImage has returned an error ! ");
                        }

                        //- Publish each image and call new frame ready for each frame
                        StdBufferCbMgr& buffer_mgr = m_buffer_cb_mgr;
                        DEB_TRACE() << "Publishing image : " << i << " through newFrameReady()";
                        m_start_sec = Timestamp::now();
                        
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
                            memcpy((uint16_t *)lima_img_ptr, (uint16_t *)m_one_image,m_full_image_size_in_bytes);
                        else //- aka 32 bits
                            memcpy((uint32_t *)lima_img_ptr, (uint32_t *)m_one_image,m_full_image_size_in_bytes);

                        HwFrameInfoType frame_info;
                        frame_info.acq_frame_nb = i;
                        //- raise the image to Lima
                        buffer_mgr.newFrameReady(frame_info);
                        DEB_TRACE() << "image " << i <<" published with newFrameReady()" ;
                    }
                }   
                
                //- Finished
                m_start_sec = Timestamp::now();
                DEB_TRACE() << "Freeing last image pointer";
                delete[] m_one_image;
                m_status = Camera::Ready;
                m_end_sec = Timestamp::now() - m_start_sec;
                DEB_TRACE() << "Time for freeing memory: now Ready! (sec) = " << m_end_sec;
                DEB_TRACE() << "m_status is Ready";
            }
            break;

            //-----------------------------------------------------    
        case XPAD_DLL_CALIBRATE:
            {
                DEB_TRACE() <<"Camera::->XPAD_DLL_CALIBRATE";

                m_status = Camera::Calibrating;

                switch (m_calibration_type)
                {
                    //-----------------------------------------------------	
                case Camera::OTN_SLOW:
                    {
                        DEB_TRACE() <<"XPAD_DLL_CALIBRATE->OTN_SLOW";   

                        if(imxpad_calibration_OTN_slow(m_modules_mask,(char*)m_calibration_path.c_str(),m_calibration_adjusting_number) == 0)
                        {
                            DEB_TRACE() << "calibrateOTNSlow -> imxpad_calibration_OTN_slow -> OK" ;
                        }
                        else
                        {

                            Event *my_event = new Event(Hardware, Event::Error, Event::Camera, Event::Default, "the calibration path already exist");
                            //DEB_EVENT(*my_event) << DEB_VAR1(*my_event);
                            reportEvent(my_event);

                            m_status = Camera::Fault;
                            //- TODO: get the xpix error 
                            throw LIMA_HW_EXC(Error, "Error in imxpad_calibration_OTN_slow!");
                        }
                    }
                    break;

                    //-----------------------------------------------------	
                case Camera::UPLOAD:
                    {
                        DEB_TRACE() <<"XPAD_DLL_CALIBRATE->UPLOAD";    

                        if(imxpad_uploadCalibration(m_modules_mask,(char*)m_calibration_path.c_str()) == 0)
                        {
                            DEB_TRACE() << "uploadCalibration -> imxpad_uploadCalibration -> OK" ;
                        }
                        else
                        {
                            m_status = Camera::Fault;
                            //- TODO: get the xpix error 
                            throw LIMA_HW_EXC(Error, "Error in imxpad_uploadCalibration!");
                        }
                    }
                    break;
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

    if (xpci_modExposureParam(  m_modules_mask, Texp, Twait, Tinit,
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
    m_acquisition_type = (Camera::XpadAcqType) acq_type;

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

    if(xpci_modLoadAllConfigG(  mask_local_module,mask_local_chip, 
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
void Camera::loadConfigG(const std::vector<unsigned long>& reg_and_value)
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

//-----------------------------------------------------
//		calibrate over the noise Slow
//-----------------------------------------------------
void Camera::calibrateOTNSlow ( const std::string& path)
{
    DEB_MEMBER_FUNCT();

    m_calibration_path = path;
    m_calibration_type = Camera::OTN_SLOW;

    this->post(new yat::Message(XPAD_DLL_CALIBRATE), kPOST_MSG_TMO);
}

//-----------------------------------------------------
//		upload a calibration
//-----------------------------------------------------
void Camera::uploadCalibration(const std::string& path)
{
    DEB_MEMBER_FUNCT();

    m_calibration_path = path;
    m_calibration_type = Camera::UPLOAD;

    this->post(new yat::Message(XPAD_DLL_CALIBRATE), kPOST_MSG_TMO);
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
//		Set the specific parameters
//-----------------------------------------------------
void Camera::setSpecificParameters(    unsigned deadtime, unsigned init,
                                       unsigned shutter, unsigned ovf,
                                       unsigned n,       unsigned p,
                                       unsigned busy_out_sel,
                                       bool geom_corr,
                                       unsigned GP1,     unsigned GP2,    unsigned GP3,      unsigned GP4)
{

    DEB_MEMBER_FUNCT();

    DEB_TRACE() << "Setting Specific Parameters ..." ;

    m_time_between_images_usec  = deadtime; //- Temps entre chaque image
    m_time_before_start_usec    = init;     //- Temps initial
    m_shutter_time_usec         = shutter;
    m_ovf_refresh_time_usec     = ovf;
    m_specific_param_n          = n;
    m_specific_param_p          = p;
    m_busy_out_sel              = busy_out_sel;
    m_geom_corr                 = (unsigned int)geom_corr;
    m_specific_param_GP1		= GP1;
    m_specific_param_GP2		= GP2;
    m_specific_param_GP3		= GP3;
    m_specific_param_GP4		= GP4;
}


//-----------------------------------------------------
//		decrement the ITHL
//-----------------------------------------------------
void Camera::setCalibrationAdjustingNumber(unsigned calibration_adjusting_number)
{
    DEB_MEMBER_FUNCT();

    m_calibration_adjusting_number = calibration_adjusting_number;

}
