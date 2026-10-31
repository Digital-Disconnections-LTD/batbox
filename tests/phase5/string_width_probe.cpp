#include "text_width/StringWidth.hpp"
#include "text_width/IndigenousData.hpp"
#include <iostream>
#include <iterator>
#include <openssl/sha.h>
using namespace batbox;
int main(){const auto input=value::parse(std::string(std::istreambuf_iterator<char>{std::cin},{}));Json out=Json::array();
 for(const auto& item:input){try{
  if(item.value("op",std::string())=="wideDigest"){
   std::string values;values.reserve(0x110000);
   for(std::uint32_t cp=0;cp<0x110000;++cp)values.push_back(static_cast<char>(text_width::data::eastAsianWidth(cp)));
   unsigned char digest[SHA256_DIGEST_LENGTH];SHA256(reinterpret_cast<const unsigned char*>(values.data()),values.size(),digest);
   std::string hex;for(const auto byte:digest){hex+="0123456789abcdef"[byte>>4];hex+="0123456789abcdef"[byte&15];}out.push_back(hex);continue;
  }
  auto stash=std::make_shared<intl::Stash>(intl::indigenousServices());auto services=text_width::indigenousServices(stash);Json calls=Json::array();
  if(item.value("bun",false))services.resolveBunStringWidth=[&]{calls.push_back("resolveBun");return text_width::BunWidth([&](const Json& text,const Json& options){calls.push_back(Json{{"text",text},{"options",options}});return 314.0;});};
  text_width::StringWidth width(services);const auto text=item.at("text");
  Json result={{"width",width.stringWidth(text)},{"calls",calls}};
  if(text.is_string()){
   const auto str=text.get<std::string>();result["needs"]=text_width::needsSegmentation(str);result["emojiWidth"]=text_width::getEmojiWidth(str);
  }
  if(item.contains("codePoint"))result["zero"]=text_width::isZeroWidth(item["codePoint"]);
  out.push_back(result);
 }catch(const std::exception& e){out.push_back(Json{{"error",e.what()}});}}
 std::cout<<value::stringify(out)<<'\n';
}
