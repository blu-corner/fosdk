set (INSTALL_HEADERS
  gwcOptiq.h
  )

set (SOURCES
  gwcOptiq.cpp
  )

include_directories(
  ${CMAKE_INSTALL_PREFIX}/include/codec/optiq
  )

add_library (gwcoptiq SHARED ${SOURCES})
target_link_libraries (gwcoptiq gwc optiqcodec)

install (TARGETS gwcoptiq
         LIBRARY DESTINATION lib)
install (FILES ${INSTALL_HEADERS} DESTINATION include/gwc)
