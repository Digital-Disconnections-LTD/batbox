#include "messages/RuntimeMessages.hpp"
#include "glob/Regex16.hpp"

namespace batbox::messages {
std::optional<std::string> extractTag(const std::string& html,const std::string& name){
  if(value::trim(html).empty()||value::trim(name).empty())return {};
  std::u16string escaped;
  for(const auto character:glob::units(name)){
    if(std::u16string_view(u"\\^$.*+?()[]{}|").find(character)!=std::u16string_view::npos)
      escaped+=u'\\';
    escaped+=character;
  }
  const glob::Regex16 pattern(u"<"+escaped+u"(?:\\s+[^>]*)?>([\\s\\S]*?)<\\/"+escaped+u">",true);
  const glob::Regex16 opening(u"<"+escaped+u"(?:\\s+[^>]*?)?>",true);
  const glob::Regex16 closing(u"<\\/"+escaped+u">",true);
  const auto source=glob::units(html);
  glob::RegexMatch match;std::size_t offset=0,last=0;
  while(pattern.search(source,offset,&match)){
    const auto before=std::u16string_view(source).substr(last,match.start-last);
    int depth=0;glob::RegexMatch nested;
    for(std::size_t at=0;opening.search(before,at,&nested);at=nested.end)++depth;
    for(std::size_t at=0;closing.search(before,at,&nested);at=nested.end)--depth;
    if(depth==0&&match.groups.size()>1&&match.groups[1]&&!match.groups[1]->empty())
      return glob::text(*match.groups[1]);
    last=match.end;offset=match.end;
  }
  return {};
}
} // namespace batbox::messages
