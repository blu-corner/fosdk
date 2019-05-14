include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_INSTALL_PREFIX}/include
    ${CMAKE_INSTALL_PREFIX}/include/sbf
    ${CMAKE_INSTALL_PREFIX}/include/logger
    ${CMAKE_INSTALL_PREFIX}/include/properties
    ${CMAKE_INSTALL_PREFIX}/include/utils
    ${CMAKE_INSTALL_PREFIX}/include/cdr
    ${CMAKE_INSTALL_PREFIX}/include/codec
    ${CMAKE_INSTALL_PREFIX}/include/codec/fields

    # need these includes for the lse-raw-send example
    ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/
    ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/lse
    ${CMAKE_INSTALL_PREFIX}/include/codec/millennium/lse/packets

    ${CMAKE_INSTALL_PREFIX}/include/codec/swx/
    ${CMAKE_INSTALL_PREFIX}/include/codec/swx/packets

    ${CMAKE_INSTALL_PREFIX}/include/codec/optiq
    ${CMAKE_INSTALL_PREFIX}/include/codec/optiq/packets
    
    ${CMAKE_BINARY_DIR}/googletest/include
    )

link_directories(
    ${CMAKE_INSTALL_PREFIX}/lib
    ${CMAKE_BINARY_DIR}/googletest/lib
    )

add_executable(gwc-lse-cdr-test "${PROJECT_SOURCE_DIR}/examples/lse-cdr-example.cpp")
target_link_libraries(gwc-lse-cdr-test gwc)

add_executable(gwc-lse-raw-test "${PROJECT_SOURCE_DIR}/examples/lse-raw-example.cpp")
target_link_libraries(gwc-lse-raw-test gwc)

add_executable(gwc-swx-cdr-test "${PROJECT_SOURCE_DIR}/examples/swx-cdr-example.cpp")
target_link_libraries(gwc-swx-cdr-test gwc)

add_executable(gwc-xetra-cdr-test "${PROJECT_SOURCE_DIR}/examples/xetra-cdr-example.cpp")
target_link_libraries(gwc-xetra-cdr-test gwc)

add_executable(gwc-optiq-cdr-test "${PROJECT_SOURCE_DIR}/examples/optiq-cdr-example.cpp")
target_link_libraries(gwc-optiq-cdr-test gwc)

add_executable(gwc-bme-cdr-test "${PROJECT_SOURCE_DIR}/examples/bme-cdr-example.cpp")
target_link_libraries(gwc-bme-cdr-test gwc)

install(TARGETS gwc-lse-cdr-test gwc-lse-raw-test gwc-swx-cdr-test
    gwc-xetra-cdr-test gwc-optiq-cdr-test gwc-bme-cdr-test RUNTIME DESTINATION bin)
