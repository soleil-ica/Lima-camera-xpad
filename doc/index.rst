.. _camera-xpad:

Xpad
-------

.. image:: xpad.jpg

Introduction
````````````

The XPAD detector is based on the photon counting technology providing a quasi noiseless imaging as well as a very high dynamic range and a fast frame rate (500 images/s).
This is a detector stemming from the collaboration of Soleil, CPPM and ESRF(D2AM). It is now supported by the ImXPAD company.

This plugin support the following models:

 - S70,
 - S140,
 - S340,
 - S540

The XPAD runs under Linux, with the help of a PCI express board from PLDA.

Prerequisite
````````````

The host where the PCI express board is installed, should have the PLDA driver installed.

Initialisation and Capabilities
````````````````````````````````

Implementing a new plugin for new detector is driven by the LIMA framework but the developer has some freedoms to choose which standard and specific features will be made available. This section is supposed to give you the correct information regarding how the camera is exported within the LIMA framework.

Camera initialisation
......................

The camera will be initialized within the :cpp:class:`Xpad::Camera` object. One should pass to the constructor, the Xpad type as a string. Possible values are:

  - "IMXPAD_S70",
  - "IMXPAD_S140",
  - "IMXPAD_S340",
  - "IMXPAD_S540"

Synchrone or Asynchrone acquisition should be selected with a call :cpp:func:`setAcquisitionType()`.

Std capabilities
................

This plugin has been implemented in respect of the mandatory capabilites but with some limitations according to some programmer's  choices. We only provide here extra information for a better understanding of the capabilities for the xpad camera.

HwDetInfo
~~~~~~~~~

 - 16 or 32 bit unsigned type are supported
 - the size of the image will depend of the type of Xpad

HwSync
~~~~~~

Trigger type supported are:

	- IntTrig
	- ExtTrigSingle
	- ExtGate : 1 external trigger start N internal gates (gates being configured by software)
	- ExtTrigMult : N external trigger start N internal gates (gates being configured by software)

Optional capabilities
.....................

There are no optional capabilities.

Configuration
`````````````

No Specific hardware configuration is needed.

How to use
````````````

Here is a list of accessible fonctions to configure and use the Xpad detector:

.. code-block:: cpp

	//! Set all the config G
	void setAllConfigG(const std::vector<long>& allConfigG);
	//!	Set the Acquisition type between synchrone and asynchrone
	void setAcquisitionType(short acq_type);
	//!	Load of flat config of value: flat_value (on each pixel)
	void loadFlatConfig(unsigned flat_value);
	//! Load all the config G
	void loadAllConfigG(unsigned long modNum, unsigned long chipId , unsigned long* config_values);
	//! Load a wanted config G with a wanted value
	void loadConfigG(const std::vector<unsigned long>& reg_and_value);
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
	void calibrateOTNSlow (const std::string& path);
	//! Calibrate over the noise Medium and save dacl and configg files in path
	void calibrateOTNMedium (const std::string& path);
	//! Calibrate over the noise High and save dacl and configg files in path
	void calibrateOTNHigh (const std::string& path);
	//! upload the calibration (dacl + config) that is stored in path
	void uploadCalibration(const std::string& path);
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
								unsigned busy_out_sel,
								bool geom_corr,
								unsigned GP1,     unsigned GP2,    unsigned GP3,      unsigned GP4);

	//! Set the Calibration Adjusting number of iteration
	void setCalibrationAdjustingNumber(unsigned calibration_adjusting_number);
