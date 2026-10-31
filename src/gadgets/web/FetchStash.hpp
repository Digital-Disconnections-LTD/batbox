#pragma once
#include "gadgets/web/Web.hpp"
#include <chrono>
#include <list>
#include <mutex>
#include <optional>

namespace batbox::gadgets::web {
struct FetchStash {
  struct Entry {std::string url;FetchContent value;std::size_t size;std::chrono::steady_clock::time_point expiry;};
  struct Domain {std::string hostname;std::chrono::steady_clock::time_point expiry;};
  std::mutex mutex;
  std::list<Entry> urls;
  std::list<Domain> allowedDomains;
  std::size_t totalBytes=0;
};
std::optional<FetchContent> stashedFetch(const FetchServices& services,const std::string& url);
void stashFetch(const FetchServices& services,const std::string& url,const FetchContent& value);
bool stashedAllowedDomain(const FetchServices& services,const std::string& hostname);
void stashAllowedDomain(const FetchServices& services,const std::string& hostname);
} // namespace batbox::gadgets::web
