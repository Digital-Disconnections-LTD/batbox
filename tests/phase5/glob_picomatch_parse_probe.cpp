#include "glob/PicomatchParser.hpp"
#include "glob/Regex16.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <iterator>

int main(){
  const std::string input(std::istreambuf_iterator<char>{std::cin},{});
  batbox::Json rows=batbox::Json::array();
  for(const auto& pattern:batbox::value::parse(input)){
    batbox::Json row={{"pattern",pattern}};
    try{const auto state=batbox::glob::detail::PicoParser(
      batbox::glob::units(pattern.get<std::string>()),true).parse();
      row["output"]=batbox::glob::text(state.output);
      row["negated"]=state.negated;
    }catch(const std::exception& error){row["error"]=error.what();}
    rows.push_back(std::move(row));
  }
  std::cout<<batbox::value::stringify(rows)<<'\n';
}
