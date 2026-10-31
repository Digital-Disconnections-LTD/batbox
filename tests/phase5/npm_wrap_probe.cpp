#include "npm_wrap_ansi/Wrap.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <limits>
#include <string>
int main(int argc,char** argv){
  if(argc!=2||std::string(argv[1])!="--probe")return 2;
  std::string line;
  while(std::getline(std::cin,line)){
    try{
      const auto caseValue=batbox::value::parse(line);
      const batbox::Json input=caseValue.contains("input")?caseValue.at("input"):batbox::Json(batbox::Json::value_t::discarded);
      const batbox::Json columns=caseValue.contains("columns")?caseValue.at("columns"):batbox::Json(batbox::Json::value_t::discarded);
      const auto options=caseValue.contains("options")?
        std::optional<batbox::Json>(caseValue.at("options")):std::nullopt;
      const auto output=batbox::npm_wrap_ansi::wrapAnsiJs(input,columns,options);
      std::cout<<"{\"result\":"<<batbox::value::quote(output)<<"}\n";
    }catch(const std::exception& e){
      std::cout<<batbox::Json{{"error",e.what()}}.dump()<<'\n';
    }
  }
}
