###########################################################################
# This file is part of LImA, a Library for Image Acquisition
#
#  Copyright (C) : 2009-2017
#  European Synchrotron Radiation Facility
#  CS40220 38043 Grenoble Cedex 9 
#  FRANCE
# 
#  Contact: lima@esrf.fr
# 
#  This is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
# 
#  This software is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, see <http://www.gnu.org/licenses/>.
############################################################################

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--add-needed")

set(xpad_srcs src/XpadCamera.cpp  src/XpadInterface.cpp
	 src/XpadDetInfoCtrlObj.cpp src/XpadSyncCtrlObj.cpp
	 src/XpadBufferCtrlObj.cpp src/XpadEventCtrlObj.cpp)

file(STRINGS "VERSION" xpad_vers)
add_library(limaxpad SHARED ${xpad_srcs})

target_include_directories(limaxpad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(limaxpad PUBLIC "/usr/local/yat/include")

find_library(XPAD_LIB_XPCI xpci_lib)
find_library(XPAD_LIB_YAT yat /usr/local/yat/lib64)

target_link_libraries(limaxpad limacore)
target_link_libraries(limaxpad ${XPAD_LIB_XPCI})
target_link_libraries(limaxpad ${XPAD_LIB_YAT})

set_target_properties(limaxpad PROPERTIES VERSION ${xpad_vers} SOVERSION "1")

install(TARGETS limaxpad LIBRARY DESTINATION lib)

if(COMPILE_SIP)
    set(NAME "xpad")
    include(CameraRunSIP)
    install(FILES python/__init__.py DESTINATION "${PYTHON_SITE_PACKAGES_DIR}/Lima/Xpad")
endif()
