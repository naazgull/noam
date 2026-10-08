ExternalProject_Add(tree-sitter-json
  GIT_REPOSITORY    https://github.com/tree-sitter/tree-sitter-json.git
  GIT_TAG           v0.24.8
  GIT_SHALLOW       TRUE
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DBUILD_SHARED_LIBS=ON
)
add_dependencies(tree-sitter-json
  tree-sitter-lib
)

add_library(tree-sitter-json-grammar SHARED IMPORTED)
set_target_properties(tree-sitter-json-grammar PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-json.so
)
add_dependencies(tree-sitter-json-grammar
  tree-sitter-json
)
