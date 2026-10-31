include_guard(GLOBAL)
find_package(PkgConfig REQUIRED)
option(BATBOX_FETCH_CMARK_GFM "Use the pinned cmark-gfm build if a system package is missing" ON)
if(NOT BATBOX_FORCE_BUNDLED_CMARK_GFM)
  pkg_check_modules(CMARK_GFM QUIET IMPORTED_TARGET libcmark-gfm)
endif()
if(NOT TARGET PkgConfig::CMARK_GFM)
  if(NOT BATBOX_FETCH_CMARK_GFM)
    message(FATAL_ERROR "libcmark-gfm is required; install it or enable BATBOX_FETCH_CMARK_GFM")
  endif()
  include(FetchContent)
  set(CMARK_TESTS OFF CACHE BOOL "" FORCE)
  set(CMARK_SHARED OFF CACHE BOOL "" FORCE)
  set(CMARK_STATIC ON CACHE BOOL "" FORCE)
  # This pinned release predates CMake 4's policy compatibility floor.
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
  FetchContent_Declare(batbox_cmark_gfm
    URL https://codeload.github.com/github/cmark-gfm/tar.gz/refs/tags/0.29.0.gfm.13
    URL_HASH SHA256=5abc61798ebd9de5660bc076443c07abad2b8d15dbc11094a3a79644b8ad243a
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(batbox_cmark_gfm)
  add_library(batbox_cmark_gfm_sdk INTERFACE)
  target_include_directories(batbox_cmark_gfm_sdk INTERFACE
    "${batbox_cmark_gfm_SOURCE_DIR}/src" "${batbox_cmark_gfm_BINARY_DIR}/src"
    "${batbox_cmark_gfm_SOURCE_DIR}/extensions" "${batbox_cmark_gfm_BINARY_DIR}/extensions")
  target_compile_definitions(batbox_cmark_gfm_sdk INTERFACE
    CMARK_GFM_STATIC_DEFINE CMARK_GFM_EXTENSIONS_STATIC_DEFINE)
  target_link_libraries(batbox_cmark_gfm_sdk INTERFACE
    libcmark-gfm-extensions_static libcmark-gfm_static)
  add_library(PkgConfig::CMARK_GFM ALIAS batbox_cmark_gfm_sdk)
endif()
