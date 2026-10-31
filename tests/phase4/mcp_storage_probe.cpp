#include "mcp/OutputStorage.hpp"
#include "mcp/Codec.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <iostream>

int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    const auto op=input.at("op").get<std::string>();
    std::optional<std::string> mime;
    if(input.contains("mime")&&input.at("mime").is_string())
      mime=input.at("mime").get<std::string>();
    batbox::Json result;
    if(op=="extension")result=batbox::mcp::extensionForMimeType(mime);
    else if(op=="binary")result=batbox::mcp::isBinaryContentType(mime.value_or(""));
    else if(op=="format")result=batbox::mcp::getFormatDescription(
      input.at("type").get<std::string>(),input.value("schema",std::string()));
    else if(op=="instructions")result=batbox::mcp::getLargeOutputInstructions(
      input.at("path").get<std::string>(),input.at("length").get<double>(),
      input.at("format").get<std::string>(),
      input.contains("maxReadLength")&&input.at("maxReadLength").is_number()
        ?std::optional<double>(input.at("maxReadLength").get<double>()):std::nullopt);
    else if(op=="saved")result=batbox::mcp::getBinaryBlobSavedMessage(
      input.at("path").get<std::string>(),mime,input.at("size").get<double>(),
      input.at("description").get<std::string>());
    else{
      batbox::mcp::StorageConfig config;
      const auto dir=input.at("dir").get<std::string>();
      config.ensureGadgetResultsDir=[dir](){std::filesystem::create_directories(dir);};
      config.getGadgetResultsDir=[dir](){return dir;};
      result=batbox::mcp::persistBinaryContent(
        batbox::mcp::decodeBase64(input.at("data").get<std::string>()),mime,
        input.at("id").get<std::string>(),config);
    }
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
