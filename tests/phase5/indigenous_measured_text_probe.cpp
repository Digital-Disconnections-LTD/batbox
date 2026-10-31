#include "interactive/text_input/MeasuredText.hpp"
#include "intl/IndigenousInternal.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Unicode.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using namespace batbox;
using batbox::interactive::text_input::MeasuredText;
using JsonOut=nlohmann::ordered_json;
int main(){
  interactive::text_input::MetricsServices services;
  services.normalizeNfc=value::normalizeNFC;
  services.grapheme=intl::indigenousSegmenter("grapheme");
  services.word=intl::indigenousSegmenter("word");
  services.stringWidth=[grapheme=services.grapheme](const std::string& text){
    double width=0;
    auto cursor=grapheme->segment(text);
    while(const auto item=cursor.next()){
      const auto& segment=item->text;
      width+=segment.find("👩")!=std::string::npos?2:
        static_cast<double>(value::stringLength(segment));
    }
    return width;
  };
  services.wrapAnsi=[](const std::string& text,double){
    return text=="abc de"?std::string("abc\n de"):text;
  };
  const std::pair<std::string,double> cases[]={
    {"",10},{"a\nb",10},{"e\xcc\x81X",10},{"👩‍💻x",10},
    {"abc def",10},{"abc de",4}};
  for(const auto& [text,columns]:cases){
    MeasuredText m(text,columns,services);
    JsonOut out={{"text",m.text()},{"wrapped",JsonOut::array()},
      {"visible",m.getWrappedText()},{"graphemes",JsonOut::array()},
      {"positions",JsonOut::array()},{"words",JsonOut::array()}};
    for(const auto& line:m.getWrappedLines())
      out["wrapped"].push_back(JsonOut::array({line.text,line.startOffset,
        line.isPrecededByNewline,line.endsWithNewline}));
    for(std::size_t i=0;i<=m.length();++i){
      out["graphemes"].push_back(JsonOut::array({m.prevOffset(i),
        m.nextOffset(i),m.snapToGraphemeBoundary(i)}));
      const auto p=m.getPositionFromOffset(i);
      out["positions"].push_back(JsonOut::array({p.line,static_cast<int>(p.column),
        m.getOffsetFromPosition(p)}));
    }
    for(const auto& word:m.getWordBoundaries())
      out["words"].push_back(JsonOut::array({word.start,word.end,
        word.isWordLike}));
    std::cout<<out.dump()<<'\n';
  }
}
