#include "git/RemoteIdentity.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include <array>
#include <iomanip>
#include <openssl/sha.h>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unicode/ustring.h>

namespace batbox::git {
namespace {
std::string lowerJs(const std::string& value){
  const auto units=value::utf16CodeUnits(value);
  UErrorCode error=U_ZERO_ERROR;
  const auto required=u_strToLower(nullptr,0,
    reinterpret_cast<const UChar*>(units.data()),static_cast<int32_t>(units.size()),"root",&error);
  if(error!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(error))
    throw std::runtime_error("Unicode case conversion failed");
  std::u16string out(static_cast<std::size_t>(required),u'\0');
  error=U_ZERO_ERROR;
  u_strToLower(reinterpret_cast<UChar*>(out.data()),required,
    reinterpret_cast<const UChar*>(units.data()),static_cast<int32_t>(units.size()),"root",&error);
  if(U_FAILURE(error))throw std::runtime_error("Unicode case conversion failed");
  return value::fromUtf16(out);
}
std::string hashPrefix(const std::string& value){
  const auto bytes=value::encodeUtf8(value);
  std::array<unsigned char,SHA256_DIGEST_LENGTH> digest{};
  SHA256(reinterpret_cast<const unsigned char*>(bytes.data()),bytes.size(),digest.data());
  std::ostringstream out;out<<std::hex<<std::setfill('0');
  for(std::size_t index=0;index<8;++index)out<<std::setw(2)<<static_cast<unsigned>(digest[index]);
  return out.str();
}
}
bool isLocalHost(const std::string& host){
  const auto colon=host.find(':');
  const auto withoutPort=host.substr(0,colon);
  return withoutPort=="localhost"||std::regex_match(withoutPort,
    std::regex(R"(^127\.\d{1,3}\.\d{1,3}\.\d{1,3}$)"));
}
std::optional<std::string> normalizeGitRemoteUrl(const std::string& url){
  const auto trimmed=value::trim(url);
  if(trimmed.empty())return std::nullopt;
  std::smatch match;
  if(std::regex_match(trimmed,match,
    std::regex(R"(^git@([^:]+):(.+?)(?:\.git)?$)"))&&
    match.size()>2&&!match[1].str().empty()&&!match[2].str().empty())
    return lowerJs(match[1].str()+"/"+match[2].str());
  if(std::regex_match(trimmed,match,
    std::regex(R"(^(?:https?|ssh):\/\/(?:[^@]+@)?([^/]+)\/(.+?)(?:\.git)?$)"))&&
    match.size()>2&&!match[1].str().empty()&&!match[2].str().empty()){
    const auto host=match[1].str(),path=match[2].str();
    if(isLocalHost(host)&&path.rfind("git/",0)==0){
      const auto proxyPath=path.substr(4);
      std::size_t segments=1;
      for(char ch:proxyPath)if(ch=='/')++segments;
      const auto first=proxyPath.substr(0,proxyPath.find('/'));
      if(segments>=3&&first.find('.')!=std::string::npos)
        return lowerJs(proxyPath);
      return lowerJs("github.com/"+proxyPath);
    }
    return lowerJs(host+"/"+path);
  }
  return std::nullopt;
}
event_loop::Deferred<std::optional<std::string>> getRepoRemoteHash(
  event_loop::Loop& loop,
  const std::function<event_loop::Deferred<std::optional<std::string>>()>& getRemoteUrl){
  if(!loop.isOwnerThread())throw std::logic_error("Git remote hash owner required");
  event_loop::Deferred<std::optional<std::string>> output(loop);
  try{
    if(!getRemoteUrl)throw std::invalid_argument("getRemoteUrl import required");
    auto pending=getRemoteUrl();
    pending.then([output](const std::optional<std::string>* remote,
      const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        if(!*remote){output.resolve(std::nullopt);return;}
        const auto normalized=normalizeGitRemoteUrl(**remote);
        if(!normalized){output.resolve(std::nullopt);return;}
        output.resolve(hashPrefix(*normalized));
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::git
