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
    SET(INCLUDES)
    FILE(GLOB sipfiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/sip" "${CMAKE_CURRENT_SOURCE_DIR}/sip/*.sip")
    FOREACH(sipfile ${sipfiles})
        SET(INCLUDES 
        "${INCLUDES} 
        %Include ${sipfile}")
    ENDFOREACH()

    CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/sip/limamodules.sip.in sip/limaxpad.sip)
    SET(SIP_CONCAT_PARTS 1)
    SET(SIP_INCLUDES ${SIP_INCLUDES}
        "${CMAKE_SOURCE_DIR}/third-party/Processlib/sip"
        "${CMAKE_BINARY_DIR}/sip/core"
        "${CMAKE_SOURCE_DIR}/third-party/Processlib/tasks/sip"
        "${CMAKE_SOURCE_DIR}/common/sip"
        "${CMAKE_SOURCE_DIR}/hardware/sip"
        "${CMAKE_SOURCE_DIR}/control/sip"
        "${CMAKE_SOURCE_DIR}/control/software_operation/sip"
        "${CMAKE_CURRENT_SOURCE_DIR}/sip")
    set(SIP_DISABLE_FEATURES WITH_CONFIG)
    set(SIP_CHECK_EXC ON)
    ADD_SIP_PYTHON_MODULE(limaxpad ${CMAKE_CURRENT_BINARY_DIR}/sip/limaxpad.sip)
    TARGET_INCLUDE_DIRECTORIES(python_module_limaxpad PRIVATE
                               ${PYTHON_INCLUDE_DIRS}
                               "${CMAKE_SOURCE_DIR}/sip"
                               "${CMAKE_SOURCE_DIR}/sip/core")
    TARGET_LINK_LIBRARIES(python_module_limaxpad limaxpad)

    INSTALL(FILES python/__init__.py DESTINATION "${PYTHON_SITE_PACKAGES_DIR}/Lima/Xpad")
ENDIF()
