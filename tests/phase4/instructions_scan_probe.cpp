#include "instructions/MemoryScan.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    const auto op=input.at("op").get<std::string>();
    batbox::Json result;
    if(op=="scan"){
      result=batbox::Json::array();
      for(const auto& item:batbox::instructions::scanMemoryFiles(input.at("directory"))){
        batbox::Json row={{"filename",item.filename},{"filePath",item.filePath},
          {"mtimeMs",item.mtimeMs},{"description",item.description}};
        if(item.type)row["type"]=*item.type;
        result.push_back(std::move(row));
      }
    }else if(op=="manifest"){
      std::vector<batbox::instructions::MemoryHeader> headers;
      for(const auto& item:input.at("memories")){
        batbox::instructions::MemoryHeader header;
        header.filename=item.at("filename").get<std::string>();
        header.filePath=item.value("filePath",std::string());
        header.mtimeMs=item.at("mtimeMs").get<double>();
        header.description=item.value("description",batbox::Json(nullptr));
        if(item.contains("type")&&item.at("type").is_string())
          header.type=item.at("type").get<std::string>();
        headers.push_back(std::move(header));
      }
      result=batbox::instructions::formatMemoryManifest(headers);
    }else{
      const auto mtime=input.at("mtime").get<double>(),now=input.at("now").get<double>();
      if(op=="memoryAgeDays")result=batbox::instructions::memoryAgeDays(mtime,now);
      else if(op=="memoryAge")result=batbox::instructions::memoryAge(mtime,now);
      else if(op=="memoryFreshnessText")result=batbox::instructions::memoryFreshnessText(mtime,now);
      else result=batbox::instructions::memoryFreshnessNote(mtime,now);
    }
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
