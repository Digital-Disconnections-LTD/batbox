#include "runtime/Url.hpp"
#include "runtime/Encoding.hpp"
#include <ada.h>

namespace batbox::runtime {
bool canParseUrl(const std::string& value){
  return ada::can_parse(value::encodeUtf8(value));
}
std::optional<std::string> canonicalUrl(const std::string& value){
  const auto parsed=ada::parse<ada::url_aggregator>(value::encodeUtf8(value));
  if(!parsed)return std::nullopt;
  return std::string(parsed->get_href());
}
std::optional<Json> parseUrl(const std::string& value,const std::optional<std::string>& base){
  const auto input=value::encodeUtf8(value);
  auto parsed=ada::parse<ada::url_aggregator>(input);
  if(base){
    auto baseUrl=ada::parse<ada::url_aggregator>(value::encodeUtf8(*base));
    if(!baseUrl)return std::nullopt;
    parsed=ada::parse<ada::url_aggregator>(input,&*baseUrl);
  }
  if(!parsed)return std::nullopt;
  return Json{{"href",parsed->get_href()},{"protocol",parsed->get_protocol()},
    {"username",parsed->get_username()},{"password",parsed->get_password()},
    {"host",parsed->get_host()},{"hostname",parsed->get_hostname()},
    {"port",parsed->get_port()},{"pathname",parsed->get_pathname()},
    {"search",parsed->get_search()},{"hash",parsed->get_hash()},
    {"origin",parsed->get_origin()}};
}
}
