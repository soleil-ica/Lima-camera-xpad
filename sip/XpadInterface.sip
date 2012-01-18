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
   * \class Interface
   * \brief Xpad hardware interface
   *******************************************************************/
  class Interface : HwInterface
  {
%TypeHeaderCode
#include <XpadInterface.h>
%End

  public:
    Interface(Xpad::Camera& cam);
    virtual ~Interface();

    //- From HwInterface
    //virtual void 	getCapList(CapList&) const;
    virtual void	getCapList(std::vector<HwCap> &cap_list /Out/) const;
    virtual void	reset(ResetLevel reset_level);
    virtual void 	prepareAcq();
    virtual void 	startAcq();
    virtual void 	stopAcq();
    virtual void 	getStatus(StatusType& status);
    virtual int 	getNbHwAcquiredFrames();

    //- Xpad specific
    //- Set all the config G
    void setAllConfigG(const std::vector<int>& allConfigG);
    //- Set the F parameters
    void setFParameters(unsigned deadtime, unsigned init,
			unsigned shutter, unsigned ovf,    unsigned mode,
			unsigned n,       unsigned p,
			unsigned GP1,     unsigned GP2,    unsigned GP3,      unsigned GP4);
    //-	set the Acquisition type between fast and slow
    void setAcquisitionType(short acq_type);
    //-	Load of flat config of value: flat_value (on each pixel)
    void loadFlatConfig(unsigned flat_value);
    //- Load all the config G with predefined values (on each chip)
    void loadAllConfigG();
    //- Load a wanted config G with a wanted value
    //void loadConfigG(const vector<unsigned long>& reg_and_value);
    //- load a known value to the pixel counters
    void loadAutoTest(unsigned known_value);
    //- Get the DACL values
    //vector<uint16_t> getDacl();
    //- Save and load Dacl
    //void saveAndloadDacl(uint16_t* all_dacls);
  };
};

