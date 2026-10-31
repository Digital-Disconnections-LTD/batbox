#include "memory_prefetch/Prefetch.hpp"
#include "memory_prefetch/BatMemoryPath.hpp"
#include "query/MemoryAttachmentFilter.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unistd.h>

using namespace batbox;
namespace {
void settle(event_loop::Loop& loop,const query::MemoryPrefetch& handle){
  for(int i=0;i<2000&&!handle.isSettled();++i){
    loop.runReady();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  loop.runReady();
  assert(handle.isSettled());
}
}
int main(){
  namespace fs=std::filesystem;
  const auto dir=fs::temp_directory_path()/
    ("batbox-memory-prefetch-"+std::to_string(getpid()));
  fs::create_directories(dir);
  const auto note=dir/"notes.md";
  {std::ofstream out(note);out<<"---\ndescription: setup notes\n---\n\nUse the local backend.\n";}
  event_loop::Loop loop;
  instructions::MemoryPathStash paths;
  memory_prefetch::Services services{loop};
  services.pathStash=&paths;
  services.cwd=dir.string();
  services.paths.projectRoot=dir.string();
  services.paths.configHomeDir=dir.string();
  services.paths.environment={{services.paths.keys.fullPathOverride,dir.string()}};
  assert(memory_prefetch::getBatMemoryDir("sample:agent","project",
    dir.string(),services.paths)==(dir/".batbox/agent-memory/sample-agent").string()+"/");
  assert(memory_prefetch::getBatMemoryDir("sample:agent","local",
    dir.string(),services.paths)==(dir/".batbox/agent-memory-local/sample-agent").string()+"/");
  assert(memory_prefetch::getBatMemoryDir("sample:agent","user",
    dir.string(),services.paths)==(dir/"agent-memory/sample-agent").string()+"/");
  services.searchFeatureEnabled=[](){return true;};
  services.defaultStandardModel=[](){return std::string("standard");};
  services.logWarning=[](const std::string&){assert(false);};
  services.logError=[](std::exception_ptr){assert(false);};
  services.isAbortError=[](const std::exception&){return false;};
  services.nowMillis=[](){return 1700000000000.0;};
  int queries=0;
  services.sideQuery=[&](const Json& request,std::stop_token){
    ++queries;
    assert(request.at("model")=="standard");
    assert(request.at("messages")[0].at("content").get<std::string>().find(
      "notes.md")!=std::string::npos);
    event_loop::Deferred<Json> answer(loop);
    answer.resolve(Json{{"content",Json::array({Json{{"type","text"},
      {"text","{\"selected_memories\":[\"notes.md\"]}"}}})}});
    return answer;
  };
  auto stash=file_state_stash::createFileStateStashWithSizeLimit(100);
  execution::Context context;
  context.readFileState=stash;
  context.data={{"options",{{"agentDefinitions",{{"activeAgents",Json::array()}}}}}};
  const Json messages=Json::array({Json{{"type","user"},
    {"message",{{"content","Please find setup notes"}}}}});
  auto first=memory_prefetch::startRelevantMemoryPrefetch(messages,context,services);
  assert(first.isSettled&&!first.isSettled());
  settle(loop,first);
  const Json prefetched=first.read();
  assert(prefetched.size()==1);
  assert(prefetched[0]["memories"][0]["path"]==note.string());
  assert(stash->size()==0);
  const Json marked=query::filterDuplicateMemoryAttachments(prefetched,*stash);
  assert(marked.size()==1&&stash->size()==1);
  first.dispose();
  auto second=memory_prefetch::startRelevantMemoryPrefetch(messages,context,services);
  settle(loop,second);
  assert(second.read().empty());
  assert(queries==2);
  second.dispose();

  services.searchFeatureEnabled=[](){return false;};
  auto disabled=memory_prefetch::startRelevantMemoryPrefetch(messages,context,services);
  assert(!disabled.isSettled);
  services.searchFeatureEnabled=[](){return true;};
  auto single=memory_prefetch::startRelevantMemoryPrefetch(Json::array({
    Json{{"type","user"},{"message",{{"content","single"}}}}}),context,services);
  assert(!single.isSettled);

  std::optional<event_loop::Deferred<Json>> pending;
  std::stop_token selectorStop;
  services.sideQuery=[&](const Json&,std::stop_token token){
    selectorStop=token;
    pending.emplace(loop);
    return *pending;
  };
  stash->clear();
  auto cancelled=memory_prefetch::startRelevantMemoryPrefetch(messages,context,services);
  for(int i=0;i<2000&&!pending;++i){
    loop.runReady();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(pending);
  cancelled.dispose();
  assert(selectorStop.stop_requested());
  pending->reject(std::make_exception_ptr(std::runtime_error("cancelled")));
  settle(loop,cancelled);
  assert(cancelled.read().empty());

  std::vector<fs::path> extras;
  Json names=Json::array({"notes.md"});
  for(int i=0;i<6;++i){
    const auto extra=dir/("extra"+std::to_string(i)+".md");
    {std::ofstream out(extra);out<<"---\ndescription: extra\n---\nMemory "<<i<<"\n";}
    extras.push_back(extra);names.push_back(extra.filename().string());
  }
  stash->set(note.string(),file_state_stash::FileState{"seen",1});
  services.sideQuery=[&](const Json&,std::stop_token){
    event_loop::Deferred<Json> answer(loop);
    answer.resolve(Json{{"content",Json::array({Json{{"type","text"},
      {"text",value::stringify(Json{{"selected_memories",names}})}}})}});
    return answer;
  };
  auto capped=memory_prefetch::getRelevantMemoryAttachments(
    "Find relevant setup memories",Json::array(),stash,{},std::stop_token{},
    {extras.front().string()},services);
  Json cappedResult;
  capped.then([&](const Json* value,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    if(value)cappedResult=*value;
  });
  for(int i=0;i<2000&&cappedResult.is_null();++i){
    loop.runReady();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(cappedResult.is_array()&&cappedResult.size()==1);
  assert(cappedResult[0]["memories"].size()==5);
  assert(cappedResult[0]["memories"][0]["path"]==extras[1].string());
  assert(stash->size()==1);
  for(const auto& extra:extras)fs::remove(extra);

  const auto large=dir/"large.md";
  {std::ofstream out(large);out<<std::string(5000,'x');}
  auto reading=memory_prefetch::readMemoriesForSurfacing(
    {{large.string(),1700000000000.0}},std::stop_token{},services);
  Json surfaced;
  reading.then([&](const Json* value,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    if(value)surfaced=*value;
  });
  for(int i=0;i<2000&&surfaced.is_null();++i){
    loop.runReady();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(surfaced.is_array()&&surfaced.size()==1);
  assert(surfaced[0]["content"].get<std::string>().find("4096 byte limit")!=
    std::string::npos);
  assert(surfaced[0].contains("limit"));
  fs::remove(large);
  fs::remove(note);fs::remove(dir);
}
