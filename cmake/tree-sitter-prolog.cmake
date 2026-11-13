# Rukiza/tree-sitter-prolog ships no CMakeLists.txt or Makefile, so, as with
# the SQL and fish grammars, this does not reuse the project's own CMake. Its
# committed src/parser.c is plain C and there is no external scanner, so it
# compiles straight into the shared library. The compile steps run through a
# generated shell script rather than an inline `bash -c "a; b; c"`, because
# CMake would otherwise split the semicolon-separated commands into separate
# arguments and execute only the first. Pinned to a commit (not a branch/tag)
# so an upstream force-push cannot break the build the way a tracked branch
# can, and because a commit SHA cannot be shallow-cloned on GitHub.
set(TS_PROLOG_SOURCE_DIR
  ${CMAKE_BINARY_DIR}/tree-sitter-prolog-prefix/src/tree-sitter-prolog)
set(TS_PROLOG_BUILD_DIR
  ${CMAKE_BINARY_DIR}/tree-sitter-prolog-prefix/src/tree-sitter-prolog-build)
set(TS_PROLOG_BUILD_SCRIPT
  ${CMAKE_BINARY_DIR}/ts-prolog-build.sh)
if(PROJECT_BUILD_TYPE STREQUAL debug)
  set(TS_PROLOG_OPT -O0 -g)
else()
  set(TS_PROLOG_OPT -O2)
endif()
file(WRITE ${TS_PROLOG_BUILD_SCRIPT}
  "#!/bin/sh
  set -e
  mkdir -p ${TS_PROLOG_BUILD_DIR}
  ${CMAKE_C_COMPILER} -c ${TS_PROLOG_OPT} -fPIC -I${TS_PROLOG_SOURCE_DIR}/src ${TS_PROLOG_SOURCE_DIR}/src/parser.c -o ${TS_PROLOG_BUILD_DIR}/parser.o
  ${CMAKE_C_COMPILER} -shared ${TS_PROLOG_BUILD_DIR}/parser.o -o ${TS_PROLOG_BUILD_DIR}/libtree-sitter-prolog.so
")

ExternalProject_Add(tree-sitter-prolog
  GIT_REPOSITORY    https://github.com/Rukiza/tree-sitter-prolog.git
  GIT_TAG           c246cf2bf36590a3cb4de380205376d3c46208e8
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     bash ${TS_PROLOG_BUILD_SCRIPT}
  INSTALL_COMMAND   ${CMAKE_COMMAND} -E copy
    ${TS_PROLOG_BUILD_DIR}/libtree-sitter-prolog.so
    ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-prolog.so
  TEST_COMMAND      ""
)
add_dependencies(tree-sitter-prolog
  tree-sitter-lib
)

add_library(tree-sitter-prolog-grammar SHARED IMPORTED)
set_target_properties(tree-sitter-prolog-grammar PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-prolog.so
)
add_dependencies(tree-sitter-prolog-grammar
  tree-sitter-prolog
)
