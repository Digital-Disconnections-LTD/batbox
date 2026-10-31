#include "glob/Ignore.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <iterator>
#include <stdexcept>

int main(){
  const std::string input(std::istreambuf_iterator<char>{std::cin},{});
  batbox::Json rows=batbox::Json::array();
  for(const auto& item:batbox::value::parse(input)){
    batbox::Json row={{"patterns",item.at("patterns")},{"path",item.at("path")}};
    try{row["result"]=batbox::glob::ignores(item.at("path").get<std::string>(),
      item.at("patterns").get<std::vector<std::string>>());}
    catch(const std::exception& error){row["error"]=error.what();}
    rows.push_back(std::move(row));
  }
  std::cout<<batbox::value::stringify(rows)<<'\n';
}
