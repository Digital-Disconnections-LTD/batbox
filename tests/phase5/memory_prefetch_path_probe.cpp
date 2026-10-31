#include "memory_prefetch/BatMemoryPath.hpp"
#include <iostream>

using namespace batbox;
int main(){std::string line;while(std::getline(std::cin,line)){
  try{
    const Json input=value::parse(line);
    instructions::MemoryPathServices paths;
    paths.configHomeDir=input.at("base").get<std::string>();
    std::cout<<value::stringify(memory_prefetch::getBatMemoryDir(
      input.at("type").get<std::string>(),
      input.at("scope").get<std::string>(),
      input.at("cwd").get<std::string>(),paths))<<'\n';
  }catch(const std::exception& error)
    {std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}
}}
