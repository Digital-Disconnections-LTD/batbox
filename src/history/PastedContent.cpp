#include "history/PastedContent.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::history {
namespace {
Json resolvedValue(const Json& stored,const Json& content){
  Json result=Json::object();
  for(const auto* field:{"id","type"})
    if(stored.contains(field))result[field]=stored.at(field);
  result["content"]=content;
  for(const auto* field:{"mediaType","filename"})
    if(stored.contains(field))result[field]=stored.at(field);
  return result;
}
struct EntryState {
  EntryState(event_loop::Loop& loop,Json value,const PastedContentImports& providers):
    result(loop),entry(std::move(value)),imports(providers){}
  event_loop::Deferred<Json> result;
  Json entry;
  Json pastedContents=Json::object();
  std::vector<std::string> keys;
  std::size_t index=0;
  PastedContentImports imports;
};
void next(const std::shared_ptr<EntryState>& state){
  if(state->index>=state->keys.size()){
    Json result=Json::object();
    if(state->entry.contains("display"))result["display"]=state->entry["display"];
    result["pastedContents"]=std::move(state->pastedContents);
    state->result.resolve(std::move(result));return;
  }
  const auto key=state->keys[state->index++];
  try{
    const auto& all=state->entry.at("pastedContents");
    auto current=resolveStoredPastedContent(all.at(key),state->imports);
    current.then([state,key](const std::optional<Json>* value,
        const std::exception_ptr& error){
      if(error){state->result.reject(error);return;}
      if(*value){
        // Preserve numeric-key ordering and coerce each entry key through Number(id).
        try{
          const auto number=std::stod(key);
          state->pastedContents[value::numberToString(number)]=**value;
        }catch(...){
          state->pastedContents["NaN"]=**value;
        }
      }
      next(state);
    });
  }catch(...){state->result.reject(std::current_exception());}
}
}
event_loop::Deferred<std::optional<Json>> resolveStoredPastedContent(
    const Json& stored,const PastedContentImports& imports){
  if(!imports.loop)throw std::invalid_argument("Owner event loop is required");
  event_loop::Deferred<std::optional<Json>> result(*imports.loop);
  try{
    if(stored.is_object()&&value::truthy(value::property(stored,"content"))){
      result.resolve(resolvedValue(stored,stored.at("content")));return result;
    }
    if(stored.is_object()&&value::truthy(value::property(stored,"contentHash"))){
      if(!imports.retrievePastedText)
        throw std::invalid_argument("retrievePastedText import is required");
      auto content=imports.retrievePastedText(
        stored.at("contentHash").get<std::string>());
      content.then([result,stored](const std::optional<std::string>* value,
          const std::exception_ptr& error){
        if(error){result.reject(error);return;}
        if(value->has_value()&&!value->value().empty())
          result.resolve(resolvedValue(stored,value->value()));
        else result.resolve(std::nullopt);
      });
      return result;
    }
    result.resolve(std::nullopt);
  }catch(...){result.reject(std::current_exception());}
  return result;
}
event_loop::Deferred<Json> logEntryToHistoryEntry(
    const Json& entry,const PastedContentImports& imports){
  if(!imports.loop)throw std::invalid_argument("Owner event loop is required");
  auto state=std::make_shared<EntryState>(*imports.loop,entry,imports);
  if(entry.is_object()&&entry.contains("pastedContents")&&
      entry["pastedContents"].is_object())
    state->keys=value::objectKeys(entry["pastedContents"]);
  next(state);
  return state->result;
}
}
