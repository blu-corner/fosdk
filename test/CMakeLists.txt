include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/millennium
    ${CMAKE_INSTALL_PREFIX}/include
    ${CMAKE_INSTALL_PREFIX}/include/logger
    ${CMAKE_INSTALL_PREFIX}/include/properties
    ${CMAKE_INSTALL_PREFIX}/include/utils
    ${CMAKE_INSTALL_PREFIX}/include/cdr
    ${CMAKE_INSTALL_PREFIX}/include/codec
    ${CMAKE_INSTALL_PREFIX}/include/sbf
    ${CMAKE_INSTALL_PREFIX}/include/sbf/cpp
    ${CMAKE_INSTALL_PREFIX}/include/codec/fields
    ${CMAKE_INSTALL_PREFIX}/include/codec/fields
    ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/
    ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/lse
    ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/lse/packets
    ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/oslo
    ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/oslo/packets
    ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/turquoise
    ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/turquoise/packets
    ${CMAKE_BINARY_DIR}/googletest/include
    )

link_directories(
    ${CMAKE_INSTALL_PREFIX}/lib
    ${CMAKE_BINARY_DIR}/googletest/lib
    )

set (TEST_SOURCES
     "${PROJECT_SOURCE_DIR}/test/testrunner.cpp"
     "${PROJECT_SOURCE_DIR}/test/TestLseMillenniumConnector.cpp"
)

add_executable(unittest ${TEST_SOURCES})
target_link_libraries(unittest
  properties
  logger
  codec
  fields
  lsecodec
  oslocodec
  gwc
  gwcmillennium
  gtest
  gmock
  pthread
  )
add_dependencies(unittest googletest)

add_test(NAME unittest
  COMMAND unittest
)

install(TARGETS unittest
  RUNTIME DESTINATION bin)
