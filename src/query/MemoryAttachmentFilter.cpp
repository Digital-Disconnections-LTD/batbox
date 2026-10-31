#include "query/MemoryAttachmentFilter.hpp"
#include <stdexcept>

namespace batbox::query {
Json filterDuplicateMemoryAttachments(const Json& attachments,
  file_state_stash::FileStateStash& readFileState){
  if(!attachments.is_array())
    throw std::invalid_argument("Memory attachments must be an array");
  Json output=Json::array();
  for(const Json& attachment:attachments){
    if(value::property(attachment,"type")!="relevant_memories"){
      output.push_back(attachment);continue;
    }
    const Json& memories=attachment.at("memories");
    if(!memories.is_array())
      throw std::invalid_argument("Relevant memories must be an array");
    Json filtered=Json::array();
    for(const Json& memory:memories)
      if(!readFileState.has(memory.at("path").get<std::string>()))
        filtered.push_back(memory);
    // Mark after filtering the whole attachment: repeated paths within that
    // attachment survive, while later attachments see the newly marked path.
    for(const Json& memory:filtered){
      file_state_stash::FileState state;
      state.content=memory.at("content").get<std::string>();
      state.timestamp=memory.at("mtimeMs").get<double>();
      const Json& limit=value::property(memory,"limit");
      if(limit.is_number())state.limit=limit.get<double>();
      readFileState.set(memory.at("path").get<std::string>(),std::move(state));
    }
    if(!filtered.empty()){
      Json kept=attachment;
      kept["memories"]=std::move(filtered);
      output.push_back(std::move(kept));
    }
  }
  return output;
}
} // namespace batbox::query
