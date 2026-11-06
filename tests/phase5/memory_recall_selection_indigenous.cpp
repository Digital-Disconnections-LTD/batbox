#include "application_memory/Provider.hpp"
#include "memory_prefetch/Prefetch.hpp"
#include "runtime/Platform.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace batbox;
namespace fs=std::filesystem;
namespace {
void require(bool condition,const std::string& message){
  if(!condition)throw std::runtime_error(message);
}
void write(const fs::path& path,const std::string& text){
  std::ofstream output(path,std::ios::binary|std::ios::trunc);output<<text;
  if(!output)throw std::runtime_error("Cannot write "+path.string());
}
Json settle(event_loop::Loop& loop,const query::MemoryPrefetch& handle){
  require(bool(handle.isSettled),"Recall did not start");
  for(int i=0;i<3000&&!handle.isSettled();++i){
    loop.runReady();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  loop.runReady();
  require(handle.isSettled(),"Recall did not settle");
  return handle.read();
}
execution::Context turnContext(const std::string& model,const std::optional<std::string>& bat={}){
  execution::Context result;
  result.readFileState=file_state_stash::createFileStateStashWithSizeLimit(100);
  result.data={{"options",{{"mainLoopModel",model},
    {"agentDefinitions",{{"activeAgents",Json::array()}}}}}};
  if(bat)result.data["agentId"]=*bat;
  return result;
}
Json turn(const std::string& text){
  return Json::array({Json{{"type","user"},{"message",{{"content",text}}}}});
}
std::vector<std::string> paths(const Json& result){
  std::vector<std::string> found;
  for(const auto& attachment:result)
    for(const auto& memory:attachment.at("memories"))
      found.push_back(fs::path(memory.at("path").get<std::string>()).filename().string());
  return found;
}
instructions::MemoryHeader header(const std::string& name,const std::string& description){
  return instructions::MemoryHeader{name,"/memory/"+name,0,Json(description),std::nullopt};
}
}
int main(){
  require(application_memory::recallByKeywords(Json::object(),true)&&
    !application_memory::recallByKeywords(Json::object(),false)&&
    !application_memory::recallByKeywords(Json{{"memoryRecallSelector","model"}},true)&&
    application_memory::recallByKeywords(Json{{"memoryRecallSelector","keywords"}},false)&&
    application_memory::recallByKeywords(Json{{"memoryRecallSelector","auto"}},true),
    "memoryRecallSelector did not resolve as documented");

  const std::vector<instructions::MemoryHeader> manifest{
    header("deploy-notes.md","How we ship releases to the staging cluster"),
    header("lunch.md","Favourite food near the office"),
    header("staging-db.md","Credentials rotation for the database"),
    header("cluster.md","Notes")};
  const auto picked=memory_prefetch::selectMemoriesByKeywords(
    "How do I deploy the release to staging?",manifest);
  require(picked==std::vector<std::string>({"deploy-notes.md","staging-db.md"}),
    "Keyword selection picked the wrong memories");
  require(memory_prefetch::selectMemoriesByKeywords("tell me about releases",manifest)
    .empty(),"A single shared description word selected a memory");
  require(memory_prefetch::selectMemoriesByKeywords("deploying clusters",manifest)==
    std::vector<std::string>({"deploy-notes.md","cluster.md"}),"Word endings were not matched");
  std::vector<instructions::MemoryHeader> many;
  for(int i=0;i<8;++i)many.push_back(header("build-"+std::to_string(i)+".md","build"));
  require(memory_prefetch::selectMemoriesByKeywords("build it",many).size()==5,
    "Keyword selection returned more than five memories");
  const auto bounded=memory_prefetch::boundedQuery(std::string(3000,'x')+"é",2000);
  require(bounded.starts_with(std::string(2000,'x'))&&bounded.find("more bytes not shown")!=
    std::string::npos,"Long queries were not bounded");
  require(memory_prefetch::boundedQuery(std::string(1999,'a')+"é",2000)==
    std::string(1999,'a')+"\n[... 2 more bytes not shown]","A cut split a UTF-8 character");

  const auto dir=fs::temp_directory_path()/("batbox-recall-"+platform::randomUUID());
  struct Cleanup{fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}}
    cleanup{dir};
  fs::create_directories(dir);
  write(dir/"MEMORY.md","- [Deploy](deploy-notes.md)\n");
  event_loop::Loop loop;
  instructions::MemoryPathStash pathStash;
  memory_prefetch::Services services{loop};
  services.pathStash=&pathStash;
  services.cwd=dir.string();
  services.paths.projectRoot=dir.string();
  services.paths.configHomeDir=dir.string();
  services.paths.environment={{services.paths.keys.fullPathOverride,dir.string()}};
  services.searchFeatureEnabled=[]{return true;};
  services.defaultStandardModel=[]{return std::string("remote");};
  services.logWarning=[](const std::string& message){throw std::runtime_error(message);};
  services.logError=[](std::exception_ptr error){std::rethrow_exception(error);};
  services.isAbortError=[](const std::exception&){return false;};
  services.nowMillis=[]{return 1700000000000.0;};
  services.keywordSelection=[](const std::string& model){return model=="local";};
  int requests=0;Json lastRequest;
  services.sideQuery=[&](const Json& request,std::stop_token){
    ++requests;lastRequest=request;
    event_loop::Deferred<Json> answer(loop);
    answer.resolve(Json{{"content",Json::array({Json{{"type","text"},
      {"text","{\"selected_memories\":[\"deploy-notes.md\"]}"}}})}});
    return answer;
  };
  const auto empty=memory_prefetch::startRelevantMemoryPrefetch(
    turn("Where are the deploy notes?"),turnContext("remote"),services);
  require(settle(loop,empty).empty()&&requests==0,
    "A memory folder without topic files still sent a selector request");

  write(dir/"deploy-notes.md","---\ndescription: How we ship releases to staging\n---\n"
    "Run make ship.\n");
  write(dir/"lunch.md","---\ndescription: Favourite food\n---\nNoodles.\n");
  const auto bat=memory_prefetch::startRelevantMemoryPrefetch(
    turn("Where are the deploy notes?"),turnContext("remote","explore-1"),services);
  require(!bat.isSettled&&requests==0,"A Bat started memory recall");

  const auto local=memory_prefetch::startRelevantMemoryPrefetch(
    turn("How do I deploy to staging?"),turnContext("local"),services);
  require(paths(settle(loop,local))==std::vector<std::string>({"deploy-notes.md"})&&
    requests==0,"Keyword recall on a local endpoint sent a model request or missed");

  const auto pasted="How do I deploy this? "+std::string(50000,'z')+" TAIL-MARKER";
  const auto remote=memory_prefetch::startRelevantMemoryPrefetch(
    turn(pasted),turnContext("remote"),services);
  require(paths(settle(loop,remote))==std::vector<std::string>({"deploy-notes.md"})&&
    requests==1,"Model recall did not run on a remote endpoint");
  const auto content=lastRequest.at("messages").at(0).at("content").get<std::string>();
  require(content.find("TAIL-MARKER")==std::string::npos&&content.size()<4000&&
    content.find("deploy-notes.md")!=std::string::npos,"The selector query was not bounded");
  std::cout<<"Memory recall skips Bats and empty folders, picks by keywords on local "
    "endpoints and bounds model selector queries\n";
}
