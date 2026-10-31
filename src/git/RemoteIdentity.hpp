#pragma once
#include "event_loop/Deferred.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::git {
bool isLocalHost(const std::string& host);
std::optional<std::string> normalizeGitRemoteUrl(const std::string& url);
event_loop::Deferred<std::optional<std::string>> getRepoRemoteHash(
  event_loop::Loop& loop,
  const std::function<event_loop::Deferred<std::optional<std::string>>()>& getRemoteUrl);
} // namespace batbox::git
