############################################################################
# This file is part of LImA, a Library for Image Acquisition
#
# Copyright (C) : 2009-2017
# European Synchrotron Radiation Facility
# BP 220, Grenoble 38043
# FRANCE
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
############################################################################
# CMakeLists.txt for Xpad camera

SET(CMAKE_SHARED_LINKER_FLAGS "-Wl,--add-needed")

SET(xpad_srcs src/XpadCamera.cpp  src/XpadInterface.cpp
	 src/XpadDetInfoCtrlObj.cpp src/XpadSyncCtrlObj.cpp
	 src/XpadBufferCtrlObj.cpp src/XpadEventCtrlObj.cpp)

FILE(STRINGS "VERSION" xpad_vers)
ADD_LIBRARY(limaxpad SHARED ${xpad_srcs})

TARGET_INCLUDE_DIRECTORIES(limaxpad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
TARGET_INCLUDE_DIRECTORIES(limaxpad PUBLIC "${YAT_ROOT}/include")

FIND_LIBRARY(XPAD_LIB_XPCI xpci_lib)
FIND_LIBRARY(XPAD_LIB_YAT yat ${YAT_ROOT}/lib64)

TARGET_LINK_LIBRARIES(limaxpad limacore)
TARGET_LINK_LIBRARIES(limaxpad ${XPAD_LIB_XPCI})
TARGET_LINK_LIBRARIES(limaxpad ${XPAD_LIB_YAT})

SET_TARGET_PROPERTIES(limaxpad PROPERTIES VERSION ${xpad_vers} SOVERSION "1")

INSTALL(TARGETS limaxpad LIBRARY DESTINATION lib)

IF(COMPILE_SIP)
    SET(NAME "xpad")
    include(CameraRunSIP)
    INSTALL(FILES python/__init__.py DESTINATION "${PYTHON_SITE_PACKAGES_DIR}/Lima/Xpad")
ENDIF()
