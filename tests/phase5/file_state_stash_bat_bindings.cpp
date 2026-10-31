#include "bats/ForkedBat.hpp"
#include "bats/RunBat.hpp"
#include "file_state_stash/ExecutionBindings.hpp"
#include "file_state_stash/Bindings.hpp"
#include "gadgets/files/FileGadgets.hpp"
#include "filesystem/FileOps.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <stdexcept>
#include <unistd.h>

using namespace batbox;
namespace {
gadgets::files::FileGadgetBindings fileBindings(){
  gadgets::files::FileGadgetBindings b;
  b.checkPermissions=[](const Json& input,const Json&){
    return std::async(std::launch::deferred,[input]{return Json{{"behavior","allow"},
      {"updatedInput",input}};});};
  b.matchWildcardPattern=[](const std::string& a,const std::string& b){return a==b;};
  b.userFacingName=[](const Json&){return std::string("file");};
  b.getGadgetUseSummary=[](const Json&){return Json();};
  b.renderUse=[](const Json&,const Json&){return Json();};
  b.renderResult=[](const Json&,const Json&,const Json&){return Json();};
  b.renderError=[](const Json&,const Json&){return Json();};
  b.renderRejected=[](const Json&,const Json&){return Json();};
  b.renderTag=[](const Json&){return Json();};
  b.isResultTruncated=[](const Json&){return false;};
  return b;
}
}
int main(){
  namespace fs=std::filesystem;
  const auto directory=fs::temp_directory_path()/
    ("batbox-agent-cache-"+std::to_string(getpid()));
  fs::create_directories(directory);
  const auto file=directory/"note.txt";
  {std::ofstream out(file);out<<"before";}
  auto parentStash=file_state_stash::createFileStateStashWithSizeLimit(2,32);
  const auto mtime=filesystem::getFileModificationTime(file.string());
  parentStash->set(file.string(),file_state_stash::FileState{"before",std::floor(mtime)});
  query_engine::ProcessContext process;
  process.readFileState=parentStash;
  bats::BatContext parent;
  file_state_stash::bindExecutionStash(parent.query,process);
  assert(parent.query.readFileState==parentStash);
  parent.getAppState=[](){return Json{{"toolPermissionContext",
    {{"shouldAvoidPermissionPrompts",true}}}};};
  bats::ContextServices contextServices;
  contextServices.createBatId=[](const auto&){return std::string("child");};
  contextServices.randomUUID=[](){return std::string("chain");};
  contextServices.createDenialTrackingState=[](){return Json::object();};
  auto child=bats::createSubagentContext(parent,{},contextServices);
  assert(child.query.readFileState!=parent.query.readFileState);
  assert(child.query.readFileState->get(file.string())==parentStash->get(file.string()));
  child.query.readFileState->set("extra",file_state_stash::FileState{"x",1});
  assert(!parentStash->has("extra"));

  bats::RunBatParams params;
  params.gadgetUseContext=parent;
  params.batDefinition={{"agentType","general-purpose"}};
  bats::RunBatPreflightServices preflight;
  preflight.getBatModel=[](const Json&,const Json&,
    const std::optional<std::string>&,const Json&){return std::string("standard");};
  preflight.createBatId=[](){return std::string("agent");};
  preflight.getUserContext=[](){return Json::object();};
  preflight.getSystemContext=[](){return Json::object();};
  preflight.feature=[](const std::string&){return false;};
  params.forkContextMessages=Json::array();
  auto cloned=bats::prepareRunBat(params,preflight);
  assert(cloned.indigenousBatReadFileState);
  assert(cloned.indigenousBatReadFileState!=parentStash);
  assert(cloned.indigenousBatReadFileState->get(file.string())==parentStash->get(file.string()));
  bats::SubagentOverrides subagentOverride;
  subagentOverride.indigenousReadFileState=cloned.indigenousBatReadFileState;
  auto preparedChild=bats::createSubagentContext(parent,subagentOverride,contextServices);
  assert(preparedChild.query.readFileState!=cloned.indigenousBatReadFileState);
  assert(preparedChild.query.readFileState->get(file.string())==parentStash->get(file.string()));
  params.forkContextMessages.reset();
  auto fresh=bats::prepareRunBat(params,preflight);
  assert(fresh.indigenousBatReadFileState&&fresh.indigenousBatReadFileState->size()==0);
  assert(fresh.indigenousBatReadFileState->max()==file_state_stash::READ_FILE_STATE_STASH_SIZE);

  gadgets::files::FileWriteServices write;
  auto baseStash=file_state_stash::createFileStateStashWithSizeLimit(2,32);
  file_state_stash::bindWriteStash(write,baseStash);
  write.paths.getCwd=[&]{return directory.string();};
  write.paths.homeDirectory=[&]{return directory.string();};
  write.paths.platform=[](){return std::string("linux");};
  write.paths.normalizeNFC=[](const std::string& path){return path;};
  write.checkTeamMemSecrets=[](const std::string&,const std::string&){
    return std::optional<std::string>{};};
  write.matchingEditDenyRule=[](const std::string&){return false;};
  write.discoverSkillDirsForPaths=[](const std::string&,const std::string&){
    return std::vector<std::string>{};};
  write.activateConditionalSkillsForPaths=[](const std::string&,const std::string&){};
  write.diagnosticBeforeFileEdited=[](const std::string&){};
  write.fileHistoryEnabled=[](){return false;};
  write.lspAvailable=[](){return false;};
  write.notifyEditorFileUpdated=[](const std::string&,
    const std::optional<std::string>&,const std::string&){};
  write.remoteDiffEnabled=[](){return false;};
  write.getPatchForDisplay=[](const std::string&,const std::string&,const std::string&){
    return Json::array();};
  auto gadget=gadgets::files::makeWriteGadget(std::move(write),fileBindings());
  const Json input={{"file_path",file.string()},{"content","after"}};
  gadgets::GadgetCallContext emptyContext;
  assert(!gadget.validateInputWithContext(input,emptyContext).get().result);
  gadgets::GadgetCallContext activeContext;
  activeContext.readFileState=child.query.readFileState;
  assert(gadget.validateInputWithContext(input,activeContext).get().result);
  const auto result=gadget.call(input,activeContext,{},Json{{"uuid","parent"}},{}).get();
  assert(result.data.at("type")=="update");
  assert(child.query.readFileState->get(file.string())->content=="after");
  assert(parentStash->get(file.string())->content=="before");
  assert(!baseStash->has(file.string()));

  // The dispatcher must validate against this turn's stash before hooks or
  // permissions, even when the gadget's factory has a different stash owner.
  gadgets::Gadget validationGadget;
  validationGadget.name="CacheCheck";
  validationGadget.parseInput=[](const Json& value){return value;};
  validationGadget.validateInput=[](const Json&,const Json&)
    ->std::future<gadgets::ValidationResult>{
    throw std::logic_error("JSON-only validation was called");};
  validationGadget.validateInputWithContext=[&](const Json&,
      const gadgets::GadgetCallContext& context){
    assert(context.readFileState==child.query.readFileState);
    return std::async(std::launch::deferred,[]{
      return gadgets::ValidationResult{false,"blocked by native cache",2};});
  };
  execution::Context dispatch;
  dispatch.readFileState=child.query.readFileState;
  dispatch.gadgets={&validationGadget};
  execution::Services executionServices;
  executionServices.createUserMessage=[](const Json& args){return args;};
  Json emitted=Json::array();
  execution::runGadgetUse(Json{{"id","tool-1"},{"name","CacheCheck"},
      {"input",Json::object()}},Json{{"uuid","assistant-1"}},
    dispatch,executionServices,[&](const execution::Update& update){
      if(update.message)emitted.push_back(*update.message);});
  assert(emitted.size()==1);
  assert(emitted[0]["toolUseResult"]=="Error: blocked by native cache");
  fs::remove(file);fs::remove(directory);
}
