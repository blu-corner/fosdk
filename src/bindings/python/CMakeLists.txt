# Include python
find_package(PythonInterp REQUIRED)
set(Python_ADDITIONAL_VERSIONS 2.7)
find_package(PythonLibs REQUIRED)
include_directories(${PYTHON_INCLUDE_PATH})

# set the flags for swig
set(CMAKE_SWIG_FLAGS "-threads")
set_source_files_properties(../fosdk.py.i PROPERTIES CPLUSPLUS ON)

# Add swig module
swig_add_module(pyFosdk python ../fosdk.py.i)
swig_link_libraries(pyFosdk gwc cdr utils logger ${PYTHON_LIBRARIES})
set_target_properties(_pyFosdk PROPERTIES OUTPUT_NAME "_Fosdk")

# Files to install with Python
set(PYTHON_INSTALL_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/run.sh
  ${CMAKE_CURRENT_SOURCE_DIR}/example.py
  ${CMAKE_CURRENT_SOURCE_DIR}/example-raw.py
  ${CMAKE_CURRENT_BINARY_DIR}/Fosdk.py)

# Declare install target for python
install(TARGETS _pyFosdk
  DESTINATION lib/python)
install(FILES ${PYTHON_INSTALL_FILES}
  DESTINATION lib/python)

