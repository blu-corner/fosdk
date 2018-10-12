find_program(MCS mcs)
if(NOT MCS)
  message(FATAL_ERROR "Failed to find mono compiler")
endif()
message(STATUS "Found mono compiler: ${MCS}")

# set the flags for swig
set(CMAKE_SWIG_FLAGS -namespace Neueda.Fosdk -dllimport FosdkCsharp)
set_source_files_properties(../fosdk.csharp.i PROPERTIES CPLUSPLUS ON)

# Add swig module
add_compile_options(-fno-strict-aliasing)
swig_add_module(FosdkCsharp csharp ../fosdk.csharp.i)
swig_link_libraries(FosdkCsharp gwc cdr utils logger)

# Files to install with csharp
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Fosdk.dll
  COMMAND ${MCS} -target:library -out:${CMAKE_CURRENT_BINARY_DIR}/Fosdk.dll -reference:${CMAKE_INSTALL_PREFIX}/lib/csharp/Config.dll -reference:${CMAKE_INSTALL_PREFIX}/lib/csharp/Log.dll -reference:${CMAKE_INSTALL_PREFIX}/lib/csharp/Codecs.dll -reference:${CMAKE_INSTALL_PREFIX}/lib/csharp/Cdr.dll ${CMAKE_CURRENT_BINARY_DIR}/*.cs
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CSHARP_SRC_FILES}
  COMMENT "Generating Csharp DLL"
  VERBATIM
  )
add_custom_target(CsharpDLL ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Fosdk.dll)

if (TARGET _FosdkCsharp)
  add_dependencies(CsharpDLL _FosdkCsharp)
else()
  add_dependencies(CsharpDLL FosdkCsharp)
endif()

if(APPLE)
  set(CSHARP_DYLIB_PREFIX "")
  set(CSHARP_DYLIB_SUFFIX ".so")
else()
  set(CSHARP_DYLIB_PREFIX "lib")
  set(CSHARP_DYLIB_SUFFIX ".so")
endif()

# cmake 2.8 swig has issues on figuring out naming libs
if(TARGET _FosdkCsharp)
  set_target_properties(_FosdkCsharp PROPERTIES OUTPUT_NAME "FosdkCsharp")
  set_target_properties(_FosdkCsharp PROPERTIES SUFFIX ${CSHARP_DYLIB_SUFFIX})
endif()

set(CSHARP_INSTALL_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/run.sh
  ${CMAKE_CURRENT_SOURCE_DIR}/build.sh
  ${CMAKE_CURRENT_SOURCE_DIR}/example.cs
  ${CMAKE_CURRENT_BINARY_DIR}/Fosdk.dll)

# Declare install target for csharp
install(FILES ${CSHARP_INSTALL_FILES}
  DESTINATION lib/csharp)

if(TARGET _FosdkCsharp)
  install(TARGETS _FosdkCsharp
    DESTINATION lib/csharp)
else()
  install(TARGETS FosdkCsharp
    DESTINATION lib/csharp)
endif()
