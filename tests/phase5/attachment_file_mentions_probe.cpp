#include "attachments/files/Mentions.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using namespace batbox;
int main(){std::string line;while(std::getline(std::cin,line)){
  try{
    const auto input=value::parse(line);
    const auto text=input.at("text").get<std::string>();
    if(input.at("op")=="extract"){
      std::cout<<value::stringify(attachments::files::extractAtMentionedFiles(text))<<'\n';
    }else{
      const auto parsed=attachments::files::parseAtMentionedFileLines(text);
      Json value={{"filename",parsed.filename}};
      if(parsed.lineStart)value["lineStart"]=*parsed.lineStart;
      if(parsed.lineEnd)value["lineEnd"]=*parsed.lineEnd;
      std::cout<<value::stringify(value)<<'\n';
    }
  }catch(const std::exception& error){
    std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';
  }
}}
