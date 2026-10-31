#include "text_width/StringWidth.hpp"
#include "text_width/CodePoints.hpp"
namespace batbox::text_width {
StringWidth::StringWidth(Services services):services_(std::move(services)),emoji_(services_.emojiRegex()),bun_(services_.resolveBunStringWidth()){}
double StringWidth::stringWidth(const Json& text)const{
  return bun_?bun_(text,bunOptions_):stringWidthJavaScript(text);
}
double StringWidth::stringWidthJavaScript(const Json& value)const{
  if(!value.is_string()||value.get_ref<const std::string&>().empty())return 0;
  auto str=value.get<std::string>();const auto units=value::utf16CodeUnits(str);
  bool pure=true;
  for(const auto code:units)if(code>=127||code==0x1b){pure=false;break;}
  if(pure){double width=0;for(const auto code:units)if(code>0x1f)++width;return width;}
  if(str.find('\x1b')!=std::string::npos){str=services_.stripAnsi(str);if(str.empty())return 0;}
  if(!needsSegmentation(str)){
    double width=0;
    for(const auto cp:codePoints(str))if(!isZeroWidth(cp))width+=services_.eastAsianWidth(cp);
    return width;
  }
  double width=0;auto cursor=services_.getGraphemeSegmenter()->segment(str);
  while(const auto grapheme=cursor.next()){
    emoji_->reset();
    if(emoji_->test(grapheme->text)){width+=getEmojiWidth(grapheme->text);continue;}
    for(const auto cp:codePoints(grapheme->text))if(!isZeroWidth(cp)){
      width+=services_.eastAsianWidth(cp);break;
    }
  }
  return width;
}
} // namespace batbox::text_width
