#pragma once
#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace batbox::providers::aws {
struct Credentials {
  std::string accessKeyId;
  std::string secretAccessKey;
  std::string sessionToken;
  std::optional<std::chrono::system_clock::time_point> expiration;
  bool empty() const { return accessKeyId.empty() || secretAccessKey.empty(); }
};
using HeaderList = std::vector<std::pair<std::string,std::string>>;
struct SigningRequest {
  std::string method = "POST";
  // Path and query exactly as they go on the wire, for example "/model/a%3A0/invoke".
  std::string target = "/";
  HeaderList headers;
  std::string body;
};
struct SigningOptions {
  std::string region;
  std::string service;
  std::chrono::system_clock::time_point time = std::chrono::system_clock::now();
  bool normalizePath = true;
  bool addContentSha256Header = true;
  bool signSessionToken = true;
};
struct SigningResult {
  std::string canonicalRequest;
  std::string stringToSign;
  std::string signature;
  std::string authorization;
  std::string amzDate;
  std::string payloadHash;
  // The request headers plus x-amz-date, x-amz-content-sha256, x-amz-security-token and authorization.
  HeaderList headers;
};
std::string uriEncode(std::string_view text, bool keepSlash);
std::string canonicalPath(std::string_view path, bool normalize);
std::string canonicalQuery(std::string_view query);
std::string amzDate(std::chrono::system_clock::time_point time);
std::string signingKey(std::string_view secret, std::string_view date, std::string_view region, std::string_view service);
SigningResult sign(const SigningRequest& request, const Credentials& credentials, const SigningOptions& options);
} // namespace batbox::providers::aws
