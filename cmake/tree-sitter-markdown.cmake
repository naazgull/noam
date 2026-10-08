ExternalProject_Add(tree-sitter-markdown
  GIT_REPOSITORY    https://github.com/tree-sitter-grammars/tree-sitter-markdown.git
  GIT_TAG           v0.5.3
  GIT_SHALLOW       TRUE
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DBUILD_SHARED_LIBS=ON
)
add_dependencies(tree-sitter-markdown
  tree-sitter-lib
)

add_library(tree-sitter-markdown-grammar SHARED IMPORTED)
set_target_properties(tree-sitter-markdown-grammar PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-markdown.so
)
add_dependencies(tree-sitter-markdown-grammar
  tree-sitter-markdown
)

add_library(tree-sitter-markdown-inline-grammar SHARED IMPORTED)
set_target_properties(tree-sitter-markdown-inline-grammar PROPERTIES
  IMPORTED_LOCATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libtree-sitter-markdown-inline.so
)
add_dependencies(tree-sitter-markdown-inline-grammar
  tree-sitter-markdown
)
