#include "connectors/Searxng.hpp"
#include "runtime/Loopback.hpp"
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
constexpr std::size_t excerptCharacters = 120;
// The first characters of a body on one line, cut on a UTF-8 boundary.
std::string excerpt(const std::string& body) {
  std::string text;
  std::size_t characters = 0, index = 0;
  bool space = false;
  while (index < body.size() && characters < excerptCharacters) {
    const auto c = static_cast<unsigned char>(body[index]);
    if (c < 0x20 || c == 0x7f || c == ' ') {
      space = !text.empty();
      ++index;
      continue;
    }
    std::size_t length = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xe ? 3 : (c >> 3) == 0x1e ? 4 : 0;
    bool valid = length > 0 && index + length <= body.size();
    for (std::size_t k = 1; valid && k < length; ++k)
      valid = (static_cast<unsigned char>(body[index + k]) & 0xc0) == 0x80;
    if (valid && length == 2 && c == 0xc2 && static_cast<unsigned char>(body[index + 1]) < 0xa0) {
      space = !text.empty();
      index += 2;
      continue;
    }
    if (space) {
      if (characters + 1 >= excerptCharacters) break;
      text += ' '; ++characters; space = false;
    }
    if (valid) text.append(body, index, length);
    else { text += "\xEF\xBF\xBD"; length = 1; }
    index += length;
    ++characters;
  }
  if (text.empty()) return "an empty body";
  bool more = false;
  for (; index < body.size() && !more; ++index) {
    const auto c = static_cast<unsigned char>(body[index]);
    more = c >= 0x20 && c != 0x7f && c != ' ';
  }
  return "\"" + text + (more ? "...\"" : "\"");
}
std::string answer(long status, const std::string& contentType, const std::string& body) {
  return "HTTP " + std::to_string(status) + " (" +
    (contentType.empty() ? std::string("no content type") : contentType) + "): " + excerpt(body);
}
std::string jsonKind(const Json& value) {
  if (value.is_object()) return "an object";
  if (value.is_array()) return "an array";
  if (value.is_string()) return "a string";
  if (value.is_boolean()) return "a boolean";
  if (value.is_number()) return "a number";
  return "null";
}
std::string keyList(const Json& object) {
  std::string keys;
  std::size_t listed = 0;
  for (auto it = object.begin(); it != object.end(); ++it) {
    if (listed == 8) { keys += ", ..."; break; }
    keys += (listed++ ? ", " : "") + it.key();
  }
  return keys.empty() ? "none" : keys;
}
const char* const shapeFix = "this SearXNG answers in a shape Batbox does not read; run: batbox setup web "
  "for the tested version, or check that searxng_url names a SearXNG /search endpoint";
} // namespace
SearxngResponseError::SearxngResponseError(const std::string& endpoint, SearxngProblem found)
  : std::runtime_error("SearXNG at " + endpoint + ": " + found.detail + ". Fix: " + found.fix),
    problem(std::move(found)) {}

Json searxngResults(const std::string& endpoint, long status, const std::string& contentType,
    const std::string& body) {
  using Kind = SearxngProblem::Kind;
  if (status != 200) {
    if (status == 403) throw SearxngResponseError(endpoint, {Kind::JsonOff,
      "expected HTTP 200 with JSON search results, got " + answer(status, contentType, body),
      "SearXNG answers 403 when search.formats in its settings.yml lacks json; add json there "
      "and restart it, or run: batbox setup web"});
    throw SearxngResponseError(endpoint, {Kind::HttpStatus,
      "expected HTTP 200 with JSON search results, got " + answer(status, contentType, body),
      status >= 300 && status < 400
        ? "set searxng_url to the full /search endpoint (the homepage redirects), or run: batbox setup web"
        : "check that searxng_url names the /search endpoint of a running SearXNG, or run: batbox setup web"});
  }
  Json response;
  try { response = value::parse(body); }
  catch (...) {
    throw SearxngResponseError(endpoint, {Kind::NotJson,
      "expected a JSON body, got " + answer(status, contentType, body),
      "check that searxng_url names a SearXNG /search endpoint with json in search.formats, "
      "or run: batbox setup web"});
  }
  if (!response.is_object() || !response.contains("results") || !response.at("results").is_array()) {
    std::string found;
    if (!response.is_object()) found = jsonKind(response);
    else if (!response.contains("results")) found = "an object without \"results\" (keys: " + keyList(response) + ")";
    else found = "\"results\" as " + jsonKind(response.at("results"));
    throw SearxngResponseError(endpoint, {Kind::NoResults,
      "expected a JSON object with a \"results\" array, got " + found, shapeFix});
  }
  const auto& rows = response.at("results");
  Json usable = Json::array();
  for (const auto& row : rows)
    if (row.is_object() && row.contains("url") && row.at("url").is_string()) usable.push_back(row);
  if (usable.empty() && !rows.empty()) {
    const auto& first = rows.front();
    throw SearxngResponseError(endpoint, {Kind::NoUrl,
      "expected each result to have a \"url\" string, got " + std::to_string(rows.size()) +
      (rows.size() == 1 ? " result" : " results") + " without one (first result: " +
      (first.is_object() ? "keys " + keyList(first) : jsonKind(first)) + ")", shapeFix});
  }
  return usable;
}
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
  if (runtime::isLoopbackUrl(url)) curl_easy_setopt(http.get(), CURLOPT_NOPROXY, "*");
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
  if (result == CURLE_COULDNT_CONNECT || result == CURLE_COULDNT_RESOLVE_HOST ||
      result == CURLE_OPERATION_TIMEDOUT || result == CURLE_URL_MALFORMAT)
    throw std::runtime_error("SearXNG is not reachable at " + endpoint + " (" +
      curl_easy_strerror(result) + "). Web search is unavailable; run: batbox setup web, "
      "or set searxng_url to your own instance's /search endpoint");
  if (result != CURLE_OK) throw std::runtime_error(std::string("SearXNG request failed: ") + curl_easy_strerror(result));
  long status = 0;
  curl_easy_getinfo(http.get(), CURLINFO_RESPONSE_CODE, &status);
  char* type = nullptr;
  curl_easy_getinfo(http.get(), CURLINFO_CONTENT_TYPE, &type);
  const auto rows = searxngResults(endpoint, status, type ? type : "", payload);
  Json hits = Json::array();
  const auto& allowed = value::property(input, "allowed_domains");
  const auto& blocked = value::property(input, "blocked_domains");
  for (const auto& row : rows) {
    const auto resultUrl = row.at("url").get<std::string>();
    const auto hostname = host(resultUrl);
    if (allowed.is_array() && !allowed.empty() && !matchesAny(hostname, allowed)) continue;
    if (matchesAny(hostname, blocked)) continue;
    const auto& title = value::property(row, "title");
    hits.push_back({{"title", title.is_string() ? title.get<std::string>() : std::string()}, {"url", resultUrl}});
  }
  const double duration = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  return {{"query", query}, {"results", Json::array({{{"tool_use_id", gadgetUseId}, {"content", hits}}})}, {"durationSeconds", duration}};
}
} // namespace batbox
