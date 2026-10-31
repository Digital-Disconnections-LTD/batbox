#include "media/ImageIndigenous.hpp"
#include "media/Media.hpp"
#include <vips/vips.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace batbox::media::indigenous {
namespace {
void initialize(){
  static std::once_flag once;
  std::call_once(once,[]{if(VIPS_INIT("batbox")!=0)
    throw std::runtime_error("libvips initialization failed");});
}
[[noreturn]] void fail(){
  std::string error=vips_error_buffer();vips_error_clear();
  throw std::runtime_error(error.empty()?"libvips processing failed":error);
}
using Image=std::unique_ptr<VipsImage,decltype(&g_object_unref)>;
Image load(const std::string& buffer){
  initialize();
  VipsImage* raw=vips_image_new_from_buffer(buffer.data(),buffer.size(),"",nullptr);
  if(!raw)fail();
  return Image(raw,g_object_unref);
}
}
Metadata inspect(const std::string& buffer){
  auto image=load(buffer);
  const auto mime=detectImageFormatFromBuffer(buffer);
  return {vips_image_get_width(image.get()),vips_image_get_height(image.get()),mime.substr(6)};
}
std::string encode(const std::string& buffer,int maxWidth,int maxHeight,
  const std::string& outputFormat,int quality,bool palette,int colors){
  auto source=load(buffer);
  const std::string format=outputFormat.empty()?detectImageFormatFromBuffer(buffer).substr(6):outputFormat;
  VipsImage* resizedRaw=nullptr;
  VipsImage* current=source.get();
  if(maxWidth>0&&maxHeight>0){
    const int width=vips_image_get_width(source.get()),height=vips_image_get_height(source.get());
    if(width<=0||height<=0)throw std::runtime_error("Invalid image dimensions");
    const double factor=std::min({1.0,static_cast<double>(maxWidth)/width,
      static_cast<double>(maxHeight)/height});
    if(factor<1.0){
      if(vips_resize(source.get(),&resizedRaw,factor,nullptr)!=0)fail();
      current=resizedRaw;
    }
  }
  Image resized(resizedRaw,g_object_unref);
  void* output=nullptr;std::size_t length=0;int status=-1;
  if(format=="jpeg"||format=="jpg")
    status=vips_jpegsave_buffer(current,&output,&length,"Q",quality,nullptr);
  else if(format=="png"){
    if(palette){
      if(colors>0)status=vips_pngsave_buffer(current,&output,&length,"compression",9,
        "palette",TRUE,"colours",colors,nullptr);
      else status=vips_pngsave_buffer(current,&output,&length,"compression",9,"palette",TRUE,nullptr);
    }else status=vips_pngsave_buffer(current,&output,&length,nullptr);
  }else if(format=="webp")
    status=vips_webpsave_buffer(current,&output,&length,"Q",quality,nullptr);
  else if(format=="gif")
    status=vips_gifsave_buffer(current,&output,&length,nullptr);
  else throw std::runtime_error("Unsupported image output format: "+format);
  if(status!=0)fail();
  std::string bytes(static_cast<const char*>(output),length);
  g_free(output);
  return bytes;
}
} // namespace batbox::media::indigenous
