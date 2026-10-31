#include "format/TruncateExtra.hpp"
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
    const auto width=input.at("width").get<double>();
    const auto kind=input.at("kind").get<std::string>();
    batbox::Json value;
    if(kind=="truncatePathMiddle")
      value=batbox::format::truncatePathMiddle(text,width,services);
    else if(kind=="truncateStartToWidth")
      value=batbox::format::truncateStartToWidth(text,width,services);
    else if(kind=="truncateToWidthNoEllipsis")
      value=batbox::format::truncateToWidthNoEllipsis(text,width,services);
    else if(kind=="wrapText")
      value=batbox::format::wrapText(text,width,services);
    else throw std::invalid_argument("Unknown truncate function");
    std::cout<<batbox::value::stringify(batbox::Json{{"value",value}})<<'\n';
  }
}
