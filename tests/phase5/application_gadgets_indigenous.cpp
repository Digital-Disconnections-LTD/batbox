#include <cstdlib>
#include "application_gadgets/ApplicationGadgets.hpp"
#include "application_gadgets/IndigenousFileHelpers.hpp"
#include "gadgets/live_roster/ContextBinding.hpp"
#include "mcp/ProcessResult.hpp"
#include "mcp/StdioClient.hpp"
#include "event_loop/Loop.hpp"
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using namespace batbox;
namespace {
template<class T>std::future<T> ready(T value){
  std::promise<T> promise;promise.set_value(std::move(value));return promise.get_future();
}
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
Json view(const Json&,const Json&){return Json::object();}
}
int main(int argc,char** argv){
  check(argc==3,"MCP and Scrapling fixtures required");
  char pattern[]="/tmp/batbox-application-tools-XXXXXX";
  check(::mkdtemp(pattern),"temporary directory failed");
  const auto root=std::filesystem::canonical(pattern);
  try{
    std::ofstream(root/"note.txt")<<"real local file\n";
    event_loop::Loop loop;
    auto client=std::make_shared<mcp::StdioClient>(
      std::vector<std::string>{"python3",argv[1]},"fixture",std::chrono::seconds(3));
    auto connection=mcp::makeStdioConnection(client,"fixture");
    connection->postToOwner=[&](std::function<void()> work){loop.postMission(std::move(work));};
    application_gadgets::Inputs in;
    in.cwd=[root]{return root.string();};
    in.homeDirectory=[root]{return root.string();};
    in.messages=std::make_shared<messages::Services>();
    in.messages->randomUUID=[](){return std::string("fixture-id");};
    in.messages->nowISO=[](){return std::string("2026-10-29T00:00:00Z");};
    auto& f=in.files;
    f.stash=file_state_stash::createFileStateStashWithSizeLimit(100);
    f.skillRoster=std::make_shared<skills::SkillRoster>();
    f.runOnOwnerAndWait=[](const std::function<void()>& work){work();};
    f.readDenied=f.editDenied=[](const std::string&){return false;};
    f.readIgnorePatterns=[](const Json&){return std::vector<std::string>{};};
    f.normalizeIgnorePatterns=[](const std::vector<std::string>& patterns,
      const std::string&){return patterns;};
    f.pluginGlobExclusions=[](const std::string&){return std::vector<std::string>{};};
    f.patch=[](const std::string&,const std::string&,const std::string&){return Json::array();};
    f.countTokens=[](const std::string&)->std::optional<double>{
      throw std::runtime_error("model token counter should not be needed for this file");};
    f.notebookFormatOutput=[](const std::string& text){return text;};
    f.isAutoMemoryFile=[](const std::string&){return false;};
    f.validateSettingsEdit=[](const std::string&,const std::string&,
        const std::function<std::string()>&)->std::optional<gadgets::ValidationResult>{return {};};
    f.findSimilarFile=[](const std::string&){return std::string();};
    f.suggestUnderCwd=[](const std::string&)->std::optional<std::string>{return {};};
    f.diagnosticBeforeEdit=[](const std::string&){};
    f.notifyEditor=[](const std::string&,const std::optional<std::string>&,
      const std::string&){};
    f.historyEnabled=f.lspAvailable=f.remoteDiffEnabled=[](){return false;};
    f.checkTeamMemorySecrets=[](const std::string&,const std::string&)
      ->std::optional<std::string>{return {};};
    f.localeCompare=[](const std::string& a,const std::string& b){return a.compare(b);};
    f.nowMs=[](){return 1000.0;};
    f.media.getGadgetResultsDir=[root]{return (root/"results").string();};
    f.media.randomUUID=[](){return std::string("media-id");};
    f.media.isPDFSupported=[](){return false;};
    f.simpleMode=true;
    auto allow=[](const Json& input,const Json&){return ready(Json{{"behavior","allow"},
      {"updatedInput",input}});};
    f.presentation.checkPermissions=allow;
    f.presentation.matchWildcardPattern=[](const std::string& pattern,
      const std::string& value){return pattern==value;};
    f.presentation.userFacingName=[](const Json&){return std::string("File");};
    f.presentation.getGadgetUseSummary=[](const Json& value){return value.value("file_path",Json());};
    f.presentation.renderUse=view;
    f.presentation.renderResult=[](const Json&,const Json&,const Json&){return Json::object();};
    f.presentation.renderError=f.presentation.renderRejected=view;

    shell::FlightConfig shellConfig;shellConfig.shellPath="/bin/bash";
    shellConfig.originalCwd=root.string();shellConfig.outputDir=(root/"jobs").string();
    shellConfig.generateMissionId=[](){return std::string("shell-task");};
    shellConfig.splitCommand=[](const std::string&){return std::vector<std::string>{"printf"};};
    auto& bash=in.bash.services;
    bash.shell=std::make_shared<shell::ManagedShell>(shellConfig);
    bash.poetry=[](const gadgets::PoetryContext&){return ready(std::string("Run shell commands"));};
    bash.checkPermissions=allow;
    bash.readOnlyAllowed=[](const Json&,bool){return false;};
    bash.commandHasAnyCd=[](const std::string&){return false;};
    bash.preparePermissionMatcher=[](const Json&){return ready(std::function<bool(const std::string&)>(
      [](const std::string&){return true;}));};
    bash.shouldUseSandbox=[](const Json&){return false;};
    bash.sandboxCommand=[](const std::string& command,const Json&,const Json&){return command;};
    bash.annotateSandboxFailures=[](const std::string&,const std::string& text){return text;};
    bash.logDebug=[](const std::string&){};
    bash.recordHint=[](const Json&){};
    bash.resizeImage=[](const std::string&,const shell::ExecResult&)
      ->std::optional<std::string>{return {};};
    bash.applySedEdit=[](const Json&,const gadgets::GadgetCallContext&,const Json&)
      ->gadgets::GadgetResult{throw std::runtime_error("sed edit import not used by fixture");};
    bash.parseSedEdit=[](const std::string&){return Json();};
    bash.fileEditUserFacingName=[](const Json&){return std::string("Edit");};
    bash.truncateSummary=[](const std::string& text,std::size_t max){return text.substr(0,max);};
    bash.getGadgetResultPath=[root](const std::string& id){return (root/"results"/id).string();};
    bash.registerBackgroundMission=[](const std::string&,const std::string&,
      const std::string&,const Json&){throw std::runtime_error("unexpected background task");};
    bash.renderUse=view;bash.renderResult=[](const Json&,const Json&,const Json&){return Json::object();};
    bash.renderProgress=bash.renderError=view;
    bash.renderQueued=[](){return Json::object();};
    in.bash.hintTagName="batbox-hint";
    in.bash.cwdPolicy.shouldMaintainProjectWorkingDir=[](){return false;};
    in.bash.cwdPolicy.pathInAllowedWorkingPath=[](const std::string&,const Json&){return true;};

    auto webUi=[](gadgets::web::UI& ui){
      ui.getGadgetUseSummary=[](const Json&){return Json("query");};
      ui.renderUse=ui.renderProgress=view;
      ui.renderResult=[](const Json&,const Json&,const Json&){return Json::object();};};
    webUi(in.web.search.ui);webUi(in.web.fetch.ui);
    in.web.search.localMonthYear=[](){return std::string("October 2026");};
    in.web.fetch.permissionRule=[](const Json&,const std::string&,const std::string&)
      ->std::optional<Json>{return {};};
    in.web.fetch.skipPreflight=[](){return true;};
    in.web.fetch.querySmallModel=[](const Json&,std::stop_token)->Json{
      throw std::runtime_error("small model import not used by fixture");};
    in.web.scraplingCommand={"python3",argv[2]};
    in.bat.descriptor.renderGadgetUseMessage=view;
    in.bat.descriptor.renderGadgetResultMessage=[](const Json&,const Json&,const Json&){return Json::object();};
    in.bat.makeServices=[](){return bats::BatGadgetCallServices{};};
    in.mcpConnections={connection};
    in.mcpServices.wire.searchHintMetaKey="test/searchHint";
    in.mcpServices.wire.alwaysLoadMetaKey="test/alwaysLoad";
    in.mcpServices.nowMs=[](){return std::int64_t{1000};};
    in.mcpServices.isOutputLineTruncated=[](const std::string&){return false;};
    in.mcpServices.renderGadgetUseMessage=view;
    in.mcpServices.renderGadgetUseProgressMessage=view;
    in.mcpServices.renderGadgetResultMessage=[](const Json&,const Json&,const Json&){return Json::object();};
    mcp::ProcessResultServices process;
    process.result.nowMs=[](){return std::int64_t{1000};};
    process.result.randomSuffix=[](){return std::string("fixture");};
    process.validation.configuredMaxOutputTokens=25000;
    mcp::bindIndigenousProcessResult(in.mcpServices,process);

    auto live=application_gadgets::makeLive(std::move(in));
    check(live.snapshot->base.size()==9&&live.snapshot->mcp.size()==3,
      "real core and MCP tools missing");
    auto roster=std::make_shared<gadgets::live_roster::Roster>();
    roster->publish(live.snapshot);
    interactive::context::ContextBindings contextBindings;
    contextBindings.catalogSettings.getDenyRuleForGadget=[](const gadgets::Gadget&,
      const Json&){return false;};
    contextBindings.catalogSettings.localeCompare=[](const std::string& a,
      const std::string& b){return a.compare(b);};
    gadgets::live_roster::bindContextGadgets(contextBindings,roster);
    const auto selected=contextBindings.assembleGadgets(Json::object(),
      contextBindings.getMcpGadgets({}));
    check(selected.size()==12&&selected.front()==live.snapshot->find("Agent"),
      "live context tool selection failed");
    auto context=gadgets::GadgetCallContext{};
    context.data=Json::object();
    auto read=live.snapshot->find("Read")->call(Json{{"file_path","note.txt"}},
      context,{},Json(),{}).get();
    check(read.data["type"]=="text"&&
      read.data["file"]["content"]=="real local file\n","Read failed");
    auto bashResult=live.snapshot->find("Bash")->call(
      Json{{"command","printf 'real shell\\n'"}},context,{},Json(),{}).get();
    check(bashResult.data["stdout"].get<std::string>().find("real shell")!=std::string::npos,
      "Bash failed");
    const Json parent={{"message",{{"content",Json::array(
      {{{"type","tool_use"},{"id","app-flow"}}})}}}};
    auto echo=live.snapshot->find("mcp__fixture__echo")->call(
      Json{{"text","connected"}},context,{},parent,{}).get();
    loop.runReady();
    check(echo.data=="{\"echo\":\"connected\"}","MCP failed");
    std::ofstream(root/"note.md")<<"same stem";
    check(application_gadgets::findSimilarFile((root/"note.txt").string())=="note.md",
      "similar-file lookup failed");
    check(application_gadgets::suggestPathUnderCwd(
      (root.parent_path()/"note.txt").string(),root.string())==
      (root/"note.txt").string(),"cwd path suggestion failed");
    application_gadgets::IndigenousFileReadStash editStash;
    check(editStash.readFile((root/"note.txt").string()).content=="real local file\n"&&
      editStash.getStats().size==1,"cached edit read failed");
    std::filesystem::remove(root/"note.txt");
    bool missing=false;
    try{(void)editStash.readFile((root/"note.txt").string());}
    catch(const std::system_error&){missing=true;}
    check(missing&&editStash.size()==0,"deleted file retained by edit cache");
    std::filesystem::remove_all(root);
    std::cout<<"application tools: Read, Bash, MCP and web connector construction passed\n";
  }catch(...){std::filesystem::remove_all(root);throw;}
}
