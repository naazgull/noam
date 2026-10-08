include(GNUInstallDirs)

set(TREE_SITTER_VERSION 0.26.13)

ExternalProject_Add(tree-sitter
  GIT_REPOSITORY    https://github.com/tree-sitter/tree-sitter.git
  GIT_TAG           v${TREE_SITTER_VERSION}
  GIT_SHALLOW       TRUE
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DBUILD_SHARED_LIBS=ON
    -DTREE_SITTER_FEATURE_WASM=OFF
)

add_library(tree-sitter-lib SHARED IMPORTED)
add_dependencies(tree-sitter-lib
  tree-sitter
)
set_target_properties(tree-sitter-lib PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter.so
)
