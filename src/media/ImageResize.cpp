#include "media/Media.hpp"
#include "media/ImagePipeline.hpp"
#include "formatting/Format.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace batbox::media {
namespace {
constexpr std::size_t apiImageMaxBase64Size=5*1024*1024;
constexpr std::size_t imageTargetRawSize=apiImageMaxBase64Size*3/4;
constexpr int imageMaxWidth=2000,imageMaxHeight=2000;
Json dimensions(int originalWidth,int originalHeight,int width,int height){
  return {{"originalWidth",originalWidth},{"originalHeight",originalHeight},
    {"displayWidth",width},{"displayHeight",height}};
}
bool overPngDimensions(const std::string& buffer){
  if(buffer.size()<24||detectImageFormatFromBuffer(buffer)!="image/png")return false;
  const auto* bytes=reinterpret_cast<const unsigned char*>(buffer.data());
  const auto big=[&](int offset){return (static_cast<std::uint32_t>(bytes[offset])<<24)|
    (static_cast<std::uint32_t>(bytes[offset+1])<<16)|
    (static_cast<std::uint32_t>(bytes[offset+2])<<8)|bytes[offset+3];};
  return big(16)>imageMaxWidth||big(20)>imageMaxHeight;
}
gadgets::files::ImageTransform result(std::string bytes,std::string format,const Json& dims=Json()){
  return {std::move(bytes),std::move(format),dims};
}
}

gadgets::files::ImageTransform maybeResizeAndDownsampleImageBuffer(
  const std::string& buffer,std::size_t originalSize,const std::string& ext){
  return maybeResizeWithOperations(buffer,originalSize,ext,indigenousImageOperations());
}
gadgets::files::ImageTransform maybeResizeWithOperations(
  const std::string& buffer,std::size_t originalSize,const std::string& ext,
  const ImageOperations& operations){
  if(buffer.empty())throw gadgets::files::ImageResizeError("Image file is empty (0 bytes)");
  try{
    const auto metadata=operations.inspect(buffer);
    const std::string mediaType=metadata.format.empty()?ext:metadata.format;
    const std::string normalized=mediaType=="jpg"?"jpeg":mediaType;
    if(metadata.width<=0||metadata.height<=0){
      if(originalSize>imageTargetRawSize)
        return result(operations.encode(buffer,0,0,"jpeg",80,false,0),"jpeg");
      return result(buffer,normalized);
    }
    const int originalWidth=metadata.width,originalHeight=metadata.height;
    int width=originalWidth,height=originalHeight;
    if(originalSize<=imageTargetRawSize&&width<=imageMaxWidth&&height<=imageMaxHeight)
      return result(buffer,normalized,dimensions(originalWidth,originalHeight,width,height));
    const bool needsResize=width>imageMaxWidth||height>imageMaxHeight;
    const bool isPng=normalized=="png";
    if(!needsResize&&originalSize>imageTargetRawSize){
      if(isPng){
        auto png=operations.encode(buffer,0,0,"png",80,true,0);
        if(png.size()<=imageTargetRawSize)
          return result(std::move(png),"png",dimensions(originalWidth,originalHeight,width,height));
      }
      for(int quality:{80,60,40,20}){
        auto jpeg=operations.encode(buffer,0,0,"jpeg",quality,false,0);
        if(jpeg.size()<=imageTargetRawSize)
          return result(std::move(jpeg),"jpeg",dimensions(originalWidth,originalHeight,width,height));
      }
    }
    if(width>imageMaxWidth){
      height=static_cast<int>(value::round(static_cast<double>(height)*imageMaxWidth/width));
      width=imageMaxWidth;
    }
    if(height>imageMaxHeight){
      width=static_cast<int>(value::round(static_cast<double>(width)*imageMaxHeight/height));
      height=imageMaxHeight;
    }
    auto resized=operations.encode(buffer,width,height,"",80,false,0);
    if(resized.size()>imageTargetRawSize){
      if(isPng){
        auto png=operations.encode(buffer,width,height,"png",80,true,0);
        if(png.size()<=imageTargetRawSize)
          return result(std::move(png),"png",dimensions(originalWidth,originalHeight,width,height));
      }
      for(int quality:{80,60,40,20}){
        auto jpeg=operations.encode(buffer,width,height,"jpeg",quality,false,0);
        if(jpeg.size()<=imageTargetRawSize)
          return result(std::move(jpeg),"jpeg",dimensions(originalWidth,originalHeight,width,height));
      }
      const int smallerWidth=std::min(width,1000);
      const int smallerHeight=static_cast<int>(value::round(
        static_cast<double>(height)*smallerWidth/std::max(width,1)));
      auto jpeg=operations.encode(buffer,smallerWidth,smallerHeight,"jpeg",20,false,0);
      return result(std::move(jpeg),"jpeg",
        dimensions(originalWidth,originalHeight,smallerWidth,smallerHeight));
    }
    return result(std::move(resized),normalized,dimensions(originalWidth,originalHeight,width,height));
  }catch(const std::exception&){
    const std::string detected=detectImageFormatFromBuffer(buffer);
    const std::string normalized=detected.substr(6);
    const auto base64Size=static_cast<std::size_t>(std::ceil(originalSize*4.0/3.0));
    const bool overDim=overPngDimensions(buffer);
    if(base64Size<=apiImageMaxBase64Size&&!overDim)return result(buffer,normalized);
    if(overDim)throw gadgets::files::ImageResizeError(
      "Unable to resize image — dimensions exceed the 2000x2000px limit and image processing failed. "
      "Please resize the image to reduce its pixel dimensions.");
    throw gadgets::files::ImageResizeError(
      "Unable to resize image ("+formatting::formatFileSize(originalSize)+" raw, "+
      formatting::formatFileSize(base64Size)+" base64). The image exceeds the 5MB API limit and compression failed. "
      "Please resize the image manually or use a smaller image.");
  }
}
} // namespace batbox::media
