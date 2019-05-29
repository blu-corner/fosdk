set (INSTALL_HEADERS
  gwcEti.h
  )

set (SOURCES
  gwcEti.cpp
  )

include_directories(
  ${CMAKE_INSTALL_PREFIX}/include/codec/eti
  ${CMAKE_INSTALL_PREFIX}/include/codec/eti/xetra
  ${CMAKE_INSTALL_PREFIX}/include/codec/eti/eurex
  )

add_library (gwceti SHARED ${SOURCES})
target_link_libraries (gwceti gwc xetracodec eurexcodec)

install (TARGETS gwceti
         RUNTIME DESTINATION lib
         ARCHIVE DESTINATION lib
         LIBRARY DESTINATION lib)
install (FILES ${INSTALL_HEADERS} DESTINATION include/gwc)
