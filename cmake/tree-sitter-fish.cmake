# ram02z/tree-sitter-fish ships no CMakeLists.txt or Makefile (it generates
# its parser via the tree-sitter CLI), so, as with the SQL grammar, this does
# not reuse the project's own CMake. Its committed src/parser.c and
# src/scanner.c are both plain C, so both compile with the C compiler and
# link straight into the shared library. The compile steps run through a
# generated shell script rather than an inline `bash -c "a; b; c"`, because
# CMake would otherwise split the semicolon-separated commands into separate
# arguments and execute only the first. Pinned to a commit (not a branch/tag)
# so an upstream force-push cannot break the build the way a tracked branch
# can, and because a commit SHA cannot be shallow-cloned on GitHub.
set(TS_FISH_SOURCE_DIR
  ${CMAKE_BINARY_DIR}/tree-sitter-fish-prefix/src/tree-sitter-fish)
set(TS_FISH_BUILD_DIR
  ${CMAKE_BINARY_DIR}/tree-sitter-fish-prefix/src/tree-sitter-fish-build)
set(TS_FISH_BUILD_SCRIPT
  ${CMAKE_BINARY_DIR}/ts-fish-build.sh)
if(PROJECT_BUILD_TYPE STREQUAL debug)
  set(TS_FISH_OPT -O0 -g)
else()
  set(TS_FISH_OPT -O2)
endif()
file(WRITE ${TS_FISH_BUILD_SCRIPT}
  "#!/bin/sh
  set -e
  mkdir -p ${TS_FISH_BUILD_DIR}
  ${CMAKE_C_COMPILER} -c ${TS_FISH_OPT} -fPIC -I${TS_FISH_SOURCE_DIR}/src ${TS_FISH_SOURCE_DIR}/src/parser.c -o ${TS_FISH_BUILD_DIR}/parser.o
  ${CMAKE_C_COMPILER} -c ${TS_FISH_OPT} -fPIC -I${TS_FISH_SOURCE_DIR}/src ${TS_FISH_SOURCE_DIR}/src/scanner.c -o ${TS_FISH_BUILD_DIR}/scanner.o
  ${CMAKE_C_COMPILER} -shared ${TS_FISH_BUILD_DIR}/parser.o ${TS_FISH_BUILD_DIR}/scanner.o -o ${TS_FISH_BUILD_DIR}/libtree-sitter-fish.so
")

ExternalProject_Add(tree-sitter-fish
  GIT_REPOSITORY    https://github.com/ram02z/tree-sitter-fish.git
  GIT_TAG           b7f1d682941e0c62dfcd6bf9ef481638351bab16
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     bash ${TS_FISH_BUILD_SCRIPT}
  INSTALL_COMMAND   ${CMAKE_COMMAND} -E copy
    ${TS_FISH_BUILD_DIR}/libtree-sitter-fish.so
    ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-fish.so
  TEST_COMMAND      ""
)
add_dependencies(tree-sitter-fish
  tree-sitter-lib
)

add_library(tree-sitter-fish-grammar SHARED IMPORTED)
set_target_properties(tree-sitter-fish-grammar PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-fish.so
)
add_dependencies(tree-sitter-fish-grammar
  tree-sitter-fish
)
