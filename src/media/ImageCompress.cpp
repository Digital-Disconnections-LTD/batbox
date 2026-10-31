#include "media/Media.hpp"
#include "media/ImagePipeline.hpp"
#include "media/Base64.hpp"
#include "formatting/Format.hpp"
#include <cmath>

namespace batbox::media {
namespace {
gadgets::files::ImageCompressed result(const std::string& bytes,const std::string& format,
  std::size_t originalSize){
  const auto normalized=format=="jpg"?"jpeg":format;
  return {indigenous::base64Encode(bytes),"image/"+normalized,originalSize};
}
}

gadgets::files::ImageCompressed compressImageBuffer(const std::string& buffer,
  std::size_t maxBytes,const std::string& originalMediaType){
  return compressWithOperations(buffer,maxBytes,originalMediaType,indigenousImageOperations());
}
gadgets::files::ImageCompressed compressWithOperations(const std::string& buffer,
  std::size_t maxBytes,const std::string& originalMediaType,const ImageOperations& operations){
  const auto slash=originalMediaType.find('/');
  const auto next=slash==std::string::npos?std::string::npos:originalMediaType.find('/',slash+1);
  const auto selected=slash==std::string::npos?"":originalMediaType.substr(slash+1,
    next==std::string::npos?std::string::npos:next-slash-1);
  const auto fallback=selected.empty()?"jpeg":selected;
  const auto normalizedFallback=fallback=="jpg"?"jpeg":fallback;
  try{
    const auto metadata=operations.inspect(buffer);
    const auto format=metadata.format.empty()?normalizedFallback:metadata.format;
    const auto originalSize=buffer.size();
    if(originalSize<=maxBytes)return result(buffer,format,originalSize);
    for(double factor:{1.0,0.75,0.5,0.25}){
      const auto width=static_cast<int>(value::round((metadata.width?metadata.width:2000)*factor));
      const auto height=static_cast<int>(value::round((metadata.height?metadata.height:2000)*factor));
      const bool palette=format=="png";
      const auto selectedFormat=format=="png"||format=="jpeg"||format=="jpg"||format=="webp"
        ?format:"";
      const auto bytes=operations.encode(buffer,width,height,selectedFormat,80,palette,0);
      if(bytes.size()<=maxBytes)return result(bytes,format,originalSize);
    }
    if(format=="png"){
      const auto bytes=operations.encode(buffer,800,800,"png",80,true,64);
      if(bytes.size()<=maxBytes)return result(bytes,"png",originalSize);
    }
    {
      const auto bytes=operations.encode(buffer,600,600,"jpeg",50,false,0);
      if(bytes.size()<=maxBytes)return result(bytes,"jpeg",originalSize);
    }
    return result(operations.encode(buffer,400,400,"jpeg",20,false,0),"jpeg",originalSize);
  }catch(const std::exception&){
    if(buffer.size()<=maxBytes)
      return result(buffer,detectImageFormatFromBuffer(buffer).substr(6),buffer.size());
    throw gadgets::files::ImageResizeError(
      "Unable to compress image ("+formatting::formatFileSize(buffer.size())+
      ") to fit within "+formatting::formatFileSize(maxBytes)+". Please use a smaller image.");
  }
}

gadgets::files::ImageCompressed compressImageBufferWithTokenLimit(
  const std::string& buffer,std::size_t maxTokens,const std::string& originalMediaType){
  const auto maxBase64Chars=static_cast<std::size_t>(std::floor(maxTokens/0.125));
  const auto maxBytes=static_cast<std::size_t>(std::floor(maxBase64Chars*0.75));
  return compressImageBuffer(buffer,maxBytes,originalMediaType);
}

std::string fallbackJpegResize(const std::string& buffer){
  return indigenous::encode(buffer,400,400,"jpeg",20);
}
} // namespace batbox::media
