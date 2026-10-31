#include "application_memory/Provider.hpp"
#include "application_memory/Consolidation.hpp"
#include "query/MemoryAttachmentFilter.hpp"
#include "memory_prefetch/PoetryScan.hpp"
#include "runtime/Platform.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace batbox;
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr error;
  pending.then([&](const T* result,const std::exception_ptr& failure){
    error=failure;if(result)value=*result;
  });
  loop.run();if(error)std::rethrow_exception(error);
  if(!value)throw std::runtime_error("Memory workflow Promise did not settle");
  return std::move(*value);
}
int main(int argc,char** argv){
  if(argc==5&&std::string(argv[1])=="--prompt"){
    std::cout<<application_memory::buildConsolidationPoetry(argv[2],argv[3],argv[4]);
    return 0;
  }
  event_loop::Loop loop;
  const auto root=std::filesystem::temp_directory_path()/
    ("batbox-memory-workflow-"+platform::randomUUID());
  std::filesystem::create_directories(root/"project");
  application::Config config;
  config.cwd=root/"project";config.stateDirectory=root/"state";
  config.configDirectory=root/"config";config.configPath=config.configDirectory/"config.json";
  config.model="fixture";
  Json settings={{"autoMemoryEnabled",true},{"memoryRecallEnabled",true},
    {"autoDreamEnabled",false}};
  auto messages=std::make_shared<messages::Services>();
  messages->randomUUID=platform::randomUUID;messages->nowISO=[]{return platform::isoTimestamp();};
  messages->strictGadgetResultPairing=[]{return true;};
  messages->featureGate=[](const std::string&){return false;};
  messages->logError=[](const std::string& value){throw std::runtime_error(value);};
  application_model::Config modelConfig;modelConfig.mainModel=modelConfig.smallModel="fixture";
  modelConfig.client.baseUrl="http://127.0.0.1:1";
  application_model::ModelProfile profile;profile.id="fixture";
  profile.contextTokens=100000;profile.maxOutputTokens=4000;
  profile.costs={{"inputTokens",0},{"outputTokens",0},{"promptCacheReadTokens",0},
    {"promptCacheWriteTokens",0},{"webSearchRequests",0}};
  modelConfig.profiles.emplace("fixture",profile);
  application_model::Imports imports;
  imports.authToken=[]{return std::string{};};imports.invalidateAuthToken=[]{};
  imports.debug=[](const std::string&,const std::string&){};imports.diagnostic=imports.debug;
  imports.normalizeAttachmentForAPI=[](const Json& value){return value;};
  imports.normalizeGadgetInputForAPI=[](const gadgets::Gadget&,const Json& value){return value;};
  imports.gadgetSchemaSync=[](const gadgets::Gadget&,const gadgets::GadgetSchemaOptions&){return Json::object();};
  imports.gadgetSchemaAsync=[&](const gadgets::Gadget&,const gadgets::GadgetSchemaOptions&){
    event_loop::Deferred<Json> result(loop);result.resolve(Json::object());return result;};
  auto model=std::make_shared<application_model::LocalModelService>(loop,modelConfig,imports);
  application_flights::Inputs flightInputs(loop);flightInputs.config=config;
  flightInputs.flightId=platform::randomUUID();
  flightInputs.commandNames=[]{return std::set<std::string>{};};
  flightInputs.logError=[](std::exception_ptr error){if(error)std::rethrow_exception(error);};
  auto flight=application_flights::Flight::create(std::move(flightInputs));
  application_poetry::Inputs poetryInputs;poetryInputs.config=config;
  poetryInputs.initialSettings=[&]{return settings;};
  auto poetry=std::make_shared<application_poetry::Provider>(loop,std::move(poetryInputs));
  const auto make=[&]{
    application_memory::Inputs input(loop);input.config=config;
    input.model=model;input.messages=messages;input.poetry=poetry;input.flight=flight;
    input.getAppState=[&]{return Json{{"settings",settings}};};
    return application_memory::Provider::create(std::move(input));
  };
  auto memory=make();
  const auto directory=memory->autoMemoryDirectory();
  const auto topic=directory+"feedback_native.md";
  await(loop,memory->write(topic,"---\nname: Native workflow\ndescription: Native build preference\ntype: feedback\n---\nUse the native workflow, saved on disk.\n"));
  await(loop,memory->write(directory+"MEMORY.md","- [Native workflow](feedback_native.md) — saved build preference\n"));
  auto listed=await(loop,memory->listSources());bool found=false;
  for(const auto& row:listed)if(row.at("path")==topic)found=true;
  assert(found);
  auto context=await(loop,poetry->userContext());
  assert(context.dump().find("saved build preference")!=std::string::npos);
  // Direct gadget/editor/Bash writes do not have to use Provider::write.
  {std::ofstream file(directory+"MEMORY.md");file<<"- Updated durable index after external write\n";}
  context=await(loop,poetry->userContext());
  assert(context.dump().find("Updated durable index after external write")!=std::string::npos);
  memory=make(); // Recreate the application owner; all recall comes from disk.
  auto prefetch=memory->makePrefetchServices();int selections=0;
  prefetch->sideQuery=[&](const Json& request,std::stop_token){
    ++selections;assert(request.at("querySource")=="memdir_relevance");
    assert(request.at("system").get<std::string>().find("selecting memories")!=std::string::npos);
    assert(request.at("messages").dump().find("Native build preference")!=std::string::npos);
    event_loop::Deferred<Json> result(loop);
    result.resolve(Json{{"content",Json::array({Json{{"type","text"},
      {"text","{\"selected_memories\":[\"feedback_native.md\",\"../../missing.md\"]}"}}})}});
    return result;
  };
  auto stash=file_state_stash::createFileStateStashWithSizeLimit(100);
  auto attachments=await(loop,memory_prefetch::getRelevantMemoryAttachments(
    "Recall the native build preference",Json::array(),stash,{}, {},{},*prefetch));
  assert(attachments.size()==1&&attachments[0]["memories"].size()==1);
  assert(attachments[0]["memories"][0]["path"]==topic);
  assert(attachments[0]["memories"][0]["header"].get<std::string>().find(topic)!=std::string::npos);
  auto unique=query::filterDuplicateMemoryAttachments(attachments,*stash);
  assert(unique.size()==1&&stash->has(topic));
  assert(query::filterDuplicateMemoryAttachments(attachments,*stash).empty());
  const Json prior={{"type","attachment"},{"attachment",attachments[0]}};
  const auto surfaced=memory_prefetch::collectSurfacedMemories(Json::array({prior}));
  assert(surfaced.paths.size()==1);
  // A compacted transcript no longer contains the old attachment; clearing
  // the read stash permits the durable memory to be surfaced again.
  stash->clear();
  auto recalled=await(loop,memory_prefetch::getRelevantMemoryAttachments(
    "Recall after compact",Json::array(),stash,{}, {},{},*prefetch));
  assert(recalled.size()==1&&selections==2);
  execution::Context turn;turn.readFileState=stash;
  turn.data["options"]["agentDefinitions"]["activeAgents"]=Json::array();
  Json user={{"type","user"},{"uuid",platform::randomUUID()},
    {"message",{{"content",Json::array({Json{{"type","text"},
      {"text","Recall the native preference"}}})}}}};
  auto handle=memory_prefetch::startRelevantMemoryPrefetch(Json::array({user}),turn,*prefetch);
  assert(memory->hasActiveJobs());loop.run();
  assert(handle.isSettled&&handle.isSettled());
  assert(!memory->hasActiveJobs()&&handle.read().size()==1);
  handle.dispose();
  settings["memoryRecallEnabled"]=false;memory->refresh();
  auto disabled=memory_prefetch::startRelevantMemoryPrefetch(Json::array({user}),turn,*prefetch);
  assert(!disabled.isSettled&&!memory->hasActiveJobs());
  settings["memoryRecallEnabled"]=true;memory->refresh();
  auto state=await(loop,memory->status());
  assert(state["enabled"]==true&&state["recallEnabled"]==true&&state["autoDreamEnabled"]==false);
  assert(state["lastConsolidatedAt"]==0);
  auto lock=application_memory::tryAcquireConsolidationLock(directory,platform::nowMillis());
  assert(lock&&*lock==0);
  assert(!application_memory::tryAcquireConsolidationLock(directory,platform::nowMillis()));
  application_memory::rollbackConsolidationLock(directory,*lock);
  assert(application_memory::readLastConsolidatedAt(directory)==0);
  await(loop,memory->shutdown());await(loop,flight->flush());
  std::filesystem::remove_all(root);
  std::cout<<"memory workflow: durable write, external invalidation, owner restart, recall, provenance, dedup, compact carryover, gates and lock passed\n";
}
