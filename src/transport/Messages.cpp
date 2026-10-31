#include "transport/Messages.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace batbox::transport {
namespace {
std::string toString(const Json& value) {
  if (value.is_string()) return value.get<std::string>();
  if (value.is_object()) return "[object Object]";
  if (!value.is_array()) return value::stringify(value);
  std::string result;
  for (std::size_t i=0;i<value.size();++i) {
    if (i) result += ',';
    if (!value[i].is_null()) result += toString(value[i]);
  }
  return result;
}
std::string nameOf(std::string name) {
  static constexpr std::string_view punctuation = "!#$%&'*+-.^_`|~";
  if (name.empty()) throw std::invalid_argument("Invalid HTTP header name");
  for (auto& c : name) {
    const auto byte = static_cast<unsigned char>(c);
    if (!((byte>='a' && byte<='z') || (byte>='A' && byte<='Z') ||
          (byte>='0' && byte<='9') || punctuation.find(c)!=std::string_view::npos))
      throw std::invalid_argument("Invalid HTTP header name");
    if (c>='A' && c<='Z') c += 'a'-'A';
  }
  return name;
}
std::string headerValue(const Json& value) {
  auto units = value::utf16CodeUnits(toString(value));
  std::string bytes;
  for (auto c : units) {
    if (c>255) throw std::invalid_argument("HTTP header value is outside ByteString range");
    if (c==0 || c=='\r' || c=='\n') throw std::invalid_argument("Invalid HTTP header value");
    bytes += static_cast<char>(c);
  }
  const auto begin=bytes.find_first_not_of("\t ");
  if (begin==std::string::npos) return {};
  return bytes.substr(begin,bytes.find_last_not_of("\t ")-begin+1);
}
void merge(Json& headers, const Json& source) {
  if (source.is_null()) return;
  std::unordered_set<std::string> seen;
  auto row = [&](const std::string& rawName,const Json& rawValues,bool clear) {
    const auto name=nameOf(rawName);
    const auto values=rawValues.is_array() ? rawValues : Json::array({rawValues});
    for (const auto& value : values) {
      if (clear || !seen.contains(name)) { headers.erase(name);seen.insert(name);clear=false; }
      if (value.is_null()) headers[name]=nullptr;
      else {
        const auto next=headerValue(value);
        if (!headers.contains(name) || headers[name].is_null()) headers[name]=next;
        else headers[name]=headers[name].get<std::string>()+", "+next;
      }
    }
  };
  if (source.is_object()) {
    for (auto it=source.begin();it!=source.end();++it) row(it.key(),it.value(),true);
  } else if (source.is_array()) {
    for (const auto& item:source) row(item.at(0).get<std::string>(),item.at(1),false);
  } else throw std::invalid_argument("HTTP headers must be an object or pairs");
}
} // namespace
void assertOneOutputFormat(const Json& params) {
  if (value::truthy(value::property(params,"output_format")) &&
      value::truthy(value::property(value::property(params,"output_config"),"format")))
    throw std::runtime_error("Both output_format and output_config.format were provided. "
      "Please use only output_config.format (output_format is deprecated).");
}
Json transformOutputFormat(const Json& params) {
  const auto& format=value::property(params,"output_format");
  if (!value::truthy(format)) return params;
  assertOneOutputFormat(params);
  Json result=params;
  result.erase("output_format");
  auto config=value::property(params,"output_config");
  if (!config.is_object()) config=Json::object();
  config["format"]=format;
  result["output_config"]=std::move(config);
  return result;
}
Json messageHeaders(const Json& params,const Json& overrides,const MessagesProtocol& protocol) {
  Json generated=Json::object();
  for (const auto& [field,header] : {
       std::pair{"betas",protocol.betaHeader},
       std::pair{"user_profile_id",protocol.userProfileHeader},
       std::pair{"workspace_id",protocol.roostHeader}}) {
    const auto& value=value::property(params,field);
    if (!value.is_null()) generated[header]=toString(value);
  }
  Json result=Json::object();
  merge(result,generated);
  merge(result,overrides);
  return result;
}
MessageRequest prepareMessageRequest(const Json& params,const Json& overrides,const MessagesProtocol& protocol) {
  auto modified=transformOutputFormat(params);
  auto body=modified;
  for (const auto& key: {"betas","user_profile_id","workspace_id"}) body.erase(key);
  return {std::move(body),messageHeaders(modified,overrides,protocol)};
}
} // namespace batbox::transport
