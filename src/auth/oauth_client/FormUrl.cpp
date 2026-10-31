#include "auth/oauth_client/FormUrl.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Url.hpp"
#include <stdexcept>

namespace batbox::auth::oauth_client {
namespace {
int hex(char c){
  if(c>='0'&&c<='9')return c-'0';
  if(c>='a'&&c<='f')return c-'a'+10;
  if(c>='A'&&c<='F')return c-'A'+10;
  return -1;
}
std::string decode(std::string_view text){
  std::string bytes;
  for(std::size_t i=0;i<text.size();++i){
    if(text[i]=='+'){bytes.push_back(' ');continue;}
    if(text[i]=='%'&&i+2<text.size()&&hex(text[i+1])>=0&&
        hex(text[i+2])>=0){
      bytes.push_back(static_cast<char>((hex(text[i+1])<<4)|hex(text[i+2])));
      i+=2;continue;
    }
    bytes.push_back(text[i]);
  }
  return value::decodeUtf8(bytes);
}
std::string encode(std::string_view text){
  constexpr char digits[]="0123456789ABCDEF";
  const auto bytes=value::encodeUtf8(text);
  std::string result;
  for(unsigned char c:bytes){
    if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
        (c>='0'&&c<='9')||c=='*'||c=='-'||c=='.'||c=='_')
      result.push_back(static_cast<char>(c));
    else if(c==' ')result.push_back('+');
    else{result.push_back('%');result.push_back(digits[c>>4]);
      result.push_back(digits[c&15]);}
  }
  return result;
}
}
QueryEntries parseFormQuery(std::string_view query){
  if(!query.empty()&&query.front()=='?')query.remove_prefix(1);
  QueryEntries entries;
  if(query.empty())return entries;
  std::size_t start=0;
  while(start<=query.size()){
    const auto end=query.find('&',start);
    const auto part=query.substr(start,end==std::string_view::npos?
      query.size()-start:end-start);
    if(!part.empty()){
      const auto equal=part.find('=');
      entries.emplace_back(decode(part.substr(0,equal)),
        equal==std::string_view::npos?std::string{}:decode(part.substr(equal+1)));
    }
    if(end==std::string_view::npos)break;
    start=end+1;
  }
  return entries;
}
std::string serializeFormQuery(const QueryEntries& entries){
  std::string result;
  for(const auto& [key,value]:entries){
    if(!result.empty())result.push_back('&');
    result+=encode(key);result.push_back('=');result+=encode(value);
  }
  return result;
}
std::string appendFormQuery(std::string_view href,
    const QueryEntries& additions){
  const auto parsed=runtime::parseUrl(std::string(href));
  if(!parsed)throw std::invalid_argument("Invalid OAuth authorization URL");
  auto entries=parseFormQuery((*parsed)["search"].get<std::string>());
  entries.insert(entries.end(),additions.begin(),additions.end());
  std::string output=(*parsed)["href"].get<std::string>();
  const auto hash=output.find('#');
  const std::string fragment=hash==std::string::npos?std::string{}:
    output.substr(hash);
  if(hash!=std::string::npos)output.erase(hash);
  const auto question=output.find('?');
  if(question!=std::string::npos)output.erase(question);
  output.push_back('?');output+=serializeFormQuery(entries);output+=fragment;
  return output;
}
}
