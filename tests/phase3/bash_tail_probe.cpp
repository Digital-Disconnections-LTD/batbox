#include "shell/TailFile.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    const auto result=batbox::shell::tailFile(input.at("path").get<std::string>(),
      input.at("maxBytes").get<std::size_t>());
    std::cout<<batbox::value::stringify(batbox::Json{{"content",result.content},
      {"bytesRead",result.bytesRead},{"bytesTotal",result.bytesTotal}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify(batbox::Json{{"error",error.what()}})<<'\n';
  }
}
