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

namespace Xpad
{
  /*******************************************************************
   * \class Camera
   * \brief object controlling the xpad detector via xpix driver
   *******************************************************************/
  class Camera
  {
%TypeHeaderCode
#include <XpadCamera.h>
%End

  public:

    enum Status {
      Ready, Exposure, Readout,Fault
    };

    Camera();
    ~Camera();

    void start();
    void stop();

    //- Det info
    void getImageSize(Size& size /Out/);
    void setPixelDepth(ImageType pixel_depth);
    void getPixelDepth(ImageType& pixel_depth /Out/);
    void getPixelSize(double& x_size /Out/,double &y_size /Out/);
    void getDetectorType(std::string& type /Out/);
    void getDetectorModel(std::string& model /Out/);

    //- Buffer
    BufferCtrlMgr& getBufferMgr();
    void setNbFrames(int  nb_frames);
    void getNbFrames(int& nb_frames /Out/);

    //- Sync 
    void setTrigMode(TrigMode  mode);
    void getTrigMode(TrigMode& mode /Out/);
    void setExpTime(double  exp_time);
    void getExpTime(double& exp_time /Out/);
		
    //- Status
    void getStatus(Xpad::Camera::Status& status /Out/);
	
    //---------------------------------------------------------------
    //- XPAD Stuff
    //- Set all the config G
    void setAllConfigG(const std::vector<long>& allConfigG);
    //- Set the F parameters
    void setFParameters(unsigned deadtime, unsigned init,
			unsigned shutter, unsigned ovf,    unsigned mode,
			unsigned n,       unsigned p,
			unsigned GP1,     unsigned GP2,    unsigned GP3,      unsigned GP4);
    //-	Set the Acquisition type between fast and slow
    void setAcquisitionType(short acq_type);
    //-	Load of flat config of value: flat_value (on each pixel)
    void loadFlatConfig(unsigned flat_value);
    //- Load all the config G with predefined values (on each chip)
    void loadAllConfigG();
    //- Load a wanted config G with a wanted value
    //void loadConfigG(const vector<unsigned long>& reg_and_value);
    //- Load a known value to the pixel counters
    void loadAutoTest(unsigned known_value);
    //- Get the DACL values
    //vector<uint16_t> getDacl();
    //- Save and load Dacl
    //void saveAndloadDacl(uint16_t* all_dacls);
  };

};

