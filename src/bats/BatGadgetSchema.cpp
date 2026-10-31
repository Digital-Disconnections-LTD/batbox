#include "bats/BatGadgetSchema.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
Json property(const char* kind,const std::string& description,bool optional=false){
  Json value={{"kind",kind},{"description",description}};
  if(optional)value["optional"]=true;
  return value;
}
Json choice(const Json& values,const std::string& description,bool optional=false){
  Json value=property("enum",description,optional);
  value["values"]=values;
  return value;
}
Json values(std::initializer_list<const char*> items){
  Json result=Json::array();
  for(const auto* item:items)result.push_back(item);
  return result;
}
bool contains(const Json& values,const Json& value){
  for(const auto& item:values)if(item==value)return true;
  return false;
}
Json node(const char* kind){return Json{{"kind",kind}};}
Json object(const Json& properties){return Json{{"kind","object"},{"properties",properties}};}
Json nullable(Json value){value["nullable"]=true;return value;}
Json optional(Json value){value["optional"]=true;return value;}
Json literal(const char* value){return Json{{"kind","literal"},{"value",value}};}
Json described(Json value,const char* description){
  value["description"]=description;return value;
}
Json toJSONSchema(const Json& source){
  const auto kind=source.at("kind").get<std::string>();
  Json out;
  if(kind=="object"){
    Json properties=Json::object(),required=Json::array();
    for(auto it=source.at("properties").begin();it!=source.at("properties").end();++it){
      properties[it.key()]=toJSONSchema(it.value());
      if(!it.value().value("optional",false))required.push_back(it.key());
    }
    out={{"type","object"},{"properties",properties},
      {"required",required},{"additionalProperties",false}};
  }else if(kind=="array")out={{"type","array"},
    {"items",toJSONSchema(source.at("element"))}};
  else if(kind=="union"){
    Json options=Json::array();
    for(const auto& entry:source.at("options"))options.push_back(toJSONSchema(entry));
    out={{"anyOf",options}};
  }else if(kind=="enum")out={{"type","string"},{"enum",source.at("values")}};
  else if(kind=="literal")out={{"type","string"},{"const",source.at("value")}};
  else out={{"type",kind}};
  if(source.contains("description"))out["description"]=source.at("description");
  if(source.value("nullable",false))out=Json{{"anyOf",Json::array({out,Json{{"type","null"}}})}};
  return out;
}
}
Json batGadgetInputShape(const BatGadgetSchemaOptions& options){
  Json properties=Json::object();
  properties["description"]=property("string",
    "A short (3-5 word) description of the task");
  properties["prompt"]=property("string","The task for the agent to perform");
  properties["subagent_type"]=property("string",
    "The type of specialized agent to use for this task",true);
  properties["model"]=choice(values({"standard","premium","fast"}),
    "Optional model override for this agent. Takes precedence over the agent definition's model frontmatter. If omitted, uses the agent definition's model, or inherits from the parent.",true);
  if(!options.backgroundMissionsDisabled && !options.forkSubagentEnabled)
    properties["run_in_background"]=property("boolean",
      "Set to true to run this agent in the background. You will be notified when it completes.",true);
  properties["name"]=property("string",
    "Name for the spawned agent. Makes it addressable via SendMessage({to: name}) while running.",true);
  properties["team_name"]=property("string",
    "Team name for spawning. Uses current team context if omitted.",true);
  Json modes=values({"acceptEdits","bypassPermissions","default","dontAsk","plan"});
  if(options.transcriptClassifier)modes.push_back("auto");
  properties["mode"]=choice(modes,
    "Permission mode for spawned teammate (e.g., \"plan\" to require plan approval).",true);
  properties["isolation"]=choice(values({"worktree"}),
    "Isolation mode. \"worktree\" creates a temporary git worktree so the agent works on an isolated copy of the repo.",true);
  if(options.kairos)properties["cwd"]=property("string",
    "Absolute path to run the agent in. Overrides the working directory for all filesystem and shell operations within this agent. Mutually exclusive with isolation: \"worktree\".",true);
  return Json{{"kind","object"},{"properties",properties}};
}

