#include "media/ImagePipeline.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

using batbox::Json;
namespace media=batbox::media;
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    std::string bytes;
    for(const auto& value:input.at("bytes"))bytes.push_back(static_cast<char>(value.get<int>()));
    const auto metadata=input.value("metadata",Json::object());
    Json log=Json::array();std::size_t index=0;
    media::ImageOperations operations;
    operations.inspect=[&](const std::string&){
      if(input.value("metadataError",false))throw std::runtime_error("metadata failed");
      return media::indigenous::Metadata{metadata.value("width",0),metadata.value("height",0),
        metadata.value("format",std::string())};};
    operations.encode=[&](const std::string&,int width,int height,const std::string& format,
        int quality,bool palette,int colors){
      log.push_back({{"width",width},{"height",height},{"format",format},
        {"quality",quality},{"palette",palette},{"colors",colors}});
      if(input.value("encodeError",false))throw std::runtime_error("encode failed");
      auto sizes=input.at("outputSizes");
      const auto size=index<sizes.size()?sizes.at(index++).get<std::size_t>():0;
      return std::string(size,'x');};
    Json result;
    if(input.at("op")=="resize"){
      const auto output=media::maybeResizeWithOperations(bytes,input.at("originalSize").get<std::size_t>(),
        input.value("ext",std::string("png")),operations);
      result={{"bufferSize",output.buffer.size()},{"mediaType",output.mediaType},
        {"dimensions",output.dimensions.is_null()?Json():output.dimensions}};
    }else if(input.at("op")=="compress"){
      const auto output=media::compressWithOperations(bytes,input.at("maxBytes").get<std::size_t>(),
        input.value("originalMediaType",std::string()),operations);
      result={{"bufferSize",output.base64.size()/4*3-
         (output.base64.ends_with("==")?2:output.base64.ends_with("=")?1:0)},
         {"mediaType",output.mediaType},{"originalSize",output.originalSize}};
    }else if(input.at("op")=="detect"){
      result=media::detectImageFormatFromBuffer(bytes);
    }else if(input.at("op")=="metadataText"){
      const auto note=media::createImageMetadataText(input.value("dimensions",Json::object()),
        input.contains("sourcePath")?std::optional<std::string>(input.at("sourcePath").get<std::string>()):std::nullopt);
      result=note?Json(*note):Json();
    }else throw std::runtime_error("unknown operation");
    std::cout<<batbox::value::stringify(Json{{"result",result},{"log",log}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
  }
}
