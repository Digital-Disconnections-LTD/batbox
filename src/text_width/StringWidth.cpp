#include "text_width/StringWidth.hpp"
#include "text_width/CodePoints.hpp"
#include <optional>
namespace batbox::text_width {
struct StringWidth::ShortWidths {
  static constexpr std::size_t maxBytes=64,maxEntries=4096;
  std::mutex mutex;
  std::unordered_map<std::string,double> widths;
};
StringWidth::StringWidth(Services services):shortWidths_(std::make_shared<ShortWidths>()),
  services_(std::move(services)),emoji_(services_.emojiRegex()),bun_(services_.resolveBunStringWidth()){}
double StringWidth::stringWidth(const Json& text)const{
  return bun_?bun_(text,bunOptions_):stringWidthJavaScript(text);
}
namespace {
// Width of text whose bytes are all below DEL and not ESC: the visible
// (non-control) bytes. Matches the pure-ASCII branch below without the
// UTF-16 conversion. Unset when the text needs the full algorithm.
std::optional<double> asciiWidth(std::string_view text){
  double width=0;
  for(const unsigned char code:text){
    if(code>=127||code==0x1b)return std::nullopt;
    if(code>0x1f)++width;
  }
  return width;
}
}
double StringWidth::stringWidthUtf8(std::string_view text)const{
  if(!bun_)if(const auto width=asciiWidth(text))return *width;
  return stringWidth(Json(std::string(text)));
}
double StringWidth::stringWidthJavaScript(const Json& value)const{
  if(!value.is_string()||value.get_ref<const std::string&>().empty())return 0;
  const auto& text=value.get_ref<const std::string&>();
  if(const auto width=asciiWidth(text))return *width;
  const bool shortText=text.size()<=ShortWidths::maxBytes;
  if(shortText){
    std::lock_guard lock(shortWidths_->mutex);
    if(const auto found=shortWidths_->widths.find(text);found!=shortWidths_->widths.end())
      return found->second;
  }
  const auto width=measure(text);
  if(shortText){
    std::lock_guard lock(shortWidths_->mutex);
    if(shortWidths_->widths.size()>=ShortWidths::maxEntries)shortWidths_->widths.clear();
    shortWidths_->widths.emplace(text,width);
  }
  return width;
}
double StringWidth::measure(const std::string& text)const{
  auto str=text;const auto units=value::utf16CodeUnits(str);
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
