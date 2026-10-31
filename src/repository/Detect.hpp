#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>

namespace batbox::repository {
struct ParsedRepository {
  std::string host,owner,name;
  Json toJson()const;
};
using RepositoryRef=std::shared_ptr<ParsedRepository>;
using Debug=std::function<void(const std::string&)>;
bool looksLikeRealHostname(const std::string& host);
RepositoryRef parseGitRemote(const std::string& input);
std::optional<std::string> parseGitHubRepository(const std::string& input,const Debug& debug={});
struct Services {
  std::function<std::string()> getCwd;
  std::function<event_loop::Deferred<std::optional<std::string>>()> getRemoteUrl;
  Debug debug;
};
class Detector {
  struct State;
  std::shared_ptr<State> state_;
public:
  Detector(event_loop::Loop&,Services);
  void clearRepositoryStashes();
  event_loop::Deferred<RepositoryRef> detectCurrentRepositoryWithHost();
  event_loop::Deferred<std::optional<std::string>> detectCurrentRepository();
  std::optional<std::string> getStashedRepository()const;
};
using ParseGitRemote=std::function<RepositoryRef(const std::string&)>;
using ParserImport=std::function<event_loop::Deferred<ParseGitRemote>()>;
event_loop::Deferred<std::optional<std::string>> getGithubRepo(event_loop::Loop&,
  const ParserImport&,const std::function<event_loop::Deferred<std::optional<std::string>>()>& getRemoteUrl,
  const Debug& debug={});
}
