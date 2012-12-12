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
#include "XpadTask.h"
#include <sstream>
#include <iostream>
#include <string>
#include <math.h>

using namespace lima;
using namespace lima::Xpad;
using namespace std;


// ============================================================================
//- ctor
// ============================================================================
XpadTask::XpadTask()
{
    DEB_TRACE() << "XpadTask::XpadTask";
}

// ============================================================================
//- dtor
// ============================================================================
XpadTask::~XpadTask() 
{
	DEB_TRACE() << "XpadTask::~XpadTask";
}

// ============================================================================
//- handle_message: handle the message Q
// ============================================================================
void XpadTask::handle_message( yat::Message& msg )  throw( yat::Exception )
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
                
                		DEB_TRACE() <<"Allocating every image pointer of the images array (1 image full size = "<< m_full_image_size_in_bytes << ") ";
				for( int i=0 ; i < m_nb_frames ; i++ )
				{
					if(m_imxpad_format == 0) //- aka 16 bits
						image_array[i] = new uint16_t [ m_full_image_size_in_bytes / 2 ];//we allocate a number of pixels
					else //- aka 32 bits
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
						memcpy((uint16_t *)lima_img_ptr, (uint16_t *)image_array[i],m_full_image_size_in_bytes);
					else //- aka 32 bits
						memcpy((uint32_t *)lima_img_ptr, (uint32_t *)image_array[i],m_full_image_size_in_bytes);

					HwFrameInfoType frame_info;
					frame_info.acq_frame_nb = i;
					//- raise the image to Lima
					buffer_mgr.newFrameReady(frame_info);
                    DEB_TRACE() << "image " << i <<" published with newFrameReady()" ;
				}

				DEB_TRACE() <<"Freeing every image pointer of the images array";
                //- they were allocated by the xpci_getImgSeq function
				for(i=0 ; i < m_nb_frames ; i++)
					delete[] image_array[i];
				DEB_TRACE() <<"Freeing images array";
				delete[] image_array;
				m_status = Camera::Ready;
				DEB_TRACE() <<"m_status is Ready";

			}
			break;

            //-----------------------------------------------------    
			case XPAD_DLL_START_LIVE_ACQ_MSG:
			{
				DEB_TRACE() 	<< "=========================================";
                DEB_TRACE() <<"Camera::->XPAD_DLL_START_LIVE_ACQ_MSG";

				int one_frame = 1;

				//- Declare local temporary image buffer
				void**	image_array;

				// allocate multiple buffers
				DEB_TRACE() <<"Allocating images array (" << one_frame << " images)";
				if(m_pixel_depth == B2)
					image_array = reinterpret_cast<void**>(new uint16_t* [ one_frame ]);
				else//B4
					image_array = reinterpret_cast<void**>(new uint32_t* [ one_frame ]);

				DEB_TRACE() <<"Allocating every image pointer of the images array (1 image full size = "<< m_full_image_size_in_bytes << ") ";
				for( int i=0 ; i < one_frame ; i++ )
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
                            			one_frame,
                            			(void**)image_array,
                            			// next are ignored in V2:
                            			XPIX_V1_COMPATIBILITY,
					                    XPIX_V1_COMPATIBILITY,
					                    XPIX_V1_COMPATIBILITY,
					                    XPIX_V1_COMPATIBILITY) == -1)
				{
					DEB_ERROR() << "Error: getImgSeq as returned an error..." ;

					DEB_TRACE() << "Freeing every image pointer of the images array";
					for(int i=0 ; i < one_frame ; i++)
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
				for(i=0; i<one_frame; i++)
				{
                    m_current_nb_frames = i;
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
				for(int i=0 ; i < one_frame ; i++)
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
			
                    */
		  }
		  break;

            case XPAD_DLL_CALIBRATE:
                {
                    DEB_TRACE() <<"Camera::->XPAD_DLL_CALIBRATE";

                    m_status = Camera::Exposure;

                    switch (m_calibration_type)
                    {
                        //-----------------------------------------------------	
                    case Camera::OTN_SLOW:
                        {
                            DEB_TRACE() <<"XPAD_DLL_CALIBRATE->OTN_SLOW";    

                            if(imxpad_calibrationOTN_SLOW(m_modules_mask,(char*)m_calibration_path.c_str()) == 0)
                            {
                                DEB_TRACE() << "calibrateOTNSlow -> imxpad_calibrationOTN_SLOW -> OK" ;
                            }
                            else
                            {
                                m_status = Camera::Fault;
                                //- TODO: get the xpix error 
                                throw LIMA_HW_EXC(Error, "Error in imxpad_calibrationOTN_SLOW!");
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
