set (INSTALL_HEADERS
  gwcSwx.h
  )

set (SOURCES
  gwcSwx.cpp
  )

include_directories(
  ${PROJECT_SOURCE_DIR}/src/soupbin
  ${CMAKE_INSTALL_PREFIX}/include/codec/swx
  ${CMAKE_INSTALL_PREFIX}/include/codec/swx/packets
  )

add_library (gwcswx SHARED ${SOURCES})
target_link_libraries (gwcswx gwcsoupbin swxcodec)

install (TARGETS gwcswx
         LIBRARY DESTINATION lib)
install (FILES ${INSTALL_HEADERS} DESTINATION include/gwc)
