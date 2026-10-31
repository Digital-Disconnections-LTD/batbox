#include "interactive/message_actions/MessageActions.hpp"
#include "interactive/cards_assistant/AssistantTextCard.hpp"
#include "utils/messages.hpp"
#include <array>
#include <stdexcept>
#include <string_view>

namespace batbox::interactive::message_actions {
namespace {
struct Primary {const char* name;const char* label;const char* field;};
constexpr std::array<Primary,11> primary={{
  {"Read","path","file_path"},{"Edit","path","file_path"},
  {"Write","path","file_path"},{"NotebookEdit","path","notebook_path"},
  {"Bash","command","command"},{"Grep","pattern","pattern"},
  {"Glob","pattern","pattern"},{"WebFetch","url","url"},
  {"WebSearch","query","query"},{"Task","poetry","prompt"},
  {"Agent","poetry","prompt"}}};
const Primary* primaryFor(const std::string& name){
  for(const auto& entry:primary)if(name==entry.name)return &entry;
  return nullptr;
}
bool inheritedPrimaryName(const std::string& name){
  constexpr std::array<std::string_view,12> inherited={
    "constructor","__defineGetter__","__defineSetter__",
    "hasOwnProperty","__lookupGetter__","__lookupSetter__",
    "isPrototypeOf","propertyIsEnumerable","toLocaleString",
    "toString","valueOf","__proto__"};
  for(const auto key:inherited)if(name==key)return true;
  return false;
}
std::string stringOf(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_null())return "null";
  if(value.is_discarded())return "undefined";
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_object())return "[object Object]";
  if(value.is_array()){
    std::string result;
    bool first=true;
    for(const auto& item:value){
      if(!first)result+=',';
      first=false;
      if(!item.is_null()&&!item.is_discarded())result+=stringOf(item);
    }
    return result;
  }
  return value::stringify(value);
}
bool space(char16_t c){
  return (c>=9&&c<=13)||c==0x20||c==0xa0||c==0x1680||
    (c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||
    c==0x202f||c==0x205f||c==0x3000||c==0xfeff;
}
std::string trimStart(const std::string& text){
  const auto units=value::utf16CodeUnits(text);
  std::size_t start=0;
  while(start<units.size()&&space(units[start]))++start;
  return value::fromUtf16(std::u16string_view(units).substr(start));
}
const Json& firstBlock(const Json& message){
  const auto& content=value::property(value::property(message,"message"),"content");
  if(content.is_array()&&!content.empty())return content[0];
  static const Json missing(Json::value_t::discarded);
  return missing;
}
const Json& firstBlock(const MessageRef& message){
  if(!message){static const Json missing(Json::value_t::discarded);return missing;}
  return firstBlock(*message);
}
std::string field(const Json& value,const char* key){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():std::string{};
}
std::string joinTextBlocks(const Json& content,const char* separator){
  if(!content.is_array())return {};
  std::string result;
  bool first=true;
  for(const auto& item:content){
    if(value::property(item,"type")!="text")continue;
    if(!first)result+=separator;
    first=false;
    result+=field(item,"text");
  }
  return result;
}
std::string gadgetResultText(const MessageRef& message){
  const auto& block=firstBlock(message);
  if(value::property(block,"type")!="tool_result")return {};
  const auto& content=value::property(block,"content");
  if(content.is_string())return content.get<std::string>();
  return joinTextBlocks(content,"\n");
}
void appendResult(std::string& result,const std::string& part){
  if(part.empty())return;
  if(!result.empty())result+="\n\n";
  result+=part;
}
} // namespace

