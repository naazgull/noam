cmake_minimum_required(VERSION 3.18)

project(noam VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(WITH_ASAN "Enable address sanitizer" OFF)
option(WITH_UBSAN "Enable undefined-behavior sanitizer" OFF)
option(WITH_EXCEPTION_PROPAGATION
  "Whether or not to catch exceptions (debugging)" OFF)
option(WITH_ALLOCATOR_DEBUGGING
  "Enable allocator debugging by keeping the list of allocated chunks (debugging)" OFF)

IF(WITH_ASAN)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
ENDIF()
IF(WITH_UBSAN)
  add_compile_options(-fsanitize=undefined)
  add_link_options(-fsanitize=undefined)
ENDIF()
if(WITH_EXCEPTION_PROPAGATION)
  add_compile_definitions(PROPAGATE_EXCEPTION)
endif()

add_compile_definitions(
  NOAM_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}"
  NOAM_VERSION="${CMAKE_PROJECT_VERSION}"
)

list(APPEND CMAKE_INSTALL_RPATH
  ${CMAKE_INSTALL_PREFIX}/bin
  ${CMAKE_INSTALL_PREFIX}/lib
  ${CMAKE_INSTALL_PREFIX}/lib64
)
list(APPEND CMAKE_BUILD_RPATH
  ${CMAKE_INSTALL_PREFIX}/bin
  ${CMAKE_INSTALL_PREFIX}/lib
  ${CMAKE_INSTALL_PREFIX}/lib64
)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
set(CMAKE_SKIP_INSTALL_RPATH OFF)
set(CMAKE_SKIP_BUILD_RPATH OFF)
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)

add_compile_options(
  -fno-omit-frame-pointer
  -Wall
  -Wextra
  -pedantic
  -Wno-interference-size
  -Wno-restrict
  -mcx16
)

string(TOLOWER "${CMAKE_BUILD_TYPE}" PROJECT_BUILD_TYPE)
if(PROJECT_BUILD_TYPE STREQUAL debug)
  add_compile_options(-O0 -ggdb3)
else()
  add_compile_options(-O3)
endif()

find_package(Threads REQUIRED)

include(ExternalProject)
include(GNUInstallDirs)
include(cmake/zapata.cmake)
include(cmake/ftxui.cmake)
include(cmake/tree-sitter.cmake)
include(cmake/tree-sitter-json.cmake)
include(cmake/tree-sitter-cpp.cmake)
include(cmake/tree-sitter-lua.cmake)
include(cmake/tree-sitter-python.cmake)
include(cmake/tree-sitter-rust.cmake)
include(cmake/tree-sitter-diff.cmake)
include(cmake/tree-sitter-markdown.cmake)

set(NOAM_TREE_SITTER_GRAMMARS
  tree-sitter-json-grammar
  tree-sitter-cpp-grammar
  tree-sitter-lua-grammar
  tree-sitter-python-grammar
  tree-sitter-rust-grammar
  tree-sitter-diff-grammar
  tree-sitter-markdown-grammar
  tree-sitter-markdown-inline-grammar
)

add_executable(noam
  src/document.cpp
  src/main.cpp
  src/text_render.cpp
  src/theme.cpp
)
add_dependencies(noam
  ftxui
)
target_include_directories(noam
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(noam
  PRIVATE
    ftxui-component
    ftxui-dom
    ftxui-screen
    Threads::Threads
    tree-sitter-lib
    ${NOAM_TREE_SITTER_GRAMMARS}
    zapata-base
    zapata-net-http
    zapata-ontology
    zapata-parser-http
    zapata-parser-json
    zapata-parser-uri
)

install(TARGETS noam
  RUNTIME
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/noam
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}
  FILES_MATCHING PATTERN "*.*"
)

# ------------------------------------------------------------------------
# TARGET: uninstall
#
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

