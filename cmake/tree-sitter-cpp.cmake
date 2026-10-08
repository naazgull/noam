ExternalProject_Add(tree-sitter-cpp
  GIT_REPOSITORY    https://github.com/tree-sitter/tree-sitter-cpp.git
  GIT_TAG           v0.23.4
  GIT_SHALLOW       TRUE
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DBUILD_SHARED_LIBS=ON
)
add_dependencies(tree-sitter-cpp
  tree-sitter-lib
)

add_library(tree-sitter-cpp-grammar SHARED IMPORTED)
set_target_properties(tree-sitter-cpp-grammar PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-cpp.so
)
add_dependencies(tree-sitter-cpp-grammar
  tree-sitter-cpp
)
