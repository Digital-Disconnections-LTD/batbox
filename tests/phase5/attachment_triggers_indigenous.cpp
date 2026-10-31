#include "attachment_triggers/Attachments.hpp"
#include "glob/DiscoveryMatcherBinding.hpp"
#include "gadgets/files/ActiveStash.hpp"
#include "gadgets/files/ActiveTriggers.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unistd.h>

using namespace batbox;
namespace fs=std::filesystem;
namespace project=batbox::instructions::project;
namespace async=batbox::instructions::project::async;
namespace {
void write(const fs::path& path,const std::string& value){
  fs::create_directories(path.parent_path());
  std::ofstream out(path);out<<value;
}
bool inside(const std::string& path,const std::string& base){
  const auto relative=fs::path(path).lexically_relative(base).generic_string();
  return relative.empty()||relative=="."||
    (relative!=".."&&!relative.starts_with("../")&&
     !fs::path(relative).is_absolute());
}
Json await(event_loop::Loop& loop,event_loop::Deferred<Json> pending){
  Json result=nullptr;std::exception_ptr failure;
  pending.then([&](const Json* value,const std::exception_ptr& error){
    failure=error;if(value)result=*value;
  });
  for(int i=0;i<3000&&result.is_null()&&!failure;++i){
    loop.runReady();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if(failure)std::rethrow_exception(failure);
  assert(!result.is_null());return result;
}
}
int main(){
  const auto temp=fs::canonical(fs::temp_directory_path())/
    ("batbox-attachment-triggers-"+std::to_string(getpid()));
  const auto base=temp/"project",sub=base/"sub",target=sub/"file.cpp";
  try{
    write(target,"int main() {}\n");
    write(sub/"BATBOX.md","# Nested instruction\n");
    write(sub/".batbox/rules/always.md","# Unconditional rule\n");
    write(base/".batbox/rules/condition.md",
      "---\npaths:\n  - sub/**\n---\n# Conditional rule\n");
    event_loop::Loop loop;
    project::DiscoveryServices sync;
    sync.parse.paths.getCwd=[&]{return base.string();};
    sync.parse.paths.homeDirectory=[&]{return temp.string();};
    sync.parse.paths.platform=[](){return std::string("linux");};
    sync.parse.paths.normalizeNFC=[](const std::string& value){return value;};
    sync.parse.logDebug=[](const std::string&){};
    sync.excludePatterns=[](){return std::vector<std::string>{};};
    sync.pathInWorkingPath=inside;
    sync.originalCwd=[&]{return base.string();};
    sync.managedRulesDir=[&]{return (temp/"managed/rules").string();};
    sync.userRulesDir=[&]{return (temp/"user/rules").string();};
    sync.settingSourceEnabled=[](const std::string& name){
      return name=="projectSettings";
    };
    sync.externalIncludesApproved=[](){return false;};
    glob::bindDiscoveryMatchers(sync);
    attachment_triggers::Services services{loop};
    services.cwd=base.string();
    services.memory=async::indigenousAsyncFs(loop,sync);
    int appReads=0,errors=0;
    services.getAppState=[&]{++appReads;return std::make_shared<app_state::AppState>();};
    services.pathInAllowedWorkingPath=[&](const std::string& path,
      const app_state::AppStateRef& state){assert(state);return inside(path,base.string());};
    services.skipProjectLevel=[](){return false;};
    services.logError=[&](std::exception_ptr){++errors;};
    services.nowMillis=[](){return 1700000000000.0;};
    services.hooks.getHooksConfigFromSnapshot=[]()->std::optional<Json>{
      return Json{{"InstructionsLoaded",Json::array({Json::object()})}};};
    services.hooks.getRegisteredHooks=[]()->std::optional<Json>{return std::nullopt;};
    services.hooks.createBaseHookInput=[](){return Json::object();};
    services.hooks.defaultTimeoutMs=1000;
    std::vector<std::string> hooks;
    services.hooks.executeHooksOutsideRepl=[&](
      instructions::hooks::HookDispatch dispatch){
      hooks.push_back(dispatch.hookInput["file_path"].get<std::string>()+":"+
        dispatch.hookInput["load_reason"].get<std::string>());
      assert(dispatch.hookInput["trigger_file_path"]==target.string());
      event_loop::Deferred<Json> done(loop);done.resolve(Json::object());return done;
    };
    execution::Context context;
    context.readFileState=file_state_stash::createFileStateStashWithSizeLimit(100);
    context.nestedMemoryAttachmentTriggers=
      std::make_shared<runtime::OrderedStringSet>();
    context.loadedNestedMemoryPaths=
      std::make_shared<runtime::OrderedStringSet>();
    assert(await(loop,attachment_triggers::getNestedMemoryAttachments(context,
      services)).empty());
    assert(appReads==0);
    gadgets::GadgetCallContext call;
    call.readFileState=context.readFileState;
    call.nestedMemoryAttachmentTriggers=context.nestedMemoryAttachmentTriggers;
    call.dynamicSkillDirTriggers=context.dynamicSkillDirTriggers=
      std::make_shared<runtime::OrderedStringSet>();
    gadgets::files::FileReadServices readServices;
    auto activeRead=gadgets::files::withActiveStash(readServices,call);
    assert(activeRead.activeNestedMemoryAttachmentTriggers==
      context.nestedMemoryAttachmentTriggers);
    gadgets::files::recordNestedMemoryTrigger(activeRead,target.string());
    assert(context.nestedMemoryAttachmentTriggers->contains(target.string()));
    auto first=await(loop,attachment_triggers::getNestedMemoryAttachments(
      context,services));
    assert(appReads==1&&errors==0);
    assert(context.nestedMemoryAttachmentTriggers->empty());
    const auto skillDir=base/"skills";
    write(skillDir/"sample/SKILL.md","name: sample\n");
    gadgets::files::FileWriteServices writeServices;
    auto activeWrite=gadgets::files::withActiveStash(writeServices,call);
    assert(activeWrite.activeDynamicSkillDirTriggers==
      context.dynamicSkillDirTriggers);
    gadgets::files::recordDynamicSkillDir(activeWrite,skillDir.string());
    const auto skillAttachments=await(loop,
      attachment_triggers::getDynamicSkillAttachments(context,services));
    assert(skillAttachments.size()==1);
    assert(skillAttachments[0]["skillNames"]==Json::array({"sample"}));
    assert(context.dynamicSkillDirTriggers->empty());
    assert(first.size()==3);
    assert(first[0]["path"]==(sub/"BATBOX.md").string());
    assert(first[1]["path"]==(sub/".batbox/rules/always.md").string());
    assert(first[2]["path"]==(base/".batbox/rules/condition.md").string());
    assert(hooks.size()==3);
    assert(context.loadedNestedMemoryPaths->size()==3);
    assert(context.readFileState->size()==3);
    const auto conditional=context.readFileState->get(first[2]["path"]);
    assert(conditional&&conditional->isPartialView&&conditional->partialViewPresent);
    context.nestedMemoryAttachmentTriggers->insert(target.string());
    assert(await(loop,attachment_triggers::getNestedMemoryAttachments(context,
      services)).empty());
    assert(appReads==2&&hooks.size()==3);
    context.readFileState->clear();
    context.nestedMemoryAttachmentTriggers->insert(target.string());
    assert(await(loop,attachment_triggers::getNestedMemoryAttachments(context,
      services)).empty());
    assert(context.loadedNestedMemoryPaths->size()==3);
    context.loadedNestedMemoryPaths->clear();
    services.skipProjectLevel=[](){return true;};
    context.nestedMemoryAttachmentTriggers->insert(target.string());
    assert(await(loop,attachment_triggers::getNestedMemoryAttachments(context,
      services)).empty());
    assert(context.loadedNestedMemoryPaths->empty());
    services.skipProjectLevel=[](){return false;};
    services.pathInAllowedWorkingPath=[](const std::string&,
      const app_state::AppStateRef&){return false;};
    context.nestedMemoryAttachmentTriggers->insert(target.string());
    assert(await(loop,attachment_triggers::getNestedMemoryAttachments(context,
      services)).empty());
    assert(context.nestedMemoryAttachmentTriggers->empty());
    fs::remove_all(temp);
  }catch(...){fs::remove_all(temp);throw;}
}
