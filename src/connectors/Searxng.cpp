#include "connectors/Searxng.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace batbox {
namespace {
std::size_t receive(char* data, std::size_t size, std::size_t count, void* context) {
  auto& output = *static_cast<std::string*>(context);
  try { output.append(data, size * count); return size * count; }
  catch (...) { return 0; }
}
std::string lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a'-'A') : c; });
  return text;
}
std::string host(const std::string& url) {
  std::unique_ptr<CURLU, decltype(&curl_url_cleanup)> parsed(curl_url(), curl_url_cleanup);
  if (!parsed || curl_url_set(parsed.get(), CURLUPART_URL, url.c_str(), 0) != CURLUE_OK) return {};
  char* raw = nullptr;
  if (curl_url_get(parsed.get(), CURLUPART_HOST, &raw, 0) != CURLUE_OK) return {};
  std::string result = lower(raw); curl_free(raw);
  if (!result.empty() && result.back() == '.') result.pop_back();
  return result;
}
bool domainMatches(const std::string& hostname, std::string domain) {
  domain = lower(value::trim(domain));
  if (!domain.empty() && domain.back() == '.') domain.pop_back();
  return hostname == domain || (hostname.size() > domain.size() && hostname.ends_with('.' + domain));
}
bool matchesAny(const std::string& hostname, const Json& domains) {
  return domains.is_array() && std::any_of(domains.begin(), domains.end(), [&](const auto& d) {return domainMatches(hostname, d.template get<std::string>());});
}
} // namespace
Searxng::Searxng(std::string url) : endpoint(std::move(url)) {
  static const auto initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
  if (initialized != CURLE_OK) throw std::runtime_error("HTTP initialization failed");
}
Json Searxng::search(const Json& input, const std::string& gadgetUseId, std::chrono::milliseconds timeout,std::stop_token signal) const {
  checkConnectorAbort(signal);
  const auto start = std::chrono::steady_clock::now();
  const auto query = input.at("query").get<std::string>();
  if (value::stringLength(query) < 2) throw std::invalid_argument("The search query must contain at least two characters");
  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> http(curl_easy_init(), curl_easy_cleanup);
  if (!http) throw std::runtime_error("Cannot create SearXNG request");
  char* escaped = curl_easy_escape(http.get(), query.c_str(), static_cast<int>(query.size()));
  if (!escaped) throw std::runtime_error("Cannot encode SearXNG query");
  std::string url = endpoint + (endpoint.find('?') == std::string::npos ? "?" : "&") + "q=" + escaped + "&format=json";
  curl_free(escaped);
  std::string payload;
  curl_easy_setopt(http.get(), CURLOPT_URL, url.c_str());
  curl_easy_setopt(http.get(), CURLOPT_USERAGENT, "batbox/0.0.1");
  curl_easy_setopt(http.get(), CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(http.get(), CURLOPT_CONNECTTIMEOUT_MS, 10000L);
  curl_easy_setopt(http.get(), CURLOPT_TIMEOUT_MS, static_cast<long>(timeout.count()));
  curl_easy_setopt(http.get(), CURLOPT_WRITEFUNCTION, receive);
  curl_easy_setopt(http.get(), CURLOPT_WRITEDATA, &payload);
  curl_easy_setopt(http.get(), CURLOPT_NOPROGRESS,0L);
  curl_easy_setopt(http.get(), CURLOPT_XFERINFOFUNCTION,
    +[](void* context,curl_off_t,curl_off_t,curl_off_t,curl_off_t)->int{
      return static_cast<std::stop_token*>(context)->stop_requested() ? 1 : 0;
    });
  curl_easy_setopt(http.get(), CURLOPT_XFERINFODATA,&signal);
  const auto result = curl_easy_perform(http.get());
  checkConnectorAbort(signal);
  if (result != CURLE_OK) throw std::runtime_error(std::string("SearXNG request failed: ") + curl_easy_strerror(result));
  long status = 0;
  curl_easy_getinfo(http.get(), CURLINFO_RESPONSE_CODE, &status);
  if (status < 200 || status >= 300) throw std::runtime_error("SearXNG returned HTTP " + std::to_string(status));
  auto response = value::parse(payload);
  Json hits = Json::array();
  const auto& allowed = value::property(input, "allowed_domains");
  const auto& blocked = value::property(input, "blocked_domains");
  for (const auto& row : response.at("results")) {
    const auto resultUrl = row.at("url").get<std::string>();
    const auto hostname = host(resultUrl);
    if (allowed.is_array() && !allowed.empty() && !matchesAny(hostname, allowed)) continue;
    if (matchesAny(hostname, blocked)) continue;
    hits.push_back({{"title", row.value("title", "")}, {"url", resultUrl}});
  }
  const double duration = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  return {{"query", query}, {"results", Json::array({{{"tool_use_id", gadgetUseId}, {"content", hits}}})}, {"durationSeconds", duration}};
}
} // namespace batbox
