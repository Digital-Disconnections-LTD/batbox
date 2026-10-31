#pragma once
#include <cstddef>
#include <string>

namespace batbox::media::indigenous {
struct Metadata {int width=0,height=0;std::string format;};
Metadata inspect(const std::string& buffer);
std::string encode(const std::string& buffer,int maxWidth,int maxHeight,
  const std::string& outputFormat,int quality=80,bool palette=false,int colors=0);
} // namespace batbox::media::indigenous
