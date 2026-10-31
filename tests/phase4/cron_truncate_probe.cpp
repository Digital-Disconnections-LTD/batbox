#include "format/Truncate.hpp"
#include "intl/IndigenousInternal.hpp"
#include "runtime/JsonValue.hpp"
#include "text_width/CodePoints.hpp"
#include <iostream>
#include <string>

int main(){
  const auto segmenter=batbox::intl::indigenousSegmenter("grapheme");
  const batbox::format::TruncateServices services{
    [](const std::string& text){
      return static_cast<double>(batbox::text_width::codePoints(text).size());
    },[segmenter]{return segmenter;}};
  std::string line;
  while(std::getline(std::cin,line)){
    const auto input=batbox::value::parse(line);
    const auto text=input.at("text").get<std::string>();
    const auto width=input.at("maxWidth").get<double>();
    const auto value=input.value("kind",std::string())=="width"?
      batbox::format::truncateToWidth(text,width,services):
      batbox::format::truncate(text,width,
        input.value("singleLine",false),services);
    std::cout<<batbox::value::stringify(batbox::Json{{"value",value}})<<'\n';
  }
}
