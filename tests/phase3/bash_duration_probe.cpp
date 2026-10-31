#include "shell/FormatDuration.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);const std::string data(std::istreambuf_iterator<char>{file},{});
  const auto cases=batbox::value::parse(data);batbox::Json result=batbox::Json::array();
  for(const auto& item:cases){const auto options=item.value("options",batbox::Json::object());
    result.push_back(batbox::shell::formatDuration(item.at("ms").get<double>(),
      options.value("hideTrailingZeros",false),options.value("mostSignificantOnly",false)));}
  std::cout<<batbox::value::stringify(result)<<'\n';
}
