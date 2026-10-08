ExternalProject_Add(tree-sitter-python
  GIT_REPOSITORY    https://github.com/tree-sitter/tree-sitter-python.git
  GIT_TAG           v0.25.0
  GIT_SHALLOW       TRUE
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DBUILD_SHARED_LIBS=ON
)
add_dependencies(tree-sitter-python
  tree-sitter-lib
)

add_library(tree-sitter-python-grammar SHARED IMPORTED)
set_target_properties(tree-sitter-python-grammar PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-python.so
)
add_dependencies(tree-sitter-python-grammar
  tree-sitter-python
)
