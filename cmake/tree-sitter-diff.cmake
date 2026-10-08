ExternalProject_Add(tree-sitter-diff
  GIT_REPOSITORY    https://github.com/tree-sitter-grammars/tree-sitter-diff.git
  GIT_TAG           v0.2.0
  GIT_SHALLOW       TRUE
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DBUILD_SHARED_LIBS=ON
)
add_dependencies(tree-sitter-diff
  tree-sitter-lib
)

add_library(tree-sitter-diff-grammar SHARED IMPORTED)
set_target_properties(tree-sitter-diff-grammar PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-diff.so
)
add_dependencies(tree-sitter-diff-grammar
  tree-sitter-diff
)
