set (INSTALL_HEADERS
  gwcSoupBin.h
  )

set (SOURCES
  gwcSoupBin.cpp
  )

add_library (gwcsoupbin SHARED ${SOURCES})
target_link_libraries (gwcsoupbin gwc)

install (TARGETS gwcsoupbin
         LIBRARY DESTINATION lib)
install (FILES ${INSTALL_HEADERS} DESTINATION include/gwc)
