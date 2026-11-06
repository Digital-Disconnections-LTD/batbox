#pragma once
#include "application/Config.hpp"
#include "context/AsyncContext.hpp"
#include "git/CanonicalRoot.hpp"
#include "git/GitBasics.hpp"
#include "git_filesystem/GitFilesystem.hpp"
#include "git_filesystem/RootFinder.hpp"
#include "instructions/MemoryPath.hpp"
#include "instructions/project/async/AsyncDiscovery.hpp"
#include "process/ExecFileNoThrow.hpp"
#include "poetryEntries/AsyncSystemPoetry.hpp"
#include "poetryEntries/Guidance.hpp"
#include "poetryEntries/SystemPoetryFile.hpp"

namespace batbox::application_poetry {
struct Inputs {
  application::Config config;
  Json environment=Json::object();
  std::function<Json()> initialSettings,skillCommands,outputStyle;
  std::function<void(std::exception_ptr)> logError;
  std::function<void()> clearBetaHeaderLatches;
  std::function<void(bool)> recoveryActivity;
  std::function<void(const std::string&,const std::string&,
    const std::optional<Json>&)> logDiagnostic;
  std::function<void(const instructions::project::MemoryFileInfo&,
    const std::string&)> instructionsLoadedHook;
  std::vector<std::string> additionalDirectories;
  bool testEnvironment=false;
};

// Owns independent section, instruction, Git and context stashes.
// All public calls run on the supplied event-loop owner.
class Provider final:public std::enable_shared_from_this<Provider> {
public:
  Provider(event_loop::Loop&,Inputs);
  event_loop::Deferred<std::vector<std::string>> systemPoetry(
    const Json& gadgets,const std::string& model,const Json& mcpClients);
  event_loop::Deferred<Json> systemContext();
  event_loop::Deferred<Json> userContext();
  event_loop::Deferred<std::string> projectRecovery();
  event_loop::Deferred<context::MemoryFiles> memoryFiles();
  instructions::MemoryPathServices memoryPathServices();
  void refreshMemoryFilesIfChanged();
  // stablePromptPrefix (on unless settings say false): the head message and
  // system poetry keep their bytes for the Flight and are rebuilt only where
  // the server cache is lost anyway: compaction, /clear, a model switch, or a
  // new Provider (resume, workspace switch).
  bool stablePromptPrefix()const;
  // The main Flight's model for the next request; a different one rebuilds.
  void noteMainModel(const std::string& model);
  // A Write or Edit by the main Flight changed path; that version is already
  // in the conversation and needs no tail attachment.
  void noteOwnWrite(const std::string& path);
  // nested_memory attachments for files behind the head that changed since
  // it was built, each version once. coveredElsewhere(path) true skips a
  // file whose changes already reach the model as changed-file attachments.
  Json takeHeadChangeAttachments(
    const std::function<bool(const std::string&)>& coveredElsewhere={});
  // The static head for the next request: system.md when usable, else built-in.
  const poetryEntries::SystemPoetrySource& systemPoetrySource();
  Json enhanceBatPoetry(const Json& poetry,const std::string& model,
    const Json& additionalDirectories);
  void clearPoetrySections();
  void clearProjectInstructions();
  void resetProjectInstructionsAfterCompact();
  void clearGitStatus();
  context::AsyncContextStash& contextStash(){return contextStash_;}
  const std::optional<std::string>& stashedBatboxMdContent()const{
    return stashedBatboxMdContent_;}
private:
  void bindDiscovery();
  void bindGit();
  void bindContext();
  poetryEntries::GuidanceServices guidance()const;
  poetryEntries::AsyncSystemPoetryServices poetryServices();
  event_loop::Deferred<std::string> environmentInfo(const std::string& model,
    const std::optional<std::vector<std::string>>& dirs);
  Json settings()const;
  std::string cwd()const;
  struct HeadFile {std::string stamp;std::optional<std::string> content;};
  std::string headKey(const std::string& path)const;
  void trackHeadFile(const std::string& path);
  void resetHeadSnapshot();
  event_loop::Loop& owner_;
  Inputs inputs_;
  poetryEntries::AsyncSectionStash poetrySections_;
  context::AsyncContextStash contextStash_;
  instructions::project::async::MemoryDiscoveryStash discoveryStash_;
  instructions::MemoryPathStash memoryPathStash_;
  instructions::MemoryPathServices memoryPaths_;
  instructions::project::DiscoveryServices discovery_;
  instructions::project::async::Services discoveryAsync_;
  context::AsyncServices contextServices_;
  std::shared_ptr<git_filesystem::RootFinder> rootFinder_;
  std::unique_ptr<git::CanonicalRoot> canonicalRoot_;
  std::unique_ptr<git_filesystem::GitFilesystem> gitFilesystem_;
  std::unique_ptr<git::GitBasics> gitBasics_;
  process::ExecImports execImports_;
  std::optional<std::string> stashedBatboxMdContent_;
  poetryEntries::SystemPoetryFile systemPoetryFile_;
  std::map<std::string,std::string> memoryFileStamps_;
  std::vector<std::string> observedInstructionFiles_;
  std::optional<event_loop::Deferred<std::string>> projectRecoveryStash_;
  std::string projectRecoveryStamp_;
  std::size_t activeRecoveryLoads_=0;
  std::map<std::string,HeadFile> headFiles_;
  std::map<std::string,std::string> ownWrites_;
  std::string headModel_;
  bool systemHeadFrozen_=false;
};
} // namespace batbox::application_poetry
