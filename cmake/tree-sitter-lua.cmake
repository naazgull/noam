ExternalProject_Add(tree-sitter-lua
  GIT_REPOSITORY    https://github.com/tree-sitter-grammars/tree-sitter-lua.git
  GIT_TAG           v0.5.0
  GIT_SHALLOW       TRUE
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DBUILD_SHARED_LIBS=ON
)
add_dependencies(tree-sitter-lua
  tree-sitter-lib
)

add_library(tree-sitter-lua-grammar SHARED IMPORTED)
set_target_properties(tree-sitter-lua-grammar PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-lua.so
)
add_dependencies(tree-sitter-lua-grammar
  tree-sitter-lua
)
