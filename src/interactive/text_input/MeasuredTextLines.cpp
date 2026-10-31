#include "interactive/text_input/MeasuredText.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace batbox::interactive::text_input {
namespace {
bool space(char16_t c){
  return (c>=9&&c<=13)||c==0x20||c==0xa0||c==0x1680||
    (c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||
    c==0x202f||c==0x205f||c==0x3000||c==0xfeff;
}
std::string trimStart(const std::string& text){
  const auto units=value::utf16CodeUnits(text);
  std::size_t start=0;
  while(start<units.size()&&space(units[start]))++start;
  return value::fromUtf16(std::u16string_view(units).substr(start));
}
std::vector<std::string> splitLines(const std::string& text){
  std::vector<std::string> lines;
  std::size_t start=0;
  for(;;){
    const auto end=text.find('\n',start);
    if(end==std::string::npos){lines.push_back(text.substr(start));break;}
    lines.push_back(text.substr(start,end-start));
    start=end+1;
  }
  return lines;
}
}
std::vector<WrappedLine> MeasuredText::measureWrappedText()const{
  const auto wrapped=services_.wrapAnsi(text_,columns_);
  const auto lines=splitLines(wrapped);
  std::vector<WrappedLine> result;
  std::size_t searchOffset=0;
  std::ptrdiff_t lastNewLinePos=-1;
  for(std::size_t i=0;i<lines.size();++i){
    const auto& text=lines[i];
    const auto isPreceded=[&](std::size_t offset){
      return i==0||(offset>0&&units_[offset-1]==u'\n');
    };
    if(text.empty()){
      const auto found=units_.find(u'\n',
        static_cast<std::size_t>(lastNewLinePos+1));
      if(found!=std::u16string::npos){
        lastNewLinePos=static_cast<std::ptrdiff_t>(found);
        result.push_back({text,found,isPreceded(found),true});
      }else{
        result.push_back({text,length_,isPreceded(length_),false});
      }
      continue;
    }
    const auto lineUnits=value::utf16CodeUnits(text);
    const auto found=units_.find(lineUnits,searchOffset);
    if(found==std::u16string::npos)
      throw std::runtime_error("Failed to find wrapped line in text");
    searchOffset=found+lineUnits.size();
    const bool endsWithNewline=searchOffset<length_&&units_[searchOffset]==u'\n';
    if(endsWithNewline)
      lastNewLinePos=static_cast<std::ptrdiff_t>(searchOffset);
    result.push_back({text,found,isPreceded(found),endsWithNewline});
  }
  return result;
}
const std::vector<WrappedLine>& MeasuredText::getWrappedLines()const{
  if(!wrappedLines_)wrappedLines_=measureWrappedText();
  return *wrappedLines_;
}
std::vector<std::string> MeasuredText::getWrappedText()const{
  std::vector<std::string> result;
  for(const auto& line:getWrappedLines())
    result.push_back(line.isPrecededByNewline?line.text:trimStart(line.text));
  return result;
}
const WrappedLine& MeasuredText::getLine(std::ptrdiff_t line)const{
  const auto& lines=getWrappedLines();
  if(lines.empty())throw std::logic_error("Wrapped lines are empty");
  return lines[std::clamp<std::ptrdiff_t>(line,0,
    static_cast<std::ptrdiff_t>(lines.size()-1))];
}
std::size_t MeasuredText::getOffsetFromPosition(Position position)const{
  const auto& line=getLine(position.line);
  if(line.text.empty()&&line.endsWithNewline)return line.startOffset;
  const std::size_t leading=line.isPrecededByNewline?0:
    value::stringLength(line.text)-value::stringLength(trimStart(line.text));
  const auto index=displayWidthToStringIndex(line.text,
    position.column+static_cast<double>(leading));
  const auto offset=line.startOffset+index;
  const auto lineEnd=line.startOffset+value::stringLength(line.text);
  auto maxOffset=lineEnd;
  if(line.endsWithNewline&&position.column>services_.stringWidth(line.text))
    maxOffset=lineEnd+1;
  return std::min(offset,maxOffset);
}
double MeasuredText::getLineLength(std::ptrdiff_t line)const{
  return services_.stringWidth(getLine(line).text);
}
Position MeasuredText::getPositionFromOffset(std::size_t offset)const{
  const auto& lines=getWrappedLines();
  if(lines.empty())throw std::logic_error("Wrapped lines are empty");
  for(std::size_t index=0;index<lines.size();++index){
    const auto& current=lines[index];
    if(offset<current.startOffset||
       (index+1<lines.size()&&offset>=lines[index+1].startOffset))continue;
    const auto stringPos=offset-current.startOffset;
    double column=0;
    if(current.isPrecededByNewline){
      column=stringIndexToDisplayWidth(current.text,stringPos);
    }else{
      const auto trimmed=trimStart(current.text);
      const auto leading=value::stringLength(current.text)-value::stringLength(trimmed);
      if(stringPos>=leading)
        column=stringIndexToDisplayWidth(trimmed,stringPos-leading);
    }
    return {static_cast<std::ptrdiff_t>(index),std::max(0.0,column)};
  }
  return {static_cast<std::ptrdiff_t>(lines.size()-1),
    services_.stringWidth(lines.back().text)};
}
} // namespace batbox::interactive::text_input
