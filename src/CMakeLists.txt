set (INSTALL_HEADERS
  gwcCommon.h
  gwcConnector.h
  )

set (SOURCES
  gwcConnector.cpp
  )

link_directories(
    ${CMAKE_INSTALL_PREFIX}/lib
    )

include_directories(
    ${PROJECT_SOURCE_DIR}/src/
    ${CMAKE_INSTALL_PREFIX}/include
    ${CMAKE_INSTALL_PREFIX}/include/cdr
    ${CMAKE_INSTALL_PREFIX}/include/codec
    ${CMAKE_INSTALL_PREFIX}/include/codec/fields
    ${CMAKE_INSTALL_PREFIX}/include/codec/optiq/packets
    ${CMAKE_INSTALL_PREFIX}/include/properties
    ${CMAKE_INSTALL_PREFIX}/include/logger
    ${CMAKE_INSTALL_PREFIX}/include/sbf
    ${CMAKE_INSTALL_PREFIX}/include/sbf/cpp
    ${CMAKE_INSTALL_PREFIX}/include/utils
  )

add_library (gwc SHARED ${SOURCES})
target_link_libraries (gwc cdr utils properties logger sbfcore sbfcommon sbfnetwork)

install (TARGETS gwc
  EXPORT gwc
  RUNTIME DESTINATION lib
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)
install (FILES ${INSTALL_HEADERS} DESTINATION include/gwc)

# exports
install(
  EXPORT gwc
  DESTINATION lib/cmake/fosdk COMPONENT dev
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  )

set(${PROJECT_NAME}_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)
set(${PROJECT_NAME}_LIBRARIES gwc)
configure_file(
  ${PROJECT_SOURCE_DIR}/fosdk.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/gwcConfig-install.cmake
  @ONLY
  )
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/gwcConfig-install.cmake
  DESTINATION lib/cmake/${PROJECT_NAME} COMPONENT dev
  RENAME gwcConfig.cmake
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  )

# subdirs
add_subdirectory(millennium)
add_subdirectory(soupbin)
add_subdirectory(swx)
add_subdirectory(eti)
add_subdirectory(optiq)
add_subdirectory(fix)
add_subdirectory(bindings)
