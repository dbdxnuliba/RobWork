SET(SUBSYS_NAME rws_lua )
SET(SUBSYS_DESC "Interface for accessing RobWorkStudio from lua." )
SET(SUBSYS_DEPS rw )

SET(build TRUE)

FIND_PACKAGE(SWIG 3.0.0 QUIET)
set(DEFAULT TRUE)
set(REASON )
IF( NOT SWIG_FOUND)
    set(DEFAULT false)
    set(REASON "SWIG not found!")
else()

endif()
 
RW_SUBSYS_OPTION( build ${SUBSYS_NAME} ${SUBSYS_DESC} ${DEFAULT} ${REASON})
RW_SUBSYS_DEPEND( build ${SUBSYS_NAME} DEPS ${SUBSYS_DEPS} )
RW_ADD_DOC( ${SUBSYS_NAME} )

IF( build )
   SET(RWSIM_HAVE_LUA TRUE CACHE INTERNAL "")
	#MESSAGE(STATUS "SWIG found adding swig modules!")
	INCLUDE(UseSWIG)
	
	SET(CMAKE_SWIG_FLAGS "")

	SET_SOURCE_FILES_PROPERTIES(../rws.i PROPERTIES CPLUSPLUS ON)
	SET_SOURCE_FILES_PROPERTIES(../rws.i PROPERTIES SWIG_FLAGS "-includeall")
	
	# lua interface generation
	IF ((CMAKE_VERSION VERSION_GREATER 3.8) OR (CMAKE_VERSION VERSION_EQUAL 3.8))
		SWIG_ADD_LIBRARY(rws_lua TYPE MODULE LANGUAGE lua SOURCES ../rws.i Lua.cpp ../ScriptTypes.cpp)
	ELSE()
		SWIG_ADD_MODULE(rws_lua lua ../rws.i Lua.cpp ../ScriptTypes.cpp)
	ENDIF()
    SWIG_LINK_LIBRARIES(rws_lua
                ${RWS_COMPONENT_LIBRARIES}
                ${RWS_PLUGIN_LIBRARIES}
                rws
                ${ROBWORK_LIBRARIES}
    )
    IF((CMAKE_COMPILER_IS_GNUCC) OR (CMAKE_C_COMPILER_ID STREQUAL "Clang"))
      SET_TARGET_PROPERTIES(${SWIG_MODULE_rws_lua_REAL_NAME} PROPERTIES LINK_FLAGS -Wl,--no-undefined)
    ENDIF()

    IF ((CMAKE_VERSION VERSION_GREATER 3.12.0) OR (CMAKE_VERSION VERSION_EQUAL 3.12.0))
        SWIG_ADD_LIBRARY(rws_lua_s TYPE STATIC LANGUAGE lua SOURCES ../rws.i Lua.cpp ../ScriptTypes.cpp)
    ELSE()
        ADD_LIBRARY(rws_lua_s STATIC ${swig_generated_sources} ${swig_other_sources})
    ENDIF()
    TARGET_LINK_LIBRARIES(rws_lua_s
                ${RWS_COMPONENT_LIBRARIES}
                ${RWS_PLUGIN_LIBRARIES}
                rws
                ${ROBWORK_LIBRARIES}
    )

    SET(ENV{RWS_COMPONENT_LIBRARIES} "$ENV{RWS_COMPONENT_LIBRARIES}rws_lua_s;")
    #MESSAGE("${ROBWORKSTUDIO_LIBRARIES} \n ROBWORK ${ROBWORK_LIBRARIES}" )
    # this is used to indicate static linking to Visual Studio or mingw 
    	# this is used to indicate static linking to Visual Studio or mingw
	IF (DEFINED MSVC) 
	    SET_TARGET_PROPERTIES(rws_lua_s PROPERTIES COMPILE_FLAGS "/DSTATIC_LINKED")
	ELSE()
	    SET_TARGET_PROPERTIES(rws_lua_s PROPERTIES COMPILE_FLAGS "-DSTATIC_LINKED")
	ENDIF()
    	
    	
	ADD_LIBRARY(rwslua_plugin.rwplugin MODULE LuaPlugin.cpp LuaPlugin.hpp)
    TARGET_LINK_LIBRARIES(rwslua_plugin.rwplugin rws_lua_s ${ROBWORK_LIBRARIES})
    IF((CMAKE_COMPILER_IS_GNUCC) OR (CMAKE_C_COMPILER_ID STREQUAL "Clang"))
      SET_TARGET_PROPERTIES(rwslua_plugin.rwplugin PROPERTIES LINK_FLAGS -Wl,--no-undefined)
    ENDIF()
	
	
ENDIF()
