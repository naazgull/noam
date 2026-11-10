# m-novikov/tree-sitter-sql ships no CMakeLists.txt or Makefile (it generates
# its parser via npm), so — unlike the other grammars — this does not reuse the
# project's own CMake. Its committed src/parser.c (C) and src/scanner.cc (C++)
# are compiled straight into the shared library; the C++ scanner is what makes
# the standard CMAKE_ARGS build impossible here. The compile steps run through
# a generated shell script rather than an inline `bash -c "a; b; c"`, because
# CMake would otherwise split the semicolon-separated commands into separate
# arguments and execute only the first. Pinned to a commit (not a branch/tag)
# so an upstream force-push cannot break the build the way a tracked branch
# can, and because a commit SHA cannot be shallow-cloned on GitHub.
set(TS_SQL_SOURCE_DIR
  ${CMAKE_BINARY_DIR}/tree-sitter-sql-prefix/src/tree-sitter-sql)
set(TS_SQL_BUILD_DIR
  ${CMAKE_BINARY_DIR}/tree-sitter-sql-prefix/src/tree-sitter-sql-build)
set(TS_SQL_BUILD_SCRIPT
  ${CMAKE_BINARY_DIR}/ts-sql-build.sh)
if(PROJECT_BUILD_TYPE STREQUAL debug)
  set(TS_SQL_OPT -O0 -g)
else()
  set(TS_SQL_OPT -O2)
endif()
file(WRITE ${TS_SQL_BUILD_SCRIPT}
  "#!/bin/sh
  set -e
  mkdir -p ${TS_SQL_BUILD_DIR}
  ${CMAKE_C_COMPILER} -c ${TS_SQL_OPT} -fPIC -I${TS_SQL_SOURCE_DIR}/src ${TS_SQL_SOURCE_DIR}/src/parser.c -o ${TS_SQL_BUILD_DIR}/parser.o
  ${CMAKE_CXX_COMPILER} -c ${TS_SQL_OPT} -fPIC -I${TS_SQL_SOURCE_DIR}/src ${TS_SQL_SOURCE_DIR}/src/scanner.cc -o ${TS_SQL_BUILD_DIR}/scanner.o
  ${CMAKE_CXX_COMPILER} -shared ${TS_SQL_BUILD_DIR}/parser.o ${TS_SQL_BUILD_DIR}/scanner.o -o ${TS_SQL_BUILD_DIR}/libtree-sitter-sql.so
")

ExternalProject_Add(tree-sitter-sql
  GIT_REPOSITORY    https://github.com/m-novikov/tree-sitter-sql.git
  GIT_TAG           587f30d184b058450be2a2330878210c5f33b3f9
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     bash ${TS_SQL_BUILD_SCRIPT}
  INSTALL_COMMAND   ${CMAKE_COMMAND} -E copy
    ${TS_SQL_BUILD_DIR}/libtree-sitter-sql.so
    ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-sql.so
  TEST_COMMAND      ""
)
add_dependencies(tree-sitter-sql
  tree-sitter-lib
)

add_library(tree-sitter-sql-grammar SHARED IMPORTED)
set_target_properties(tree-sitter-sql-grammar PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-sql.so
)
add_dependencies(tree-sitter-sql-grammar
  tree-sitter-sql
)
