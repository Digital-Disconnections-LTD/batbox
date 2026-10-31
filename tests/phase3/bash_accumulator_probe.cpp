#include "gadgets/bash/Accumulator.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);const std::string input(std::istreambuf_iterator<char>{file},{});
  const auto cases=batbox::value::parse(input);batbox::Json output=batbox::Json::array();
  for(const auto& item:cases){
    const auto& maximum=item.at("maxSize");
    const double maxSize=maximum.is_string()?maximum.get<std::string>()=="Infinity"?
      std::numeric_limits<double>::infinity():maximum.get<std::string>()=="-Infinity"?
      -std::numeric_limits<double>::infinity():std::numeric_limits<double>::quiet_NaN():
      maximum.get<double>();
    batbox::bash_gadget::EndTruncatingAccumulator accumulator(maxSize);
    batbox::Json states=batbox::Json::array();
    for(const auto& action:item.at("actions")){
      if(action.contains("append"))accumulator.append(action.at("append").get<std::string>());
      else if(action.contains("appendBuffer")){
        std::string bytes;for(const auto& byte:action.at("appendBuffer"))bytes.push_back(static_cast<char>(byte.get<int>()));
        accumulator.appendBuffer(bytes);
      }else if(action.value("clear",false))accumulator.clear();
      states.push_back({{"text",accumulator.toString()},{"length",accumulator.length()},
        {"truncated",accumulator.truncated()},{"totalBytes",accumulator.totalBytes()}});
    }
    output.push_back(std::move(states));
  }
  std::cout<<batbox::value::stringify(output)<<'\n';
}
