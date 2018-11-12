# Include Java
find_package(Java REQUIRED COMPONENTS Runtime Development)
find_package(JNI REQUIRED)
include(UseJava)

include_directories(${JNI_INCLUDE_DIRS})

# set the flags for swig
set(CMAKE_SWIG_FLAGS -package com.neueda.fosdk)
set_source_files_properties(../fosdk.java.i PROPERTIES CPLUSPLUS ON)

# Add swig module
add_compile_options(-fno-strict-aliasing)
swig_add_module(Fosdk java ../fosdk.java.i)
swig_link_libraries(Fosdk gwc cdr utils logger ${JNI_LIBRARIES})

set(CMAKE_JAR_CLASSES_PREFIX com/neueda/fosdk)
set(CMAKE_JAVA_INCLUDE_PATH
  ${CMAKE_INSTALL_PREFIX}/lib/java/CodecJNI.jar
  ${CMAKE_INSTALL_PREFIX}/lib/java/CdrJNI.jar
  ${CMAKE_INSTALL_PREFIX}/lib/java/PropertiesJNI.jar
  ${CMAKE_INSTALL_PREFIX}/lib/java/LoggerJNI.jar)

add_jar(FosdkJNI
  SOURCES
${CMAKE_CURRENT_BINARY_DIR}/Fosdk.java
${CMAKE_CURRENT_BINARY_DIR}/FosdkJNI.java
${CMAKE_CURRENT_BINARY_DIR}/gwcConnectorFactory.java
${CMAKE_CURRENT_BINARY_DIR}/gwcConnector.java
${CMAKE_CURRENT_BINARY_DIR}/gwcConnectorState.java
${CMAKE_CURRENT_BINARY_DIR}/gwcMessageCallbacks.java
${CMAKE_CURRENT_BINARY_DIR}/gwcOrder.java
${CMAKE_CURRENT_BINARY_DIR}/gwcOrderType.java
${CMAKE_CURRENT_BINARY_DIR}/gwcSessionCallbacks.java
${CMAKE_CURRENT_BINARY_DIR}/gwcSide.java
${CMAKE_CURRENT_BINARY_DIR}/gwcTif.java
${CMAKE_CURRENT_BINARY_DIR}/SWIGTYPE_p_std__string.java
${CMAKE_CURRENT_BINARY_DIR}/SWIGTYPE_p_void.java
${CMAKE_CURRENT_BINARY_DIR}/SWIGTYPE_p_neueda__Buffer.java
)

if(TARGET _Fosdk)
  add_dependencies(FosdkJNI _Fosdk)
else()
  add_dependencies(FosdkJNI Fosdk)
endif()

# Files to install with Java
if(APPLE)
  set(JAVA_DYLIB_SUFFIX ".jnilib")
else()
  set(JAVA_DYLIB_SUFFIX ".so")
endif()

# cmake 2.8 swig has issues on figuring out naming libs
if(TARGET _Fosdk)
  set_target_properties(_Fosdk PROPERTIES OUTPUT_NAME "Fosdk")
  set_target_properties(_Fosdk PROPERTIES SUFFIX ${JAVA_DYLIB_SUFFIX})
endif()

set(JAVA_INSTALL_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/run.sh
  ${CMAKE_CURRENT_SOURCE_DIR}/build.sh
  ${CMAKE_CURRENT_SOURCE_DIR}/example.java
  ${CMAKE_CURRENT_BINARY_DIR}/FosdkJNI.jar)

# Declare install target for java
install(FILES ${JAVA_INSTALL_FILES}
  DESTINATION lib/java)

if(TARGET _Fosdk)
  install(TARGETS _Fosdk
    DESTINATION lib/java)
else()
  install(TARGETS Fosdk
    DESTINATION lib/java)
endif()

