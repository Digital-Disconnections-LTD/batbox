#include "text_width/StringWidth.hpp"
#include "text_width/IndigenousData.hpp"
#include "glob/Regex16.hpp"
#include <stdexcept>
namespace batbox::text_width {
namespace {
bool hex(char16_t c){return (c>=u'0'&&c<=u'9')||(c>=u'a'&&c<=u'f')||(c>=u'A'&&c<=u'F');}
std::u16string pcrePattern(const std::string& source){
  const auto units=value::utf16CodeUnits(source);std::u16string result;
  for(std::size_t i=0;i<units.size();++i){
    if(units[i]==u'\\'&&i+1<units.size()){
      if(units[i+1]==u'u'&&i+5<units.size()&&hex(units[i+2])&&hex(units[i+3])&&hex(units[i+4])&&hex(units[i+5])){
        result+=u"\\x{";result+=units.substr(i+2,4);result+=u'}';i+=5;continue;
      }
      result+=units[i];result+=units[++i];continue;
    }
    result+=units[i];
  }
  return result;
}
}
Services indigenousServices(std::shared_ptr<intl::Stash> stash){
  if(!stash)throw std::invalid_argument("Terminal-width Intl cache required");
  Services services;
  services.emojiRegex=[](){
    const auto regex=std::make_shared<glob::Regex16>(pcrePattern(data::emojiPattern()));
    auto result=std::make_shared<EmojiRegex>();result->reset=[]{};
    result->test=[regex](const std::string& text){return regex->search(value::utf16CodeUnits(text),0);};
    return result;
  };
  services.resolveBunStringWidth=[](){return BunWidth{};};
  const auto ansi=std::make_shared<glob::Regex16>(pcrePattern(data::ansiPattern()));
  services.stripAnsi=[ansi](const std::string& text){
    return value::fromUtf16(ansi->replace(value::utf16CodeUnits(text),
      [](const glob::RegexMatch&){return std::u16string{};},true));
  };
  services.eastAsianWidth=data::eastAsianWidth;
  services.getGraphemeSegmenter=[stash=std::move(stash)]{return stash->getGraphemeSegmenter();};
  return services;
}
} // namespace batbox::text_width
