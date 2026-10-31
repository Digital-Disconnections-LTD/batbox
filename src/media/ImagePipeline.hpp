#pragma once
#include "media/ImageIndigenous.hpp"
#include "media/Media.hpp"
#include <functional>

namespace batbox::media {
struct ImageOperations {
  std::function<indigenous::Metadata(const std::string&)> inspect;
  std::function<std::string(const std::string&,int,int,const std::string&,int,bool,int)> encode;
};
ImageOperations indigenousImageOperations();
gadgets::files::ImageTransform maybeResizeWithOperations(
  const std::string& buffer,std::size_t originalSize,const std::string& ext,
  const ImageOperations& operations);
gadgets::files::ImageCompressed compressWithOperations(
  const std::string& buffer,std::size_t maxBytes,const std::string& originalMediaType,
  const ImageOperations& operations);
} // namespace batbox::media
