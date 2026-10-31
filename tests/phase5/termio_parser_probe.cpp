#include "termio/parser/Parser.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <string>
using batbox::Json;
using namespace batbox::termio::parser;
int main(int argc,char** argv){
  if(argc!=2||std::string(argv[1])!="--probe")return 2;
  std::string line;
  while(std::getline(std::cin,line)){
    try{
      const auto value=batbox::value::parse(line);
      const auto kind=value.at("kind").get<std::string>();
      Json output;
      if(kind=="parser"){
        Parser parser;output=Json::array();
        for(const auto& operation:value.at("operations")){
          if(operation=="reset"){
            parser.reset();output.push_back({{"reset",true}});
          }else if(operation.is_object()){
            if(operation.contains("styleParams"))parser.style=applySGR(
              operation.at("styleParams").get<std::string>(),defaultStyle());
            if(operation.contains("inLink"))parser.inLink=operation.at("inLink").get<bool>();
            if(operation.contains("linkUrl")){
              if(operation.at("linkUrl").is_null())parser.linkUrl.reset();
              else parser.linkUrl=operation.at("linkUrl").get<std::string>();
            }
            output.push_back({{"set",true}});
          }else{
            const auto actions=parser.feed(operation.get<std::string>());
            output.push_back({{"actions",toJson(actions)},
              {"style",toJson(parser.style)},{"inLink",parser.inLink},
              {"linkUrl",parser.linkUrl?Json(*parser.linkUrl):Json()}});
          }
        }
      }else if(kind=="sgr")output=toJson(applySGR(value.at("params").get<std::string>(),defaultStyle()));
      else if(kind=="osc"){
        const auto result=parseOSC(value.at("value").get<std::string>());
        output=result?toJson(*result):Json();
      }else if(kind=="color"){
        const auto result=parseOscColor(value.at("value").get<std::string>());
        output=result?toJson(*result):Json();
      }else if(kind=="esc"){
        const auto result=parseEsc(value.at("value").get<std::string>());
        output=result?toJson(*result):Json();
      }else throw std::runtime_error("unknown kind");
      std::cout<<batbox::value::stringify(Json{{"result",output}})<<'\n';
    }catch(const std::exception& error){
      std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
    }
  }
}
