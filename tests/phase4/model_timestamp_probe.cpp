#include "model_stash/Date.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <iostream>
#include <iterator>
int main(){
  const std::string input(std::istreambuf_iterator<char>{std::cin},{});
  auto out=batbox::Json::array();
  for(const auto& value:batbox::value::parse(input)){
    const auto n=batbox::model_stash::indigenousMessageTimestampMs(value.get<std::string>());
    out.push_back(std::isnan(n)?batbox::Json():batbox::Json(n));
  }
  std::cout<<batbox::value::stringify(out)<<'\n';
}
