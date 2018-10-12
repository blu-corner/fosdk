set (INSTALL_HEADERS
  gwcXetra.h
  )

set (SOURCES
  gwcXetra.cpp
  )

include_directories(
  ${CMAKE_INSTALL_PREFIX}/include/codec/xetra
  )

add_library (gwcxetra SHARED ${SOURCES})
target_link_libraries (gwcxetra gwc xetracodec)

install (TARGETS gwcxetra
         LIBRARY DESTINATION lib)
install (FILES ${INSTALL_HEADERS} DESTINATION include/gwc)
