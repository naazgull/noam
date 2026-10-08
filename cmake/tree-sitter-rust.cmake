ExternalProject_Add(tree-sitter-rust
  GIT_REPOSITORY    https://github.com/tree-sitter/tree-sitter-rust.git
  GIT_TAG           v0.24.2
  GIT_SHALLOW       TRUE
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DBUILD_SHARED_LIBS=ON
)
add_dependencies(tree-sitter-rust
  tree-sitter-lib
)

add_library(tree-sitter-rust-grammar SHARED IMPORTED)
set_target_properties(tree-sitter-rust-grammar PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-rust.so
)
add_dependencies(tree-sitter-rust-grammar
  tree-sitter-rust
)
