cmake_minimum_required(VERSION 3.10)

if(WIN32)
  execute_process(COMMAND ${CMAKE_CURRENT_LIST_DIR}/vcpkg_install.bat)
else()
  execute_process(COMMAND ${CMAKE_CURRENT_LIST_DIR}/vcpkg_install.sh ${VCPKG_PACKAGES})
endif()
