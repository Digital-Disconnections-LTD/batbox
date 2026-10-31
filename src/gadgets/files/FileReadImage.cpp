#include "gadgets/files/FileRead.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::gadgets::files {
namespace {
std::string base64(const std::string& bytes) {
  static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((bytes.size()+2)/3)*4);
  for(std::size_t i=0;i<bytes.size();i+=3){
    const unsigned a=static_cast<unsigned char>(bytes[i]);
    const unsigned b=i+1<bytes.size()?static_cast<unsigned char>(bytes[i+1]):0;
    const unsigned c=i+2<bytes.size()?static_cast<unsigned char>(bytes[i+2]):0;
    out+=alphabet[a>>2];out+=alphabet[((a&3)<<4)|(b>>4)];
    out+=i+1<bytes.size()?alphabet[((b&15)<<2)|(c>>6)]:'=';
    out+=i+2<bytes.size()?alphabet[c&63]:'=';
  }
  return out;
}
Json response(const std::string& bytes, const std::string& mediaType,
              std::size_t originalSize, const Json& dimensions=Json()) {
  Json file={{"base64",base64(bytes)},{"type","image/"+mediaType},{"originalSize",originalSize}};
  if(!dimensions.is_null())file["dimensions"]=dimensions;
  return Json{{"type","image"},{"file",file}};
}
void require(bool bound,const char* name){
  if(!bound)throw std::invalid_argument(std::string("Missing FileRead image binding: ")+name);
}
} // namespace

Json readImageWithTokenBudget(const std::string& path,std::size_t maxTokens,
                             std::optional<std::size_t> maxBytes,const ImageServices& services){
  const std::string imageBuffer=filesystem::readFileBytes(path,maxBytes);
  const auto originalSize=imageBuffer.size();
  if(originalSize==0)throw std::runtime_error("Image file is empty: "+path);
  require(static_cast<bool>(services.detectImageFormatFromBuffer),"detectImageFormatFromBuffer");
  const std::string detectedMediaType=services.detectImageFormatFromBuffer(imageBuffer);
  const auto slash=detectedMediaType.find('/');
  const std::string detectedFormat=slash!=std::string::npos && slash+1<detectedMediaType.size()
    ? detectedMediaType.substr(slash+1) : "png";
  Json result;
  require(static_cast<bool>(services.maybeResizeAndDownsampleImageBuffer),"maybeResizeAndDownsampleImageBuffer");
  try{
    const auto resized=services.maybeResizeAndDownsampleImageBuffer(imageBuffer,originalSize,detectedFormat);
    result=response(resized.buffer,resized.mediaType,originalSize,resized.dimensions);
  }catch(const ImageResizeError&){throw;}
  catch(const std::exception&){result=response(imageBuffer,detectedFormat,originalSize);}
  const auto estimatedTokens=std::ceil(result.at("file").at("base64").get<std::string>().size()*0.125);
  if(estimatedTokens>maxTokens){
    require(static_cast<bool>(services.compressImageBufferWithTokenLimit),"compressImageBufferWithTokenLimit");
    try{
      const auto compressed=services.compressImageBufferWithTokenLimit(imageBuffer,maxTokens,detectedMediaType);
      return Json{{"type","image"},{"file",{{"base64",compressed.base64},
        {"type",compressed.mediaType},{"originalSize",originalSize}}}};
    }catch(const std::exception&){
      require(static_cast<bool>(services.fallbackJpegResize),"fallbackJpegResize");
      try{
        return response(services.fallbackJpegResize(imageBuffer),"jpeg",originalSize);
      }catch(const std::exception&){return response(imageBuffer,detectedFormat,originalSize);}
    }
  }
  return result;
}
} // namespace batbox::gadgets::files
