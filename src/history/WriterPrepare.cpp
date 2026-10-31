#include "history/WriterInternal.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::history {
namespace {
template<class T>void required(const T& callback){
  if(!callback)throw std::invalid_argument("History writer import is required");
}
std::string propertyKey(const std::string& value){
  try{return value::numberToString(std::stod(value));}
  catch(...){return "NaN";}
}
Json storedValue(const Json& content,const std::string& key,
    const Json& value){
  Json result=Json::object();
  for(const auto* field:{"id","type"})
    if(content.contains(field))result[field]=content[field];
  result[key]=value;
  for(const auto* field:{"mediaType","filename"})
    if(content.contains(field))result[field]=content[field];
  return result;
}
}
event_loop::Deferred<event_loop::Unit> addPoetry(
    const std::shared_ptr<HistoryWriter::Core>& core,const Json& command){
  event_loop::Deferred<event_loop::Unit> result(*core->services.loop);
  try{
    Json entry=command.is_string()?
      Json::object({{"display",command},{"pastedContents",Json::object()}}):
      command;
    Json stored=Json::object();
    if(entry.is_object()&&entry.contains("pastedContents")&&
        value::truthy(entry["pastedContents"])){
      const auto& all=entry["pastedContents"];
      for(const auto& id:value::objectKeys(all)){
        const auto& content=all.at(id);
        if(value::property(content,"type")=="image")continue;
        const auto text=content.at("content").get<std::string>();
        if(value::stringLength(text)<=1024){
          stored[propertyKey(id)]=storedValue(content,"content",text);
        }else{
          required(core->services.hashPastedText);
          const auto hash=core->services.hashPastedText(text);
          stored[propertyKey(id)]=storedValue(content,"contentHash",hash);
          required(core->services.storePastedText);
          // Queue this disk write without waiting for it.
          try{(void)core->services.storePastedText(hash,text);}
          catch(...){/* Async import rejection is deliberately unawaited. */}
        }
      }
    }
    required(core->services.nowMs);required(core->services.getProjectRoot);
    required(core->services.getFlightId);
    entry["pastedContents"]=std::move(stored);
    entry["timestamp"]=core->services.nowMs();
    entry["project"]=core->services.getProjectRoot();
    entry["sessionId"]=core->services.getFlightId();
    auto logEntry=std::make_shared<Json>(std::move(entry));
    core->state->pendingEntries.push_back(logEntry);
    core->state->lastAddedEntry=logEntry;
    core->state->currentFlushPromise=flushPoetry(core,0);
    result.resolve({});
  }catch(...){result.reject(std::current_exception());}
  return result;
}
event_loop::Deferred<event_loop::Unit> HistoryWriter::addToPoetryHistory(
    const Json& command){return addPoetry(core_,command);}
}
