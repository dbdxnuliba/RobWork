SET(SUBSYS_NAME rws_python )
SET(SUBSYS_DESC "Interface for accessing RobWorkStudio from python." )
SET(SUBSYS_DEPS rw )

SET(build TRUE)

FIND_PACKAGE(SWIG 3.0.0 QUIET)

set(DEFAULT TRUE)
set(REASON )
IF( NOT SWIG_FOUND)
    set(DEFAULT false)
    set(REASON "SWIG not found!")
else()

    FIND_PACKAGE(PythonInterp QUIET)
    FIND_PACKAGE(PythonLibs QUIET)

    if( NOT (PYTHONLIBS_FOUND AND PYTHONINTERP_FOUND) )
        set(DEFAULT false)
        set(REASON "PYTHONLIBS AND PYTHONINTERP not found!")
    endif()
endif()
 
RW_SUBSYS_OPTION( build ${SUBSYS_NAME} ${SUBSYS_DESC} ${DEFAULT} ${REASON})
RW_SUBSYS_DEPEND( build ${SUBSYS_NAME} DEPS ${SUBSYS_DEPS} )
RW_ADD_DOC( ${SUBSYS_NAME} )

IF( build )
INCLUDE(UseSWIG)
    INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_DIRS})
    SET_SOURCE_FILES_PROPERTIES(../rws.i PROPERTIES CPLUSPLUS ON)
	SET_SOURCE_FILES_PROPERTIES(../rws.i PROPERTIES SWIG_FLAGS "-includeall")
	
	SET(CMAKE_SWIG_OUTDIR ${RWS_CMAKE_LIBRARY_OUTPUT_DIRECTORY})
	IF ((CMAKE_VERSION VERSION_GREATER 3.8) OR (CMAKE_VERSION VERSION_EQUAL 3.8))
		SWIG_ADD_LIBRARY(rws TYPE MODULE LANGUAGE python SOURCES ../rws.i ../ScriptTypes.cpp)
	ELSE()
		SWIG_ADD_MODULE(rws python ../rws.i ../ScriptTypes.cpp)
	ENDIF()
    SWIG_LINK_LIBRARIES(rws
        ${RWS_COMPONENT_LIBRARIES} 
        ${RWS_PLUGIN_LIBRARIES} 
        rws
        ${ROBWORK_LIBRARIES} 
        ${PYTHON_LIBRARIES} )
    IF((CMAKE_COMPILER_IS_GNUCC) OR (CMAKE_C_COMPILER_ID STREQUAL "Clang"))
      SET_TARGET_PROPERTIES(${SWIG_MODULE_rws_REAL_NAME} PROPERTIES LINK_FLAGS -Wl,--no-undefined)
    ENDIF()
     
ENDIF()
