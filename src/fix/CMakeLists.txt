#
# Copyright 2014-2018 Neueda Ltd.
#

find_package(LibXml2 REQUIRED)

set (INSTALL_HEADERS
  gwcFix.h
  )

set (SOURCES
  gwcFix.cpp
  msgWriter.cpp
  )

include_directories(
  ${LIBXML2_INCLUDE_DIR}
  ${CMAKE_INSTALL_PREFIX}/include/codec/fix
  )

add_library (gwcfix SHARED ${SOURCES})
target_link_libraries (gwcfix gwc fixcodec)

install (TARGETS gwcfix
         RUNTIME DESTINATION lib
         ARCHIVE DESTINATION lib
         LIBRARY DESTINATION lib)
install (FILES ${INSTALL_HEADERS} DESTINATION include/gwc)
