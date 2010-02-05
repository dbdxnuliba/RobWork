# Find and sets up RobWorkHardware
# 
#  ROBWORKHARDWARE_INCLUDE_DIR - Where to find robwork include sub-directory.
#  ROBWORKHARDWARE_LIBRARIES   - List of libraries when using RobWork (includes all libraries that RobWork depends on).
#  ROBWORKHARDWARE_LIBRARY_DIRS - List of directories where libraries of RobWork are located. 
#  ROBWORKHARDWARE_FOUND       - True if RobWork was found. (not impl yet)
#
#  ROBWORKHARDWARE_ROOT             - If set this defines the root of RobWorkHardware if not set then it
#                              if possible be autodetected.

# Allow the syntax else (), endif (), etc.
SET(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS 1)

# Check if RW_ROOT path are setup correctly
FIND_FILE(ROBWORKHARDWARE_FOUND FindRobWorkHardware.cmake ${ROBWORKHARDWARE_ROOT}/build NO_DEFAULT_PATH)
IF(NOT ROBWORKHARDWARE_FOUND)
 MESSAGE(SEND_ERROR "Path to RobWorkHardware root (ROBWORKHARDWARE_ROOT) is incorrectly setup! \nROBWORKHARDWARE_ROOT  ==${ROBWORKHARDWARE_ROOT}")
ENDIF()

#Compiler flags
IF (DEFINED MSVC)
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D QT_NO_DEBUG")
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -D QT_NO_DEBUG")
  SET(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -D QT_NO_DEBUG")
ELSE ()
  ADD_DEFINITIONS(-DQT_NO_DEBUG)
ENDIF ()

# Setup the libraries
IF (RWHARDWARE_BUILD_WITH_SANDBOX)
  SET(ROBWORKHARDWARE_SANDBOX_LIB rwhw_sandbox)
  SET(ROBWORKHARDWARE_HAVE_SANDBOX ON)
ELSE ()
  SET(ROBWORKHARDWARE_HAVE_SANDBOX OFF) 
ENDIF ()

SET(ROBWORKHARDWARE_INCLUDE_DIRS ${ROBWORKHARDWARE_ROOT}/src/)

SET(ROBWORKHARDWARE_LIBRARIES
    	${ROBWORKHARDWARE_SANDBOX_LIB}
	rwhw_can
	rwhw_crsa465
#	rwhw_dockwelder
#	rwhw_fanucdevice
#	rwhw_katana
	rwhw_motomanIA20
#	rwhw_pa10
	rwhw_pcube
#	rwhw_sdh
	rwhw_serialport
#	rwhw_sick
#	rwhw_swissranger   
	rwhw_tactile
)


IF (CMAKE_COMPILER_IS_GNUCXX)
    IF (DEFINED MINGW)
        # TODO mingw32 libraries
    ELSE()
        INCLUDE(${ROBWORKHARDWARE_ROOT}/build/FindDC1394.cmake)
        #INCLUDE(${ROBWORKHARDWARE_ROOT}/build/FindRAW1394.cmake)
        IF(DC1394_FOUND) #AND RAW1394_FOUND)
            MESSAGE(STATUS "RobWork Hardware Camera: Included!")
            SET(ROBWORKHARDWARE_LIBRARIES 
            ${ROBWORKHARDWARE_LIBRARIES} rwhw_camera ${DC1394_LIBRARY} )#${RAW1394_LIBRARY})
            SET(ROBWORKHARDWARE_INCLUDE_DIRS ${ROBWORKHARDWARE_INCLUDE_DIRS} ${DC1394_INCLUDE_DIR})# ${RAW1394_INCLUDE_DIR})
        ELSE()
            MESSAGE(STATUS "RobWork Hardware Camera: Not included!")
        ENDIF()
    ENDIF()
ELSEIF (DEFINED MSVC)
    # TODO MSVC AND CMU1394
ENDIF()


IF(DEFINED MSVC)
  SET(ROBWORKHARDWARE_LIBS_DIR "${ROBWORKHARDWARE_ROOT}/libs/")
ELSE()
  SET(ROBWORKHARDWARE_LIBS_DIR "${ROBWORKHARDWARE_ROOT}/libs/${CMAKE_BUILD_TYPE}/")
ENDIF()

# Setup RobWorkHardware include and link directories
SET(ROBWORKHARDWARE_INCLUDE_DIR ${ROBWORKHARDWARE_INCLUDE_DIRS})
SET(ROBWORKHARDWARE_LIBRARY_DIRS ${ROBWORKHARDWARE_LIBS_DIR})

MESSAGE(STATUS "Path to RobWorkHardware root (ROBWORKHARDWARE_ROOT) = ${ROBWORKHARDWARE_ROOT} \n
Path to RobWorkHardware includes dir (ROBWORKHARDWARE_INCLUDE_DIR) = ${ROBWORKHARDWARE_INCLUDE_DIR} \n
Path to RobWorkHardware libraries dir (ROBWORKHARDWARE_LIBRARY_DIRS) = ${ROBWORKHARDWARE_LIBRARY_DIRS}\n
RobWork Hardware libraties: ${ROBWORKHARDWARE_LIBRARIES}")

