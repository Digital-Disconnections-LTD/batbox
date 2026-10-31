#pragma once
#include "gadgets/live_roster/LiveRoster.hpp"
#include "skills/SkillRoster.hpp"
#include "messages/Messages.hpp"
#include "filesystem/RipgrepProcess.hpp"
#include "media/Media.hpp"
#include "gadgets/web/ModelAdapter.hpp"
#include "gadgets/bash/AsyncBashCall.hpp"

namespace batbox::application_gadgets {
class IndigenousFileReadStash;
// These inputs are the application-owned state and feature imports. The gadget
// implementations retain their own validation, permission and result logic.
struct Files {
  std::shared_ptr<file_state_stash::FileStateStash> stash;
  std::shared_ptr<IndigenousFileReadStash> editReadStash;
  std::shared_ptr<skills::SkillRoster> skillRoster;
  skills::SkillRosterServices skillServices;
  std::function<void(const std::function<void()>&)> runOnOwnerAndWait;
  gadgets::files::FileGadgetBindings presentation;
  std::function<gadgets::files::FileGadgetBindings(const std::string&)> makePresentation;
  std::function<bool(const std::string&)> readDenied, editDenied;
  std::function<std::vector<std::string>(const Json&)> readIgnorePatterns;
  std::function<std::vector<std::string>(const std::vector<std::string>&,
    const std::string&)> normalizeIgnorePatterns;
  std::function<std::vector<std::string>(const std::string&)> pluginGlobExclusions;
  std::function<Json(const std::string&,const std::string&,const std::string&)> patch;
  std::function<std::optional<double>(const std::string&)> countTokens;
  std::function<std::string(const std::string&)> notebookFormatOutput;
  std::function<bool(const std::string&)> isAutoMemoryFile;
  std::function<std::string(const std::string&)> findSimilarFile;
  std::function<std::optional<std::string>(const std::string&)> suggestUnderCwd;
  std::function<void(const std::string&)> diagnosticBeforeEdit;
  std::function<void(const std::string&,const std::optional<std::string>&,
    const std::string&)> notifyEditor;
  std::function<bool()> historyEnabled,lspAvailable,remoteDiffEnabled;
  std::function<void(const std::string&,const std::string&)> trackHistory;
  std::function<void(const std::string&)> clearDiagnostics,lspSave;
  std::function<void(const std::string&,const std::string&)> lspChange;
  std::function<std::optional<Json>(const std::string&)> remoteDiff;
  std::function<std::optional<std::string>(const std::string&,const std::string&)>
    checkTeamMemorySecrets;
  std::function<std::optional<gadgets::ValidationResult>(const std::string&,
    const std::string&,const std::function<std::string()>&)> validateSettingsEdit;
  std::function<int(const std::string&,const std::string&)> localeCompare;
  std::function<double()> nowMs;
  filesystem::RipgrepConfig ripgrep;
  media::Config media;
  gadgets::files::ReadLimits readLimits;
  bool simpleMode=false,dedupDisabled=false,compactLinePrefix=false,
    conciseEditHint=false,includeReadMitigation=false,noIgnore=false,hidden=false;
};

struct Bash {
  // The shell owner binds mission roster, sandbox, readonly classification,
  // permission and FTXUI callbacks here. Indigenous syntax/schema/hint/cwd leaves
  // are installed by this application provider.
  bash_gadget::Services services;
  bash_gadget::CwdPolicyServices cwdPolicy;
  event_loop::Loop* owner=nullptr;
  std::optional<bash_gadget::AsyncMissionHost> asyncMissionHost;
  std::string hintTagName;
  double maxTimeoutMs=600000;
};

struct Web {
  gadgets::web::SearchServices search;
  gadgets::web::FetchServices fetch;
  std::string searxngEndpoint="http://127.0.0.1:8888/search";
  std::vector<std::string> scraplingCommand={"scrapling","mcp"};
  std::shared_ptr<Searxng> searxng;
  std::shared_ptr<Scrapling> scrapling;
  gadgets::web::QueryServicesFactory makeQueryServices;
  std::function<std::string()> getSmallFastModel;
};

struct Inputs {
  std::function<std::string()> cwd,homeDirectory;
  std::shared_ptr<messages::Services> messages;
  Files files;
  Bash bash;
  Web web;
  gadgets::live_roster::BatFactory bat;
  std::function<gadgets::Gadget()> makeBatGadget;
  std::vector<std::shared_ptr<mcp::Connection>> mcpConnections;
  mcp::Services mcpServices;
  std::vector<gadgets::Gadget> additionalGadgets;
};

// Keep this object alive while any turn can call its gadgets. FileRead uses the
// shared message service by pointer, and Roster retains old gadget generations.
struct Prepared {
  gadgets::live_roster::FactoryInputs factory;
  std::shared_ptr<messages::Services> messages;
};
Prepared prepare(Inputs inputs);
struct Live {
  std::shared_ptr<gadgets::live_roster::Snapshot> snapshot;
  std::shared_ptr<messages::Services> messages;
};
Live makeLive(Inputs inputs);
} // namespace batbox::application_gadgets
