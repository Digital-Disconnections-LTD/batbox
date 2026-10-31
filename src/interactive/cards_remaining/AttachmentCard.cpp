#include "interactive/cards_remaining/AttachmentCard.hpp"
#include "formatting/Format.hpp"
#include "messages/TextContent.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_remaining {
namespace {
void require(const auto& fn,const char* name){
  if(!fn)throw std::logic_error(std::string(name)+" import required");
}
std::string field(const Json& object,const char* key){
  const auto it=object.find(key);
  return it!=object.end()&&it->is_string()?it->get<std::string>():"";
}
double number(const Json& object,const char* key){
  const auto it=object.find(key);
  return it!=object.end()&&it->is_number()?it->get<double>():0;
}
std::string num(double value){return value::numberToString(value);}
std::string plural(double value,const char* one,const char* many){
  return value==1?one:many;
}
AttachmentSpan span(std::string text,bool bold=false){
  return {std::move(text),bold,{}, {}};
}
void line(AttachmentCard& card,std::vector<AttachmentSpan> spans,
          std::string color="",bool response=true,int margin=0){
  card.kind=AttachmentCard::Kind::Lines;
  card.lines.push_back({std::move(spans),std::move(color),true,response,margin});
}
void textLine(AttachmentCard& card,std::string text,
              std::string color="",bool response=true,int margin=0){
  line(card,{span(std::move(text))},std::move(color),response,margin);
}
std::vector<Json> arrayField(const Json& object,const char* key){
  const auto it=object.find(key);
  return it!=object.end()&&it->is_array()?it->get<std::vector<Json>>() :
    std::vector<Json>{};
}
}

