#include "providers/aws/SigV4.hpp"
#include "providers/cloud/Crypto.hpp"
#include <algorithm>
#include <ctime>
#include <map>
#include <stdexcept>

namespace batbox::providers::aws {
namespace {
using cloud::hmacSha256;
using cloud::sha256Hex;
bool unreserved(unsigned char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
         c == '-' || c == '_' || c == '.' || c == '~';
}
int hexValue(char c) {
  if (c >= '0' && c <= '9') return c-'0';
  if (c >= 'a' && c <= 'f') return c-'a'+10;
  if (c >= 'A' && c <= 'F') return c-'A'+10;
  return -1;
}
std::string percentDecode(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '%' && i+2 < text.size() && hexValue(text[i+1]) >= 0 && hexValue(text[i+2]) >= 0) {
      out += static_cast<char>(hexValue(text[i+1])*16+hexValue(text[i+2]));
      i += 2;
    } else out += text[i];
  }
  return out;
}
std::string lower(std::string text) {
  std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c) { return c >= 'A' && c <= 'Z' ? c+('a'-'A') : c; });
  return text;
}
std::string canonicalValue(std::string_view value) {
  std::string out;
  bool space = false;
  for (const char c : value) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { space = !out.empty(); continue; }
    if (space) { out += ' '; space = false; }
    out += c;
  }
  return out;
}
bool hasHeader(const HeaderList& headers, std::string_view name) {
  return std::any_of(headers.begin(),headers.end(),[&](const auto& header) { return lower(header.first) == name; });
}
} // namespace
std::string uriEncode(std::string_view text, bool keepSlash) {
  static constexpr char digits[] = "0123456789ABCDEF";
  std::string out;
  for (const unsigned char c : text) {
    if (unreserved(c) || (keepSlash && c == '/')) out += static_cast<char>(c);
    else { out += '%'; out += digits[c >> 4]; out += digits[c & 15]; }
  }
  return out;
}
std::string canonicalPath(std::string_view path, bool normalize) {
  if (path.empty()) return "/";
  std::string working(path);
  if (normalize) {
    std::vector<std::string> segments;
    std::size_t start = 1;
    const bool trailing = working.size() > 1 && (working.ends_with('/') || working.ends_with("/.") || working.ends_with("/.."));
    while (start <= working.size()) {
      auto end = working.find('/',start);
      if (end == std::string::npos) end = working.size();
      const auto segment = working.substr(start,end-start);
      if (segment == "..") { if (!segments.empty()) segments.pop_back(); }
      else if (!segment.empty() && segment != ".") segments.push_back(segment);
      start = end+1;
    }
    working = "/";
    for (std::size_t i = 0; i < segments.size(); ++i) working += (i ? "/" : "")+segments[i];
    if (trailing && !segments.empty()) working += '/';
  }
  // Services other than S3 encode the wire path a second time.
  return uriEncode(working,true);
}
std::string canonicalQuery(std::string_view query) {
  std::vector<std::pair<std::string,std::string>> pairs;
  std::size_t start = 0;
  while (start <= query.size() && !query.empty()) {
    auto end = query.find('&',start);
    if (end == std::string_view::npos) end = query.size();
    const auto item = query.substr(start,end-start);
    if (!item.empty()) {
      const auto equals = item.find('=');
      const auto key = percentDecode(item.substr(0,equals));
      const auto value = equals == std::string_view::npos ? std::string() : percentDecode(item.substr(equals+1));
      pairs.emplace_back(uriEncode(key,false),uriEncode(value,false));
    }
    start = end+1;
  }
  std::sort(pairs.begin(),pairs.end());
  std::string out;
  for (const auto& [key,value] : pairs) out += (out.empty() ? "" : "&")+key+"="+value;
  return out;
}
std::string amzDate(std::chrono::system_clock::time_point time) {
  const auto seconds = std::chrono::system_clock::to_time_t(time);
  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc,&seconds);
#else
  gmtime_r(&seconds,&utc);
#endif
  char buffer[32];
  std::strftime(buffer,sizeof buffer,"%Y%m%dT%H%M%SZ",&utc);
  return buffer;
}
std::string signingKey(std::string_view secret, std::string_view date, std::string_view region, std::string_view service) {
  const auto dateKey = hmacSha256("AWS4"+std::string(secret),date);
  const auto regionKey = hmacSha256(dateKey,region);
  const auto serviceKey = hmacSha256(regionKey,service);
  return hmacSha256(serviceKey,"aws4_request");
}
SigningResult sign(const SigningRequest& request, const Credentials& credentials, const SigningOptions& options) {
  if (credentials.empty()) throw std::invalid_argument("AWS credentials are missing an access key id or secret access key");
  if (options.region.empty() || options.service.empty()) throw std::invalid_argument("SigV4 needs a region and a service name");
  SigningResult result;
  result.headers = request.headers;
  result.payloadHash = sha256Hex(request.body);
  if (hasHeader(result.headers,"x-amz-date")) {
    for (const auto& [name,value] : result.headers) if (lower(name) == "x-amz-date") result.amzDate = canonicalValue(value);
  } else {
    result.amzDate = amzDate(options.time);
    result.headers.emplace_back("x-amz-date",result.amzDate);
  }
  if (options.addContentSha256Header && !hasHeader(result.headers,"x-amz-content-sha256"))
    result.headers.emplace_back("x-amz-content-sha256",result.payloadHash);
  if (!credentials.sessionToken.empty() && options.signSessionToken && !hasHeader(result.headers,"x-amz-security-token"))
    result.headers.emplace_back("x-amz-security-token",credentials.sessionToken);

  std::map<std::string,std::vector<std::string>> canonical;
  for (const auto& [name,value] : result.headers) {
    auto key = lower(name);
    if (key != "authorization") canonical[std::move(key)].push_back(canonicalValue(value));
  }
  std::string canonicalHeaders, signedHeaders;
  for (const auto& [name,values] : canonical) {
    canonicalHeaders += name+":";
    for (std::size_t i = 0; i < values.size(); ++i) canonicalHeaders += (i ? "," : "")+values[i];
    canonicalHeaders += "\n";
    signedHeaders += (signedHeaders.empty() ? "" : ";")+name;
  }
  const auto question = request.target.find('?');
  const auto path = request.target.substr(0,question);
  const auto query = question == std::string::npos ? std::string() : request.target.substr(question+1);
  result.canonicalRequest = request.method+"\n"+canonicalPath(path,options.normalizePath)+"\n"+canonicalQuery(query)+"\n"+
                            canonicalHeaders+"\n"+signedHeaders+"\n"+result.payloadHash;
  const auto date = result.amzDate.substr(0,8);
  const auto scope = date+"/"+options.region+"/"+options.service+"/aws4_request";
  result.stringToSign = "AWS4-HMAC-SHA256\n"+result.amzDate+"\n"+scope+"\n"+sha256Hex(result.canonicalRequest);
  result.signature = cloud::toHex(hmacSha256(signingKey(credentials.secretAccessKey,date,options.region,options.service),result.stringToSign));
  result.authorization = "AWS4-HMAC-SHA256 Credential="+credentials.accessKeyId+"/"+scope+", SignedHeaders="+signedHeaders+
                         ", Signature="+result.signature;
  if (!credentials.sessionToken.empty() && !options.signSessionToken && !hasHeader(result.headers,"x-amz-security-token"))
    result.headers.emplace_back("x-amz-security-token",credentials.sessionToken);
  result.headers.emplace_back("authorization",result.authorization);
  return result;
}
} // namespace batbox::providers::aws
