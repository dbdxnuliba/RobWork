SET(component_name serialport)
SET(SUBSYS_NAME serialport )
SET(SUBSYS_DESC "Serial port abstractions!" )
SET(SUBSYS_DEPS rw )

SET(build )
set(DEFAULT TRUE)
set(REASON "")
RW_SUBSYS_OPTION( build ${SUBSYS_NAME} ${SUBSYS_DESC} ${DEFAULT} ${REASON})
RW_SUBSYS_DEPEND( build ${SUBSYS_NAME} DEPS ${SUBSYS_DEPS} )
RW_ADD_DOC( ${SUBSYS_NAME} )

IF( build )
    RW_ADD_LIBRARY(rwhw_serialport serialport SerialPort.cpp)
    TARGET_LINK_LIBRARIES(rwhw_serialport PUBLIC ${ROBWORK_LIBRARIES})
    RW_ADD_INCLUDES(serialport "rwhw/serialport" SerialPort.hpp)
    
    SET( ENV{ROBWORKHARDWARE_LIBRARIES} "$ENV{ROBWORKHARDWARE_LIBRARIES}rwhw_serialport;")
ENDIF()
