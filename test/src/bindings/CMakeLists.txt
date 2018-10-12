include_directories(
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/src/millennium
  ${PROJECT_SOURCE_DIR}/src/soupbin
  ${PROJECT_SOURCE_DIR}/src/swx
  ${CMAKE_INSTALL_PREFIX}/include/
  ${CMAKE_INSTALL_PREFIX}/include/cdr
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/lse
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/lse/packets
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/oslo
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/oslo/packets
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/turquoise
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/turquoise/packets
  ${CMAKE_INSTALL_PREFIX}/include/codec/swx
  ${CMAKE_INSTALL_PREFIX}/include/codec/swx/packets
  )

if(PYTHON OR JAVA OR CSHARP)
  find_package(SWIG REQUIRED)
  include(${SWIG_USE_FILE})
endif()

if(PYTHON)
  add_subdirectory(python)
endif()

if(JAVA)
  add_subdirectory(java)
endif()

if(CSHARP)
  add_subdirectory(csharp)
endif()
