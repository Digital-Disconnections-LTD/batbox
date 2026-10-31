#include "flights/Tombstone.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using namespace batbox;
int main(){std::string line;while(std::getline(std::cin,line)){
 const auto input=value::parse(line);Json diagnostics=Json::array();
 flights::removeMessageFromFile(input.at("path"),input.at("uuid"),[&](const auto& message,const auto& level){diagnostics.push_back({message,level});});
 std::cout<<value::stringify(Json{{"diagnostics",diagnostics}})<<'\n';
}}
