#include "memory_prefetch/IndigenousIO.hpp"
#include <stdexcept>

namespace batbox::memory_prefetch {
namespace {
constexpr const char* selectionPoetry=
  "You are selecting memories that will be useful to Batbox as it processes a user's query. "
  "You will be given the user's query and a list of available memory files with their filenames and descriptions.\n\n"
  "Return a list of filenames for the memories that will clearly be useful to Batbox as it processes the user's query (up to 5). "
  "Only include memories that you are certain will be helpful based on their name and description.\n"
  "- If you are unsure if a memory will be useful in processing the user's query, then do not include it in your list. Be selective and discerning.\n"
  "- If there are no memories in the list that would clearly be useful, feel free to return an empty list.\n"
  "- If a list of recently-used tools is provided, do not select memories that are usage reference or API documentation for those tools "
  "(Batbox is already exercising them). DO still select memories containing warnings, gotchas, or known issues about those tools — active use is exactly when those matter.\n";
std::string joinGadgets(const std::vector<std::string>& gadgets){
  std::string result;
  for(const auto& gadget:gadgets){if(!result.empty())result+=", ";result+=gadget;}
  return result;
}
}
event_loop::Deferred<std::vector<std::string>> selectRelevantMemories(
  const std::string& query,const std::vector<instructions::MemoryHeader>& memories,
  const std::vector<std::string>& recentGadgets,std::stop_token signal,
  const Services& services){
  event_loop::Deferred<std::vector<std::string>> output(services.loop);
  if(!services.sideQuery||!services.defaultStandardModel)
    throw std::invalid_argument("Memory selector requires model and sideQuery bindings");
  std::unordered_set<std::string> valid;
  for(const auto& memory:memories)valid.insert(memory.filename);
  std::string user="Query: "+query+"\n\nAvailable memories:\n"+
    instructions::formatMemoryManifest(memories);
  if(!recentGadgets.empty())user+="\n\nRecently used tools: "+joinGadgets(recentGadgets);
  Json schema={{"type","object"},
    {"properties",{{"selected_memories",{{"type","array"},
      {"items",{{"type","string"}}}}}}},
    {"required",Json::array({"selected_memories"})},
    {"additionalProperties",false}};
  try{
    Json request={{"model",services.defaultStandardModel()},
    {"system",selectionPoetry},{"skipSystemPromptPrefix",true},
    {"messages",Json::array({Json{{"role","user"},{"content",user}}})},
    {"max_tokens",256},
    {"output_format",{{"type","json_schema"},{"schema",schema}}},
    {"querySource","memdir_relevance"}};
    auto pending=services.sideQuery(request,signal);
    if(!services.loop.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Memory selector sideQuery belongs to another event loop");
    pending.then([output,valid=std::move(valid),signal,services](
      const Json* value,const std::exception_ptr& error){
    try{
      if(error)std::rethrow_exception(error);
      if(!value)throw std::runtime_error("Memory selector returned no response");
      const Json& content=value::property(*value,"content");
      if(content.is_array())for(const Json& block:content){
        if(value::property(block,"type")!="text")continue;
        const Json parsed=value::parse(value::property(block,"text").get<std::string>());
        const Json& names=parsed.at("selected_memories");
        std::vector<std::string> selected;
        for(const Json& name:names){
          if(!name.is_string())continue;
          const auto text=name.get<std::string>();
          if(valid.contains(text))selected.push_back(text);
        }
        output.resolve(std::move(selected));return;
      }
      output.resolve({});
    }catch(const std::exception& failure){
      if(!signal.stop_requested()){
        if(!services.logWarning)
          {output.reject(std::make_exception_ptr(std::invalid_argument(
            "Memory selector warning logger required")));return;}
        services.logWarning(std::string("[memdir] selectRelevantMemories failed: ")+
          failure.what());
      }
      output.resolve({});
    }
    });
  }catch(const std::exception& failure){
    if(!signal.stop_requested()){
      if(!services.logWarning)
        throw std::invalid_argument("Memory selector warning logger required");
      services.logWarning(std::string("[memdir] selectRelevantMemories failed: ")+
        failure.what());
    }
    output.resolve({});
  }
  return output;
}
event_loop::Deferred<std::vector<RelevantMemory>> findRelevantMemories(
  const std::string& query,const std::string& memoryDir,
  const std::vector<std::string>& recentGadgets,
  const std::unordered_set<std::string>& alreadySurfaced,
  std::stop_token signal,const Services& services){
  event_loop::Deferred<std::vector<RelevantMemory>> output(services.loop);
  auto scanned=scanMemoryFilesAsync(memoryDir,signal,services.loop);
  scanned.then([output,query,recentGadgets,alreadySurfaced,signal,services](
      const std::vector<instructions::MemoryHeader>* value,
      const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    std::vector<instructions::MemoryHeader> available;
    if(value)for(const auto& header:*value)
      if(!alreadySurfaced.contains(header.filePath))available.push_back(header);
    if(available.empty()){output.resolve({});return;}
    try{
      auto selected=selectRelevantMemories(query,available,recentGadgets,
        signal,services);
      selected.then([output,available=std::move(available)](
          const std::vector<std::string>* names,const std::exception_ptr& failure){
        if(failure){output.reject(failure);return;}
        std::vector<RelevantMemory> results;
        if(names)for(const auto& name:*names){
          for(auto memory=available.rbegin();memory!=available.rend();++memory)
            if(memory->filename==name){
              results.push_back({memory->filePath,memory->mtimeMs});break;}
        }
        output.resolve(std::move(results));
      });
    }catch(...){output.reject(std::current_exception());}
  });
  return output;
}
} // namespace batbox::memory_prefetch
