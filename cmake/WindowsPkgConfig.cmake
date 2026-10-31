# MSYS2 metadata can contain literal /ucrt64 paths in addition to ${prefix}.
# Native pkgconf relocates prefix variables, but needs the installation sysroot
# to translate those literal paths into the same drive-qualified directories.
if(NOT CMAKE_HOST_WIN32 OR NOT MINGW OR CMAKE_CROSSCOMPILING)
  return()
endif()
if(DEFINED ENV{PKG_CONFIG_SYSROOT_DIR} AND NOT "$ENV{PKG_CONFIG_SYSROOT_DIR}" STREQUAL "")
  return()
endif()
find_package(PkgConfig REQUIRED)
get_filename_component(batbox_pkgconfig_bin "${PKG_CONFIG_EXECUTABLE}" DIRECTORY)
get_filename_component(batbox_pkgconfig_prefix "${batbox_pkgconfig_bin}" DIRECTORY)
get_filename_component(batbox_pkgconfig_environment "${batbox_pkgconfig_prefix}" NAME)
string(TOLOWER "${batbox_pkgconfig_environment}" batbox_pkgconfig_environment)
if(batbox_pkgconfig_environment MATCHES "^(ucrt64|mingw64|mingw32|clang64|clangarm64)$"
    AND IS_DIRECTORY "${batbox_pkgconfig_prefix}/lib/pkgconfig")
  get_filename_component(batbox_msys_root "${batbox_pkgconfig_prefix}" DIRECTORY)
  set(ENV{PKG_CONFIG_SYSROOT_DIR} "${batbox_msys_root}")
endif()
