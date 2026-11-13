# CMake package configuration for noam-markdown.
#
# Installed by noam to <prefix>/<libdir>/cmake/noam-markdown/. A consumer does:
#
#   find_package(noam-markdown REQUIRED)
#   target_link_libraries(<target> PRIVATE noam::markdown)
#
# and gets the shared markdown + tree-sitter rendering library. The consumer
# must provide and link ftxui itself (the library's public headers expose
# ftxui::Elements / ftxui::Color); the tree-sitter core and the per-language
# grammar shared libraries are shipped in the same libdir and are wired in here
# as transitive link dependencies so they resolve on the consumer's runtime path.

# The libdir is two levels up from this file
# (<prefix>/<libdir>/cmake/noam-markdown). Anchoring on the libdir — rather than
# guessing the prefix depth — keeps this correct for both a plain "lib" libdir
# and a multi-arch "lib/<triplet>" one.
get_filename_component(NOAM_MARKDOWN_LIBDIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(IS_DIRECTORY "${NOAM_MARKDOWN_LIBDIR}/../include")
  set(NOAM_MARKDOWN_PREFIX "${NOAM_MARKDOWN_LIBDIR}/..")
else()
  set(NOAM_MARKDOWN_PREFIX "${NOAM_MARKDOWN_LIBDIR}/../..")
endif()

# The tree-sitter core plus every grammar noam-markdown was built with. Each is
# exposed under a noam::markdown:: prefix so it cannot collide with a target of
# the same name in the consumer's own build.
set(NOAM_MARKDOWN_TREE_SITTER_LIBS
  tree-sitter
  tree-sitter-json
  tree-sitter-cpp
  tree-sitter-lua
  tree-sitter-python
  tree-sitter-rust
  tree-sitter-diff
  tree-sitter-bash
  tree-sitter-sql
  tree-sitter-fish
  tree-sitter-prolog
  tree-sitter-markdown
  tree-sitter-markdown-inline
)
foreach(_lib IN LISTS NOAM_MARKDOWN_TREE_SITTER_LIBS)
  if(NOT TARGET "noam::markdown::${_lib}")
    add_library("noam::markdown::${_lib}" SHARED IMPORTED)
    set_target_properties("noam::markdown::${_lib}" PROPERTIES
      IMPORTED_LOCATION "${NOAM_MARKDOWN_LIBDIR}/lib${_lib}.so")
  endif()
endforeach()

if(NOT TARGET noam::markdown)
  add_library(noam::markdown SHARED IMPORTED)
  set_target_properties(noam::markdown PROPERTIES
    IMPORTED_LOCATION "${NOAM_MARKDOWN_LIBDIR}/libnoam-markdown.so"
    INTERFACE_INCLUDE_DIRECTORIES "${NOAM_MARKDOWN_PREFIX}/include")
endif()

set(_noam_markdown_transitive "")
foreach(_lib IN LISTS NOAM_MARKDOWN_TREE_SITTER_LIBS)
  list(APPEND _noam_markdown_transitive "noam::markdown::${_lib}")
endforeach()
set_target_properties(noam::markdown PROPERTIES
  INTERFACE_LINK_LIBRARIES "${_noam_markdown_transitive}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(noam-markdown
  REQUIRED_VARS NOAM_MARKDOWN_LIBDIR
  FAIL_MESSAGE
    "noam-markdown: libnoam-markdown.so not found in ${NOAM_MARKDOWN_LIBDIR}")
