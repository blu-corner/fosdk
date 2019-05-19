set (INSTALL_HEADERS
  gwcMillennium.h
  )

set (SOURCES
  gwcMillennium.cpp
  )

include_directories(
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/lse
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/lse/packets
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/oslo
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/oslo/packets
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/turquoise
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/turquoise/packets
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/jse
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/jse/packets
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/borsa
  ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/borsa/packets
  )

add_library (gwcmillennium SHARED ${SOURCES})
target_link_libraries (gwcmillennium gwc lsecodec oslocodec turquoisecodec)

install (TARGETS gwcmillennium
         RUNTIME DESTINATION lib
         ARCHIVE DESTINATION lib
         LIBRARY DESTINATION lib)
install (FILES ${INSTALL_HEADERS} DESTINATION include/gwc)
