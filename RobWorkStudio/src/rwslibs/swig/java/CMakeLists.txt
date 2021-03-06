SET(SUBSYS_NAME rws_java )
SET(SUBSYS_DESC "Interface for accessing RobWorkStudio from java." )
SET(SUBSYS_DEPS rw )

SET(build TRUE)

FIND_PACKAGE(SWIG 3.0.0 QUIET)
set(DEFAULT TRUE)
set(REASON )
IF( NOT SWIG_FOUND)
    set(DEFAULT false)
    set(REASON "SWIG not found!")
else()

    FIND_PACKAGE(Java)	
    FIND_PACKAGE(JNI)
    if( NOT (JAVA_FOUND AND JNI_FOUND) )
        set(DEFAULT false)
        set(REASON "JAVA or JNI not found!")
    endif()
endif()
 
RW_SUBSYS_OPTION( build ${SUBSYS_NAME} ${SUBSYS_DESC} ${DEFAULT} ${REASON})
RW_SUBSYS_DEPEND( build ${SUBSYS_NAME} DEPS ${SUBSYS_DEPS} )
RW_ADD_DOC( ${SUBSYS_NAME} )

IF( build )
    INCLUDE(UseJava)
    INCLUDE(UseSWIG)
    
    SET_SOURCE_FILES_PROPERTIES(../rws.i PROPERTIES CPLUSPLUS ON)
	SET_SOURCE_FILES_PROPERTIES(../rws.i PROPERTIES SWIG_FLAGS "-includeall")
    
    INCLUDE_DIRECTORIES(${JAVA_INCLUDE_DIRS} ${JNI_INCLUDE_DIRS})
    set(CMAKE_SWIG_FLAGS "-package" "dk.robwork")
    # Put java files in different directory suitable for JAR generation later on
    SET(CMAKE_SWIG_OUTDIR ${CMAKE_CURRENT_BINARY_DIR}/java_src/dk/robwork)
    # SWIG
	IF ((CMAKE_VERSION VERSION_GREATER 3.8) OR (CMAKE_VERSION VERSION_EQUAL 3.8))
		SWIG_ADD_LIBRARY(rws_jni MODULE LANGUAGE java SOURCES ../rws.i ../ScriptTypes.cpp)
	ELSE()
		SWIG_ADD_MODULE(rws_jni java ../rws.i ../ScriptTypes.cpp)
	ENDIF()
    SWIG_LINK_LIBRARIES(rws_jni 
        ${RWS_COMPONENT_LIBRARIES}
        ${RWS_PLUGIN_LIBRARIES}
        rws
        ${ROBWORK_LIBRARIES} )
    IF((CMAKE_COMPILER_IS_GNUCC) OR (CMAKE_C_COMPILER_ID STREQUAL "Clang"))
      SET_TARGET_PROPERTIES(${SWIG_MODULE_rws_jni_REAL_NAME} PROPERTIES LINK_FLAGS -Wl,--no-undefined)
    ENDIF()
    # Force removal of previous Java compilation and source when interface file changes
    # This is required as types may be removed or change name
    # (in this case previous java classes would interfere with current compilation).
	ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/CleanDep
        COMMAND ${CMAKE_COMMAND} -E remove_directory java_src
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/CleanDep
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../rws.i"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Removing old Java source..."
    )
    ADD_CUSTOM_TARGET(CleanDep DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/CleanDep )
    ADD_DEPENDENCIES(rws_jni CleanDep)
    # Compile java code and create JAR and Javadoc
	ADD_CUSTOM_COMMAND(TARGET rws_jni
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E echo "Removing old Java compilation..."
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/java_build"
    	COMMAND ${CMAKE_COMMAND} -E remove_directory "${RWS_CMAKE_LIBRARY_OUTPUT_DIRECTORY}/javadoc"
		COMMAND ${CMAKE_COMMAND} -E echo "Copying Java source..."
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/LoaderRWS.java java_src/dk/robwork/LoaderRWS.java
		COMMAND ${CMAKE_COMMAND} -E echo "Compiling Java files..."
        COMMAND ${CMAKE_COMMAND} -E make_directory java_build/dk/robwork
        COMMAND ${Java_JAVAC_EXECUTABLE} -cp ${RW_LIBS}/rw_java.jar -d ${CMAKE_CURRENT_BINARY_DIR}/java_build java_src/dk/robwork/*.java
        COMMAND ${CMAKE_COMMAND} -E echo "Creating jar file..."
        COMMAND ${Java_JAR_EXECUTABLE} cvf ${RWS_CMAKE_LIBRARY_OUTPUT_DIRECTORY}/rws_java.jar  -C java_build .
		COMMAND ${CMAKE_COMMAND} -E echo "Creating Javadoc..."
    	COMMAND ${CMAKE_COMMAND} -E make_directory ${RWS_CMAKE_LIBRARY_OUTPUT_DIRECTORY}/javadoc
    	COMMAND ${Java_JAVADOC_EXECUTABLE}
    					-classpath ${RW_LIBS}/rw_java.jar
    					-d ${RWS_CMAKE_LIBRARY_OUTPUT_DIRECTORY}/javadoc
    					-windowtitle "RobWorkStudio Java API Documentation"
    					-public
    					-sourcepath java_src dk.robwork
    					-linkoffline ${RW_LIBS}/javadoc ${RW_LIBS}/javadoc
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    SET(CMAKE_SWIG_OUTDIR ${RWS_CMAKE_LIBRARY_OUTPUT_DIRECTORY})

ENDIF()
