#include "terminal_render/StyledText.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::terminal_render {
namespace {
bool space(char16_t c){
  return (c>=0x09&&c<=0x0d)||c==0x20||c==0xa0||c==0x1680||
    (c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||
    c==0x202f||c==0x205f||c==0x3000||c==0xfeff;
}
std::vector<std::u16string> splitLines(const std::u16string& text){
  std::vector<std::u16string> result;
  std::size_t start=0;
  while(true){
    const auto end=text.find(u'\n',start);
    if(end==std::u16string::npos){result.push_back(text.substr(start));break;}
    result.push_back(text.substr(start,end-start));start=end+1;
  }
  return result;
}
std::u16string styledRun(const std::u16string& run,
    std::size_t segmentIndex,const StyledSegments& segments,
    const StyledTextImports& imports){
  if(segmentIndex>=segments.size())return run;
  if(!imports.applyTextStyles)
    throw std::invalid_argument("Text style import required");
  const auto& segment=segments[segmentIndex];
  auto styled=imports.applyTextStyles(value::fromUtf16(run),segment.styles);
  if(segment.hyperlink&&!segment.hyperlink->empty())
    styled=wrapWithOsc8Link(styled,*segment.hyperlink);
  return value::utf16CodeUnits(styled);
}
}
std::string applyStylesToWrappedText(const std::string& wrappedPlain,
    const StyledSegments& segments,
    const std::vector<std::size_t>& charToSegment,
    const std::string& originalPlain,bool trimEnabled,
    const StyledTextImports& imports){
  const auto lines=splitLines(value::utf16CodeUnits(wrappedPlain));
  const auto original=value::utf16CodeUnits(originalPlain);
  std::vector<std::u16string> resultLines;
  std::size_t charIndex=0;
  for(std::size_t lineIdx=0;lineIdx<lines.size();lineIdx++){
    const auto& line=lines[lineIdx];
    if(trimEnabled&&!line.empty()){
      const bool lineStartsWithWhitespace=space(line[0]);
      const bool originalHasWhitespace=charIndex<original.size()&&
        space(original[charIndex]);
      if(originalHasWhitespace&&!lineStartsWithWhitespace)
        while(charIndex<original.size()&&space(original[charIndex]))
          charIndex++;
    }
    std::u16string styledLine;
    std::size_t runStart=0;
    std::size_t runSegmentIndex=charIndex<charToSegment.size()?
      charToSegment[charIndex]:0;
    for(std::size_t i=0;i<line.size();i++){
      const auto currentSegmentIndex=charIndex<charToSegment.size()?
        charToSegment[charIndex]:runSegmentIndex;
      if(currentSegmentIndex!=runSegmentIndex){
        styledLine+=styledRun(line.substr(runStart,i-runStart),
          runSegmentIndex,segments,imports);
        runStart=i;
        runSegmentIndex=currentSegmentIndex;
      }
      charIndex++;
    }
    styledLine+=styledRun(line.substr(runStart),runSegmentIndex,
      segments,imports);
    resultLines.push_back(std::move(styledLine));
    if(charIndex<original.size()&&original[charIndex]==u'\n')charIndex++;
    if(trimEnabled&&lineIdx+1<lines.size()){
      const auto& nextLine=lines[lineIdx+1];
      const auto nextFirst=nextLine.empty()?std::optional<char16_t>{}:
        std::optional<char16_t>{nextLine[0]};
      while(charIndex<original.size()&&space(original[charIndex])){
        if(nextFirst&&original[charIndex]==*nextFirst)break;
        charIndex++;
      }
    }
  }
  std::u16string joined;
  for(std::size_t i=0;i<resultLines.size();i++){
    if(i)joined+=u'\n';
    joined+=resultLines[i];
  }
  return value::fromUtf16(joined);
}
} // namespace batbox::terminal_render
