find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPS REQUIRED IMPORTED_TARGET vips)
add_library(batbox_media
  src/media/Base64.cpp src/media/Binding.cpp src/media/ImageBasics.cpp
  src/media/ImageCompress.cpp src/media/ImageIndigenous.cpp src/media/ImagePipeline.cpp
  src/media/ImageResize.cpp src/media/PDFExtract.cpp src/media/PDFFile.cpp
  src/media/PDFRead.cpp src/media/Process.cpp)
target_link_libraries(batbox_media PUBLIC batbox_core PkgConfig::VIPS)
target_compile_options(batbox_media PRIVATE -Wall -Wextra -Wpedantic)
