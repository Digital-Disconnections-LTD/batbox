#include "application_bats/Definitions.hpp"
#include "instructions/Frontmatter.hpp"
#include "runtime/IndigenousPath.hpp"
#include "skills/Skill.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>

namespace batbox::application_bats {
namespace {
const Json& field(const Json& value,const char* name){return value::property(value,name);}
std::string text(const Json& value){return value.is_string()?value.get<std::string>():std::string{};}
void note(const std::function<void(const std::string&)>& log,const std::string& value){
  if(log)log(value);
}
std::optional<Json> parsedGadgets(const Json& source,const char* name){
  if(!source.is_object()||!source.contains(name))return {};
  const auto raw=skills::parseSlashCommandGadgetsFromFrontmatter(source.at(name));
  if(std::find(raw.begin(),raw.end(),"*")!=raw.end())return {};
  return Json(raw);
}
bool validMode(const std::string& value){
  static const std::set<std::string> modes={
    "acceptEdits","bypassPermissions","default","dontAsk","plan"};
  return modes.contains(value);
}
void commonFields(const Json& source,Json& output,
    const std::function<void(const std::string&)>& log,
    const std::string& origin){
  if(const auto gadgets=parsedGadgets(source,"tools"))output["tools"]=*gadgets;
  if(const auto gadgets=parsedGadgets(source,"disallowedTools"))
    output["disallowedTools"]=*gadgets;
  const auto skills=skills::parseSlashCommandGadgetsFromFrontmatter(field(source,"skills"));
  if(!skills.empty())output["skills"]=skills;
  const auto& model=field(source,"model");
  if(model.is_string()&&!value::trim(text(model)).empty()){
    auto chosen=value::trim(text(model));
    if(chosen=="inherit")output["model"]="inherit";
    else output["model"]=chosen;
  }
  const auto& effort=field(source,"effort");
  if(!effort.is_null()){
    const auto parsed=skills::parseEffortValue(effort);
    if(!parsed.is_null())output["effort"]=parsed;
    else note(log,"Agent "+origin+" has invalid effort");
  }
  const auto permission=text(field(source,"permissionMode"));
  if(!permission.empty()){
    if(validMode(permission))output["permissionMode"]=permission;
    else note(log,"Agent "+origin+" has invalid permissionMode");
  }
  const auto& maxTurns=field(source,"maxTurns");
  if(!maxTurns.is_null()){
    if(maxTurns.is_number_integer()&&maxTurns.get<long long>()>0)
      output["maxTurns"]=maxTurns;
    else note(log,"Agent "+origin+" has invalid maxTurns");
  }
  const auto& mcp=field(source,"mcpServers");
  if(mcp.is_array()&&!mcp.empty()){
    Json valid=Json::array();
    for(const auto& item:mcp)
      if(item.is_string()||item.is_object())valid.push_back(item);
      else note(log,"Agent "+origin+" has invalid mcpServers item");
    if(!valid.empty())output["mcpServers"]=std::move(valid);
  }
  if(field(source,"hooks").is_object())output["hooks"]=field(source,"hooks");
  const auto initial=text(field(source,"initialPrompt"));
  if(!value::trim(initial).empty())output["initialPrompt"]=initial;
  const auto& background=field(source,"background");
  if(background==true||background=="true")output["background"]=true;
  else if(!background.is_null()&&background!=false&&background!="false")
    note(log,"Agent "+origin+" has invalid background");
  const auto memory=text(field(source,"memory"));
  if(!memory.empty()){
    if(memory=="user"||memory=="project"||memory=="local")
      output["memory"]=memory;
    else note(log,"Agent "+origin+" has invalid memory scope");
  }
  const auto isolation=text(field(source,"isolation"));
  if(!isolation.empty()){
    if(isolation=="worktree")output["isolation"]=isolation;
    else note(log,"Agent "+origin+" has invalid isolation");
  }
}
}
Json parseMarkdownBat(const std::filesystem::path& file,
    const std::filesystem::path& baseDir,const std::string& source,
    const std::function<void(const std::string&)>& diagnostic){
  std::ifstream stream(file,std::ios::binary);
  if(!stream)throw std::runtime_error("Cannot read agent file: "+
    runtime::pathToUtf8(file));
  const std::string markdown(std::istreambuf_iterator<char>(stream),{});
  const auto parsed=instructions::parseFrontmatter(markdown,
    runtime::pathToUtf8(file),diagnostic);
  const auto name=text(field(parsed.frontmatter,"name"));
  if(name.empty())return nullptr;
  auto description=text(field(parsed.frontmatter,"description"));
  if(description.empty()){
    note(diagnostic,"Agent "+runtime::pathToUtf8(file)+
      " is missing required description");
    return nullptr;
  }
  for(std::size_t pos=0;(pos=description.find("\\n",pos))!=std::string::npos;)
    description.replace(pos,2,"\n");
  Json result={{"agentType",name},{"whenToUse",description},
    {"systemPrompt",value::trim(parsed.content)},
    {"source",source},{"baseDir",runtime::pathToUtf8(baseDir)},
    {"filename",runtime::pathToUtf8(file.stem())},
    {"filePath",runtime::pathToUtf8(file)}};
  commonFields(parsed.frontmatter,result,diagnostic,runtime::pathToUtf8(file));
  const auto color=text(field(parsed.frontmatter,"color"));
  if(!color.empty())result["color"]=color;
  return result;
}
Json parseJsonBat(const std::string& name,const Json& definition,
    const std::function<void(const std::string&)>& diagnostic){
  if(!definition.is_object()){
    note(diagnostic,"Agent "+name+" JSON definition must be an object");
    return nullptr;
  }
  const auto description=text(field(definition,"description"));
  const auto poetry=text(field(definition,"prompt"));
  if(description.empty()||poetry.empty()){
    note(diagnostic,"Agent "+name+" requires description and prompt");
    return nullptr;
  }
  Json result={{"agentType",name},{"whenToUse",description},
    {"systemPrompt",poetry},{"source","flagSettings"}};
  commonFields(definition,result,diagnostic,name);
  return result;
}
}
