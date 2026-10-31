#include "application_lsp/Symbol.hpp"
#include "runtime/JsonValue.hpp"
#include <array>
#include <fstream>

namespace batbox::application_lsp {
namespace {
bool word(char16_t c){return c==u'_'||c==u'$'||c==u'\''||c==u'!'||
  (c>=u'a'&&c<=u'z')||(c>=u'A'&&c<=u'Z')||(c>=u'0'&&c<=u'9');}
bool operatorChar(char16_t c){
  constexpr std::u16string_view chars=u"+-*/%&|^~<>=";
  return chars.find(c)!=std::u16string_view::npos;
}
}
std::optional<std::string> symbolAtPosition(const std::filesystem::path& path,
    int zeroBasedLine,int zeroBasedCharacter){
  if(zeroBasedLine<0||zeroBasedCharacter<0)return {};
  try{
    std::ifstream file(path,std::ios::binary);
    if(!file)return {};
    std::array<char,65536> bytes{};
    file.read(bytes.data(),bytes.size());
    auto count=static_cast<std::size_t>(file.gcount());
    auto content=value::utf16CodeUnits(std::string_view(bytes.data(),count));
    std::size_t start=0;int line=0;
    while(line<zeroBasedLine){auto newline=content.find(u'\n',start);
      if(newline==std::u16string::npos)return {};
      start=newline+1;++line;}
    auto end=content.find(u'\n',start);
    if(end==std::u16string::npos){
      if(count==bytes.size())return {};
      end=content.size();
    }
    auto width=end-start;
    if(static_cast<std::size_t>(zeroBasedCharacter)>=width)return {};
    auto offset=start+static_cast<std::size_t>(zeroBasedCharacter);
    auto classify=word(content[offset])?word:(operatorChar(content[offset])?operatorChar:nullptr);
    if(!classify)return {};
    auto left=offset,right=offset+1;
    while(left>start&&classify(content[left-1]))--left;
    while(right<end&&classify(content[right]))++right;
    if(right-left>30)right=left+30;
    return value::fromUtf16(std::u16string_view(content).substr(left,right-left));
  }catch(...){return {};}
}
}
