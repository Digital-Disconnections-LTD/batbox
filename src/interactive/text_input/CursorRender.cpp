#include "interactive/text_input/Cursor.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive::text_input {
namespace {
bool space(char16_t c){
  return (c>=9&&c<=13)||c==0x20||c==0xa0||c==0x1680||
    (c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||
    c==0x202f||c==0x205f||c==0x3000||c==0xfeff;
}
std::string trimEnd(const std::string& text){
  const auto units=value::utf16CodeUnits(text);
  auto end=units.size();
  while(end>0&&space(units[end-1]))--end;
  return value::fromUtf16(std::u16string_view(units).substr(0,end));
}
std::vector<intl::Segment> segments(const intl::Segmenter& segmenter,
                                     const std::string& text){
  std::vector<intl::Segment> result;
  auto cursor=segmenter.segment(text);
  while(const auto item=cursor.next())result.push_back(*item);
  return result;
}
}
std::string Cursor::render(const std::string& cursorChar,
    const std::string& mask,
    const std::function<std::string(const std::string&)>& invert,
    const std::optional<GhostText>& ghostText,
    std::optional<std::size_t> maxVisibleLines)const{
  if(!invert)throw std::invalid_argument("Cursor inversion import required");
  const auto [line,column]=getPosition();
  const auto allLines=measured_->getWrappedText();
  const auto start=getViewportStartLine(maxVisibleLines);
  const auto end=maxVisibleLines&&*maxVisibleLines>0?
    std::min(allLines.size(),start+*maxVisibleLines):allLines.size();
  std::string result;
  for(auto current=start;current<end;++current){
    if(current>start)result+='\n';
    std::string display=allLines[current];
    if(!mask.empty()){
      const auto graphemes=segments(*measured_->services().grapheme,display);
      if(current==allLines.size()-1){
        const auto visible=std::min<std::size_t>(6,graphemes.size());
        const auto hidden=graphemes.size()-visible;
        const auto split=graphemes.size()>visible?
          graphemes[hidden].index:0;
        display.clear();
        for(std::size_t n=0;n<hidden;++n)display+=mask;
        display+=value::slice(allLines[current],split);
      }else{
        display.clear();
        for(std::size_t n=0;n<graphemes.size();++n)display+=mask;
      }
    }
    if(line!=static_cast<std::ptrdiff_t>(current)){
      result+=trimEnd(display);
      continue;
    }
    std::string before,at=cursorChar,after;
    double width=0;
    bool found=false;
    for(const auto& segment:segments(*measured_->services().grapheme,display)){
      if(found){after+=segment.text;continue;}
      const auto next=width+measured_->width(segment.text);
      if(next>column){at=segment.text;found=true;}
      else{width=next;before+=segment.text;}
    }
    std::string rendered,ghostSuffix;
    if(ghostText&&current==allLines.size()-1&&isAtEnd()&&
       !ghostText->text.empty()){
      const auto ghostSegments=segments(*measured_->services().grapheme,
                                        ghostText->text);
      const auto first=!ghostSegments.empty()?ghostSegments.front().text:
        value::substring(ghostText->text,0,1);
      rendered=cursorChar.empty()?first:invert(first);
      const auto rest=value::slice(ghostText->text,value::stringLength(first));
      if(!rest.empty()){
        if(!ghostText->dim)
          throw std::invalid_argument("Ghost text dim import required");
        ghostSuffix=ghostText->dim(rest);
      }
    }else rendered=cursorChar.empty()?at:invert(at);
    result+=before+rendered+ghostSuffix+trimEnd(after);
  }
  return result;
}
} // namespace batbox::interactive::text_input