Json batGadgetInputJSONSchema(const BatGadgetSchemaOptions& options){
  const Json shape=batGadgetInputShape(options);
  Json properties=Json::object();
  for(auto it=shape.at("properties").begin();it!=shape.at("properties").end();++it){
    const Json& entry=it.value();
    Json wire={{"description",entry.at("description")}};
    if(entry.at("kind")=="enum"){
      wire["type"]="string";wire["enum"]=entry.at("values");
    }else wire["type"]=entry.at("kind");
    properties[it.key()]=std::move(wire);
  }
  return Json{{"type","object"},{"properties",properties},
    {"required",Json::array({"description","prompt"})},
    {"additionalProperties",false},{"$schema","https://json-schema.org/draft/2020-12/schema"}};
}

Json inspectBatGadgetInput(const Json& input,const BatGadgetSchemaOptions& options){
  if(!input.is_object())return Json{{"success",false},
    {"error",Json::array({Json{{"code","invalid_type"},{"path",Json::array()}}})}};
  const Json shape=batGadgetInputShape(options);
  Json output=Json::object(),issues=Json::array();
  for(auto it=shape.at("properties").begin();it!=shape.at("properties").end();++it){
    const auto key=it.key();
    const Json& spec=it.value();
    if(!input.contains(key)){
      if(!spec.value("optional",false))
        issues.push_back(Json{{"code","invalid_type"},
          {"path",Json::array({key})}});
      continue;
    }
    const Json& value=input.at(key);
    const auto kind=spec.at("kind");
    if((kind=="string" && !value.is_string()) ||
       (kind=="boolean" && !value.is_boolean())){
      issues.push_back(Json{{"code","invalid_type"},
        {"path",Json::array({key})}});
      continue;
    }
    if(kind=="enum" && !contains(spec.at("values"),value)){
      issues.push_back(Json{{"code","invalid_value"},
        {"path",Json::array({key})}});
      continue;
    }
    output[key]=value;
  }
  if(!issues.empty())return Json{{"success",false},{"error",issues}};
  return Json{{"success",true},{"data",output}};
}

Json parseBatGadgetInput(const Json& input,const BatGadgetSchemaOptions& options){
  const Json inspected=inspectBatGadgetInput(input,options);
  if(inspected.at("success")==true)return inspected.at("data");
  throw std::invalid_argument("Agent input validation failed: "+
    value::stringify(inspected.at("error")));
}

Json batGadgetOutputShape(){
  const Json number=node("number"),string=node("string");
  Json usage=object(Json{{"input_tokens",number},{"output_tokens",number},
    {"cache_creation_input_tokens",nullable(number)},
    {"cache_read_input_tokens",nullable(number)},
    {"server_tool_use",nullable(object(Json{{"web_search_requests",number},
      {"web_fetch_requests",number}}))},
    {"service_tier",nullable(Json{{"kind","enum"},
      {"values",values({"standard","priority","batch"})}})},
    {"cache_creation",nullable(object(Json{{"ephemeral_1h_input_tokens",number},
      {"ephemeral_5m_input_tokens",number}}))}});
  Json textBlock=object(Json{{"type",literal("text")},{"text",string}});
  Json sync=object(Json{{"agentId",string},{"agentType",optional(string)},
    {"content",Json{{"kind","array"},{"element",textBlock}}},
    {"totalToolUseCount",number},{"totalDurationMs",number},
    {"totalTokens",number},{"usage",usage},
    {"status",literal("completed")},{"prompt",string}});
  Json async=object(Json{{"status",literal("async_launched")},
    {"agentId",described(string,"The ID of the async agent")},
    {"description",described(string,"The description of the task")},
    {"prompt",described(string,"The prompt for the agent")},
    {"outputFile",described(string,
      "Path to the output file for checking agent progress")},
    {"canReadOutputFile",described(optional(node("boolean")),
      "Whether the calling agent has Read/Bash tools to check progress")}});
  return Json{{"kind","union"},{"options",Json::array({sync,async})}};
}

Json batGadgetOutputJSONSchema(){
  Json schema=toJSONSchema(batGadgetOutputShape());
  schema["$schema"]="https://json-schema.org/draft/2020-12/schema";
  return schema;
}

void configureBatGadgetInputSchema(gadgets::Gadget& gadget,
    const BatGadgetSchemaOptions& options){
  gadget.inputSchema=batGadgetInputShape(options);
  gadget.inputJSONSchema=batGadgetInputJSONSchema(options);
  gadget.outputSchema=batGadgetOutputJSONSchema();
  gadget.parseInput=[options](const Json& input){
    return parseBatGadgetInput(input,options);
  };
}
} // namespace batbox::bats
