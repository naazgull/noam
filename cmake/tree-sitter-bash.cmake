ExternalProject_Add(tree-sitter-bash
  GIT_REPOSITORY    https://github.com/tree-sitter/tree-sitter-bash.git
  GIT_TAG           v0.25.1
  GIT_SHALLOW       TRUE
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DBUILD_SHARED_LIBS=ON
)
add_dependencies(tree-sitter-bash
  tree-sitter-lib
)

add_library(tree-sitter-bash-grammar SHARED IMPORTED)
set_target_properties(tree-sitter-bash-grammar PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-bash.so
)
add_dependencies(tree-sitter-bash-grammar
  tree-sitter-bash
)
