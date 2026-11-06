#include "application_media/AttachmentAggregate.hpp"
#include "memory_prefetch/PoetryScan.hpp"
#include "skills/SkillGadgetPoetry.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <regex>
#include <stdexcept>
#include <thread>
#include <unordered_set>

namespace batbox::application_media {
namespace {
event_loop::Deferred<Json> ready(event_loop::Loop& loop,Json value){
  event_loop::Deferred<Json> result(loop);result.resolve(std::move(value));return result;
}
const Json& field(const Json& value,const char* name){return value::property(value,name);}
const Json& options(const execution::Context& context){
  return field(context.data,"options");
}
std::string stringOr(const Json& value,const std::string& fallback={}){
  return value.is_string()?value.get<std::string>():fallback;
}
Json arrayOr(const Json& value){return value.is_array()?value:Json::array();}
std::vector<std::string> resourceMentions(const std::string& text){
  // Match /(^|\s)@([^\s]+:[^\s]+)\b/g, with deduplication preserving
  // mention order. Keep the boundary as a regex rather than treating all @
  // mentions as MCP resources.
  static const std::regex pattern(R"((^|\s)@([^\s]+:[^\s]+)\b)");
  std::vector<std::string> out;
  std::unordered_set<std::string> seen;
  for(auto it=std::sregex_iterator(text.begin(),text.end(),pattern);
      it!=std::sregex_iterator();++it){
    const auto mention=(*it)[2].str();
    if(seen.insert(mention).second)out.push_back(mention);
  }
  return out;
}
bool connected(const Json& clients,const std::string& name){
  if(!clients.is_array())return false;
  return std::any_of(clients.begin(),clients.end(),[&](const Json& client){
    return field(client,"name")==name&&field(client,"type")=="connected";});
}
std::optional<Json> resourceInfo(const Json& resources,
    const std::string& name,const std::string& uri){
  const auto& server=value::property(resources,name);
  if(!server.is_array())return {};
  for(const auto& resource:server)
    if(field(resource,"uri")==uri)return resource;
  return {};
}
std::vector<Json> skillRows(const Json& value){
  if(!value.is_array())throw std::runtime_error(
    "Skill command provider must return an array");
  return std::vector<Json>(value.begin(),value.end());
}
std::string model(const execution::Context& context){
  return stringOr(field(options(context),"mainLoopModel"));
}
std::string batId(const execution::Context& context){
  return stringOr(field(context.data,"agentId"));
}
event_loop::Deferred<Json> unsupported(event_loop::Loop& loop,
    const char* feature){
  event_loop::Deferred<Json> value(loop);
  value.reject(std::make_exception_ptr(std::runtime_error(
    std::string(feature)+" attachment provider is unavailable")));
  return value;
}
}

void AggregateProvider::bindRemaining(attachments::Services& services){
  auto& loop=services.loop;
  auto self=shared_from_this();
  services.collectors.batMentions=[self,&loop](
      const attachments::Input& input,execution::Context& context){
    Json result=Json::array();
    if(!input.text)return ready(loop,std::move(result));
    const auto mentions=memory_prefetch::extractBatMentions(*input.text);
    if(mentions.empty())return ready(loop,std::move(result));
    const auto& definitions=field(
      field(options(context),"agentDefinitions"),"activeAgents");
    for(const auto& mention:mentions){
      const auto batType=mention.starts_with("agent-")?
        mention.substr(6):mention;
      if(!definitions.is_array())continue;
      auto found=std::find_if(definitions.begin(),definitions.end(),
        [&](const Json& definition){
          return field(definition,"agentType")==batType;});
      if(found==definitions.end()){
        self->inputs_.log("Agent mention not found: "+batType);
        continue;
      }
      result.push_back(Json{{"type","agent_mention"},{"agentType",batType}});
    }
    return ready(loop,std::move(result));
  };
  services.collectors.mcpResources=[self,&loop](
      const attachments::Input& input,execution::Context& context){
    if(!input.text)return ready(loop,Json::array());
    const auto mentions=resourceMentions(*input.text);
    if(mentions.empty())return ready(loop,Json::array());
    const auto& configuredClients=field(options(context),"mcpClients");
    const auto& configuredResources=field(options(context),"mcpResources");
    const Json clients=configuredClients.is_array()?configuredClients:
      self->inputs_.mcpClients?self->inputs_.mcpClients():Json();
    const Json resources=configuredResources.is_object()?configuredResources:
      self->inputs_.mcpResources?self->inputs_.mcpResources():Json();
    if(!self->inputs_.readMcpResource)
      return unsupported(loop,"MCP resource read");
    struct Pending {
      event_loop::Deferred<Json> result;
      explicit Pending(event_loop::Loop& owner):result(owner){}
    };
    auto pending=std::make_shared<Pending>(loop);
    auto remaining=std::make_shared<std::size_t>(0);
    auto rows=std::make_shared<std::vector<std::optional<Json>>>(mentions.size());
    const auto stop=context.stop;
    for(std::size_t index=0;index<mentions.size();++index){
      const auto& mention=mentions[index];
      const auto colon=mention.find(':');
      if(colon==std::string::npos||colon==0||colon+1==mention.size())continue;
      const auto server=mention.substr(0,colon);
      const auto uri=mention.substr(colon+1);
      if(!connected(clients,server))continue;
      const auto info=resourceInfo(resources,server,uri);
      if(!info)continue;
      ++*remaining;
      try{
        auto future=self->inputs_.readMcpResource(server,uri,stop);
        std::thread([future=std::move(future),pending,remaining,rows,
            info=*info,server,uri,index,self]()mutable{
          std::optional<Json> row;
          std::string error;
          try{
            const auto content=future.get();
            row=Json{{"type","mcp_resource"},{"server",server},
              {"uri",uri},{"name",stringOr(field(info,"name"),uri)},
              {"content",content}};
            const auto& description=field(info,"description");
            if(!description.is_null())(*row)["description"]=description;
          }catch(const std::exception& issue){error=issue.what();}
          try{
            pending->result.dispatcher().postMission(
              [pending,remaining,rows,row=std::move(row),error,index,self]{
                if(!error.empty())
                  self->inputs_.log("MCP resource read: "+error);
                (*rows)[index]=row;
                if(--*remaining==0){
                  Json results=Json::array();
                  for(const auto& value:*rows)if(value)results.push_back(*value);
                  pending->result.resolve(std::move(results));
                }
              });
          }catch(...){
            // Owner shutdown can close its dispatcher while the backend read
            // is settling. Late attachment responses are also dropped.
          }
        }).detach();
      }catch(const std::exception& issue){
        self->inputs_.log("MCP resource read: "+std::string(issue.what()));
        --*remaining;
      }
    }
    if(*remaining==0)pending->result.resolve(Json::array());
    return pending->result;
  };
  services.collectors.skillListing=[self,&loop](
      const attachments::Input&,execution::Context& context){
    if(!gadgets::findGadgetByName(context.gadgets,"Skill"))
      return ready(loop,Json::array());
    if(!self->inputs_.skillCommands)
      return unsupported(loop,"Skill command listing");
    auto commands=skillRows(self->inputs_.skillCommands());
    const auto key=batId(context);
    auto& sent=self->sentSkillNames_[key];
    std::vector<Json> newSkills;
    for(const auto& command:commands){
      const auto name=stringOr(field(command,"name"));
      if(!name.empty()&&!sent.contains(name))newSkills.push_back(command);
    }
    if(newSkills.empty())return ready(loop,Json::array());
    const bool initial=sent.empty();
    for(const auto& command:newSkills)
      sent.insert(stringOr(field(command,"name")));
    skills::SkillPoetryServices formatter;
    formatter.stringWidth=[](const std::string& value){
      return static_cast<double>(value::stringLength(value));};
    formatter.truncate=[](const std::string& value,double width){
      return value::slice(value,0,width);};
    std::optional<double> window;
    if(self->inputs_.effectiveContextWindow){
      const auto chosen=model(context);
      if(!chosen.empty())
        window=self->inputs_.effectiveContextWindow(chosen);
    }
    const auto content=skills::formatCommandsWithinBudget(newSkills,window,
      formatter);
    return ready(loop,Json::array({Json{{"type","skill_listing"},
      {"content",content},{"skillCount",newSkills.size()},
      {"isInitial",initial}}}));
  };
  services.collectors.batPendingMessages=[self,&loop](
      const attachments::Input&,execution::Context& context){
    const auto id=batId(context);
    if(id.empty())return ready(loop,Json::array());
    if(!self->inputs_.drainBatPendingMessages)
      return unsupported(loop,"Agent pending message");
    const auto drained=arrayOr(self->inputs_.drainBatPendingMessages(id));
    Json output=Json::array();
    for(const auto& message:drained)
      output.push_back(Json{{"type","queued_command"},{"prompt",message},
        {"origin",Json{{"kind","coordinator"}}},{"isMeta",true}});
    return ready(loop,std::move(output));
  };
  services.collectors.asyncHookResponses=[self,&loop](
      const attachments::Input&,execution::Context&){
    if(!self->inputs_.getAsyncHookResponses)
      return ready(loop,Json::array());
    return self->inputs_.getAsyncHookResponses();
  };
}
} // namespace batbox::application_media
