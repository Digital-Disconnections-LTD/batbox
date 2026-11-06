#include "memory_prefetch/IndigenousIO.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

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
// The selector needs the gist of the request, not a pasted log.
constexpr std::size_t selectorQueryBytes=2000;
const std::unordered_set<std::string>& stopWords(){
  static const std::unordered_set<std::string> words{"the","and","for","with","that",
    "this","from","what","how","are","was","were","you","your","can","not","but","all",
    "any","use","using","into","about","have","has","does","did","why","when","where",
    "which","will","would","should","could","there","their","them","then","than","its",
    "our","out","get","got","let","may","also","just","make","more","some","such","only",
    "please","need","want","like","here","these","those","been","being","each","other",
    "very","now","one","two","new","see","way","who","yes","her","his","she","him","they"};
  return words;
}
// Lowercase ASCII words of three or more bytes (UTF-8 letters kept whole),
// with common English endings removed so that note/notes and deploy/deploying
// compare equal.
std::vector<std::string> keywords(std::string_view text){
  std::vector<std::string> result;
  std::string word;
  const auto flush=[&]{
    if(word.size()>=3&&!stopWords().contains(word)){
      if(word.size()>4&&word.ends_with('s')&&!word.ends_with("ss"))word.pop_back();
      for(const char* suffix:{"ing","ment","ed"}){
        const std::string_view tail=suffix;
        if(word.size()>tail.size()+3&&word.ends_with(tail)){
          word.resize(word.size()-tail.size());break;
        }
      }
      result.push_back(word);
    }
    word.clear();
  };
  for(const char c:text){
    const auto byte=static_cast<unsigned char>(c);
    if(byte>=0x80||std::isalnum(byte))
      word.push_back(byte<0x80?static_cast<char>(std::tolower(byte)):c);
    else flush();
  }
  flush();
  return result;
}
}
std::string boundedQuery(const std::string& text,std::size_t limit){
  if(text.size()<=limit)return text;
  auto cut=limit;
  while(cut>0&&(static_cast<unsigned char>(text[cut])&0xC0)==0x80)--cut;
  return text.substr(0,cut)+"\n[... "+std::to_string(text.size()-cut)+" more bytes not shown]";
}
std::vector<std::string> selectMemoriesByKeywords(const std::string& query,
    const std::vector<instructions::MemoryHeader>& memories){
  std::unordered_set<std::string> wanted;
  for(auto& word:keywords(query))wanted.insert(std::move(word));
  std::vector<std::pair<int,std::size_t>> scored;
  for(std::size_t index=0;index<memories.size();++index){
    const auto& memory=memories[index];
    auto name=memory.filename;
    if(name.ends_with(".md"))name.resize(name.size()-3);
    std::unordered_set<std::string> nameWords,descriptionWords;
    for(auto& word:keywords(name))nameWords.insert(std::move(word));
    if(memory.description.is_string())
      for(auto& word:keywords(memory.description.get<std::string>()))
        descriptionWords.insert(std::move(word));
    int score=0;
    for(const auto& word:wanted){
      if(nameWords.contains(word))score+=2;
      else if(descriptionWords.contains(word))score+=1;
    }
    // One shared description word is too weak a signal on its own.
    if(score>=2)scored.emplace_back(score,index);
  }
  std::stable_sort(scored.begin(),scored.end(),
    [](const auto& a,const auto& b){return a.first>b.first;});
  std::vector<std::string> selected;
  for(const auto& [score,index]:scored){
    (void)score;
    selected.push_back(memories[index].filename);
    if(selected.size()==5)break;
  }
  return selected;
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
  std::string user="Query: "+boundedQuery(query,selectorQueryBytes)+"\n\nAvailable memories:\n"+
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
      event_loop::Deferred<std::vector<std::string>> selected(services.loop);
      if(services.selectByKeywords)
        selected.resolve(selectMemoriesByKeywords(query,available));
      else selected=selectRelevantMemories(query,available,recentGadgets,
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