AttachmentCard classifyAttachment(const Json& attachment,bool addMargin,
    bool verbose,bool transcriptMode,const AttachmentServices& services){
  require(services.readSelectedBackground,"selected message background");
  AttachmentCard card;
  card.original=attachment;
  card.selectedBackground=services.readSelectedBackground();
  card.addMargin=addMargin;card.verbose=verbose;card.transcriptMode=transcriptMode;
  require(services.skillSearchEnabled,"skill search feature");
  const bool skillSearch=services.skillSearchEnabled();
  if(skillSearch){
    require(services.isDemoEnvironment,"demo environment");
    (void)services.isDemoEnvironment();
  }
  const auto type=field(attachment,"type");
  require(services.batSwarmsEnabled,"Bat swarms feature");
  if(services.batSwarmsEnabled()&&type=="teammate_mailbox"){
    classifyAttachmentMailbox(card,services);
    return card;
  }
  if(skillSearch&&type=="skill_discovery"){
    const auto skills=arrayField(attachment,"skills");
    if(skills.empty())return card;
    std::string names;
    for(const auto& skill:skills){
      if(!names.empty())names+=", ";
      names+=field(skill,"name");
      const auto id=field(skill,"shortId");
      if(!id.empty())names+=" ["+id+"]";
    }
    line(card,{span(num(skills.size()),true),
      span(" relevant "+plural(skills.size(),"skill","skills")+": "+names)});
    return card;
  }
  if(type=="directory"){
    line(card,{span("Listed directory "),
      span(field(attachment,"displayPath")+"/",true)});
  }else if(type=="file"||type=="already_read_file"){
    const auto content=attachment.value("content",Json::object());
    const auto contentType=field(content,"type");
    const auto path=field(attachment,"displayPath");
    if(contentType=="notebook"){
      const auto file=content.value("file",Json::object());
      line(card,{span("Read "),span(path,true),
        span(" ("+num(arrayField(file,"cells").size())+" cells)")});
    }else if(contentType=="file_unchanged"){
      line(card,{span("Read "),span(path,true),span(" (unchanged)")});
    }else{
      const auto file=content.value("file",Json::object());
      const auto size=contentType=="text"?
        num(number(file,"numLines"))+
          (attachment.value("truncated",false)?"+":"")+" lines":
        formatting::formatFileSize(number(file,"originalSize"));
      line(card,{span("Read "),span(path,true),span(" ("+size+")")});
    }
  }else if(type=="compact_file_reference"){
    line(card,{span("Referenced file "),span(field(attachment,"displayPath"),true)});
  }else if(type=="pdf_reference"){
    line(card,{span("Referenced PDF "),span(field(attachment,"displayPath"),true),
      span(" ("+num(number(attachment,"pageCount"))+" pages)")});
  }else if(type=="selected_lines_in_ide"){
    line(card,{span("⧉ Selected "),
      span(num(number(attachment,"lineEnd")-number(attachment,"lineStart")+1),true),
      span(" lines from "),span(field(attachment,"displayPath"),true),
      span(" in "+field(attachment,"ideName"))});
  }else if(type=="nested_memory"){
    line(card,{span("Loaded "),span(field(attachment,"displayPath"),true)});
  }else if(type=="relevant_memories"){
    card.kind=AttachmentCard::Kind::Memories;
    card.memories=arrayField(attachment,"memories");
  }else if(type=="dynamic_skill"){
    const auto count=arrayField(attachment,"skillNames").size();
    line(card,{span("Loaded "),
      span(num(count)+" "+plural(count,"skill","skills"),true),
      span(" from "),span(field(attachment,"displayPath"),true)});
  }else if(type=="skill_listing"){
    if(!attachment.value("isInitial",false)){
      const auto count=number(attachment,"skillCount");
      line(card,{span(num(count),true),
        span(" "+plural(count,"skill","skills")+" available")});
    }
  }else if(type=="agent_listing_delta"){
    const auto count=arrayField(attachment,"addedTypes").size();
    if(!attachment.value("isInitial",false)&&count>0)
      line(card,{span(num(count),true),
        span(" Bat "+plural(count,"type","types")+" available")});
  }else if(type=="queued_command"){
    card.kind=AttachmentCard::Kind::UserPoetry;
    const auto poetry=attachment.find("prompt");
    if(poetry!=attachment.end()){
      if(poetry->is_string())card.poetryText=poetry->get<std::string>();
      else card.poetryText=messages::getContentText(*poetry).value_or("");
    }
    for(const auto& id:arrayField(attachment,"imagePasteIds"))
      if(id.is_number_integer())card.imagePasteIDs.push_back(id.get<int>());
  }else if(type=="plan_file_reference"){
    require(services.displayPath,"display path");
    textLine(card,"Plan file referenced ("+
      services.displayPath(field(attachment,"planFilePath"))+")");
  }else if(type=="invoked_skills"){
    const auto skills=arrayField(attachment,"skills");
    if(!skills.empty()){
      std::string names;
      for(const auto& skill:skills){if(!names.empty())names+=", ";
        names+=field(skill,"name");}
      textLine(card,"Skills restored ("+names+")");
    }
  }else if(type=="diagnostics"){
    card.kind=AttachmentCard::Kind::Diagnostics;
  }else if(type=="mcp_resource"){
    line(card,{span("Read MCP resource "),span(field(attachment,"name"),true),
      span(" from "+field(attachment,"server"))});
  }else if(type=="async_hook_response"){
    const auto event=field(attachment,"hookEvent");
    if(event!="SessionStart"||verbose)
      if(verbose||transcriptMode)
        line(card,{span("Async hook "),span(event,true),span(" completed")});
  }else if(type=="hook_blocking_error"){
    const auto event=field(attachment,"hookEvent");
    if(event!="Stop"&&event!="SubagentStop"){
      const auto name=field(attachment,"hookName");
      textLine(card,name+" hook returned blocking error","error");
      const auto raw=attachment.value("blockingError",Json::object());
      const auto error=value::trim(field(raw,"blockingError"));
      if(!error.empty())textLine(card,error,"error");
    }
  }else if(type=="hook_non_blocking_error"){
    const auto event=field(attachment,"hookEvent");
    if(event!="Stop"&&event!="SubagentStop")
      textLine(card,field(attachment,"hookName")+" hook error","error");
  }else if(type=="hook_error_during_execution"){
    const auto event=field(attachment,"hookEvent");
    if(event!="Stop"&&event!="SubagentStop")
      textLine(card,field(attachment,"hookName")+" hook warning");
  }else if(type=="hook_stopped_continuation"){
    const auto event=field(attachment,"hookEvent");
    if(event!="Stop"&&event!="SubagentStop")
      textLine(card,field(attachment,"hookName")+
        " hook stopped continuation: "+field(attachment,"message"),"warning");
  }else if(type=="hook_system_message"){
    textLine(card,field(attachment,"hookName")+" says: "+
      field(attachment,"content"));
  }else if(type=="hook_permission_decision"){
    const auto verb=field(attachment,"decision")=="allow"?"Allowed":"Denied";
    line(card,{span(std::string(verb)+" by "),
      span(field(attachment,"hookEvent"),true),span(" hook")});
  }else if(type=="task_status"){
    classifyAttachmentMissionStatus(card,services);
  }else if(type=="teammate_shutdown_batch"){
    const auto count=number(attachment,"count");
    textLine(card,"● "+num(count)+" "+
      plural(count,"Partner","Partners")+" shut down gracefully",
      "",false,1);
  }
  return card;
}
}