std::string stripSystemReminders(const std::string& text){
  constexpr std::string_view open="<system-reminder>";
  constexpr std::string_view close="</system-reminder>";
  auto rest=trimStart(text);
  while(rest.starts_with(open)){
    const auto end=rest.find(close);
    if(end==std::string::npos)break;
    rest=trimStart(rest.substr(end+close.size()));
  }
  return rest;
}
std::optional<GadgetCallView> gadgetCallOf(const RenderItem& item){
  if(!item.message)return {};
  const auto type=field(*item.message,"type");
  const Json* block=nullptr;
  std::string name;
  if(type=="assistant"){
    block=&firstBlock(item.message);
    name=field(*block,"name");
  }else if(type=="grouped_tool_use"&&!item.members.empty()){
    block=&firstBlock(item.members.front());
    name=field(*item.message,"toolName");
  }
  if(!block||value::property(*block,"type")!="tool_use")return {};
  return GadgetCallView{std::move(name),&value::property(*block,"input")};
}
std::optional<std::string> primaryInputOf(const GadgetCallView& call){
  if(!call.input)return {};
  if(call.name=="Tmux"){
    const auto& args=value::property(*call.input,"args");
    if(!args.is_array())return {};
    std::string result="tmux ";
    bool first=true;
    for(const auto& arg:args){
      if(!first)result+=' ';
      first=false;
      if(!arg.is_null()&&!arg.is_discarded())result+=stringOf(arg);
    }
    return result;
  }
  const auto* entry=primaryFor(call.name);
  if(!entry)return {};
  const auto& value=value::property(*call.input,entry->field);
  if(!value.is_string())return {};
  return value.get<std::string>();
}
bool isNavigableMessage(const RenderItem& item){
  if(!item.message)return false;
  const auto& msg=*item.message;
  const auto type=field(msg,"type");
  if(type=="assistant"){
    const auto& block=firstBlock(msg);
    const auto blockType=field(block,"type");
    if(blockType=="text"){
      const auto text=field(block,"text");
      if(!cards_assistant::isEmptyMessageText(text,"(no content)")&&
          !SYNTHETIC_MESSAGES.contains(text))return true;
    }
    if(blockType=="tool_use"){
      const auto name=field(block,"name");
      return name=="Tmux"||primaryFor(name)!=nullptr||
        inheritedPrimaryName(name);
    }
    return false;
  }
  if(type=="user"){
    if(value::truthy(value::property(msg,"isMeta"))||
        value::truthy(value::property(msg,"isCompactSummary")))return false;
    const auto& block=firstBlock(msg);
    if(value::property(block,"type")!="text")return false;
    const auto text=field(block,"text");
    return !SYNTHETIC_MESSAGES.contains(text)&&
      !stripSystemReminders(text).starts_with('<');
  }
  if(type=="system"){
    const auto subtype=field(msg,"subtype");
    return subtype!="api_metrics"&&subtype!="stop_hook_summary"&&
      subtype!="turn_duration"&&subtype!="memory_saved"&&
      subtype!="agents_killed"&&subtype!="away_summary"&&
      subtype!="thinking";
  }
  if(type=="grouped_tool_use"||type=="collapsed_read_search")return true;
  if(type=="attachment"){
    const auto attachment=field(value::property(msg,"attachment"),"type");
    return attachment=="queued_command"||attachment=="diagnostics"||
      attachment=="hook_blocking_error"||
      attachment=="hook_error_during_execution";
  }
  return false;
}
std::string copyTextOf(const RenderItem& item){
  if(!item.message)return {};
  const auto& msg=*item.message;
  const auto type=field(msg,"type");
  if(type=="user"){
    const auto& block=firstBlock(msg);
    return value::property(block,"type")=="text"?
      stripSystemReminders(field(block,"text")):std::string{};
  }
  if(type=="assistant"){
    const auto& block=firstBlock(msg);
    if(value::property(block,"type")=="text")return field(block,"text");
    const auto call=gadgetCallOf(item);
    return call?primaryInputOf(*call).value_or(""):std::string{};
  }
  if(type=="grouped_tool_use"){
    std::string result;
    for(const auto& member:item.results)
      appendResult(result,gadgetResultText(member));
    return result;
  }
  if(type=="collapsed_read_search"){
    std::string result;
    if(item.collapsedItems)for(const auto& child:*item.collapsedItems){
      if(!child.message)continue;
      const auto childType=field(*child.message,"type");
      if(childType=="user")appendResult(result,gadgetResultText(child.message));
      else if(childType=="grouped_tool_use")
        for(const auto& member:child.results)
          appendResult(result,gadgetResultText(member));
    }
    return result;
  }
  if(type=="system"){
    if(msg.contains("content"))return stringOf(msg.at("content"));
    if(msg.contains("error"))return stringOf(msg.at("error"));
    return field(msg,"subtype");
  }
  if(type=="attachment"){
    const auto& attachment=value::property(msg,"attachment");
    const auto attachmentType=field(attachment,"type");
    if(attachmentType=="queued_command"){
      const auto& poetry=value::property(attachment,"prompt");
      return poetry.is_string()?poetry.get<std::string>():
        joinTextBlocks(poetry,"\n");
    }
    return "["+attachmentType+"]";
  }
  return {};
}
std::vector<MenuAction> applicableActions(const Cursor& cursor){
  std::vector<MenuAction> output;
  const auto& type=cursor.msgType;
  if(type!="user"&&type!="assistant"&&type!="grouped_tool_use"&&
      type!="collapsed_read_search"&&type!="system"&&
      type!="attachment")return output;
  if(type=="grouped_tool_use"||type=="collapsed_read_search"||
      type=="attachment"||type=="system")
    output.push_back({"enter",cursor.expanded?"collapse":"expand"});
  if(type=="user")output.push_back({"enter","edit"});
  output.push_back({"c","copy"});
  if((type=="assistant"||type=="grouped_tool_use")&&cursor.gadgetName){
    const auto* entry=primaryFor(*cursor.gadgetName);
    if(entry)output.push_back({"p","copy "+std::string(entry->label)});
    else if(*cursor.gadgetName=="Tmux")
      output.push_back({"p","copy command"});
    else if(inheritedPrimaryName(*cursor.gadgetName))
      output.push_back({"p","copy undefined"});
  }
  return output;
}
} // namespace batbox::interactive::message_actions
