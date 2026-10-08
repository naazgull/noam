# FTXUI is built from source and installed into the project prefix (no system
# package required), following the same ExternalProject pattern as zapata in
# cmake/zapata.cmake. The installed shared libraries are consumed through
# IMPORTED targets; the build is ordered with add_dependencies() on the
# `ftxui` ExternalProject target.
include(GNUInstallDirs)

set(FTXUI_VERSION 7.0.3)

ExternalProject_Add(ftxui
  GIT_REPOSITORY    https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG           v${FTXUI_VERSION}
  GIT_SHALLOW       TRUE
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DBUILD_SHARED_LIBS=ON
    -DFTXUI_BUILD_DOCS=OFF
    -DFTXUI_BUILD_EXAMPLES=OFF
    -DFTXUI_BUILD_TESTS=OFF
    -DFTXUI_BUILD_TESTS_FUZZER=OFF
    -DFTXUI_ENABLE_INSTALL=ON
)

foreach(_ftxui_lib IN ITEMS screen dom component)
  add_library(ftxui-${_ftxui_lib} SHARED IMPORTED)
  set_target_properties(ftxui-${_ftxui_lib} PROPERTIES
    IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libftxui-${_ftxui_lib}.so
  )
endforeach()
