#include "memory_prefetch/Prefetch.hpp"
#include <iostream>

using namespace batbox;
int main(){std::string line;while(std::getline(std::cin,line)){
  try{const Json input=value::parse(line);
    std::cout<<value::stringify(memory_prefetch::memoryHeader(
      input.at("path").get<std::string>(),input.at("mtime").get<double>(),
      input.at("now").get<double>()))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}
}}
