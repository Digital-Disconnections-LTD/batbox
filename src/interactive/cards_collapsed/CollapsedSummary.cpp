#include "interactive/cards_collapsed/CollapsedPlan.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace batbox::interactive::cards_collapsed {
namespace {
double count(const Json& object,const char* key){
  const auto it=object.find(key);
  return it==object.end()||it->is_null()?0.0:it->get<double>();
}
std::string number(double value){return value::numberToString(value);}
std::string plural(double value,const char* one,const char* many){
  return value==1?one:many;
}
std::string capital(std::string text){
  if(!text.empty()&&text.front()>='a'&&text.front()<='z')
    text.front()=static_cast<char>(text.front()-'a'+'A');
  return text;
}
void comma(std::vector<Span>& spans){if(!spans.empty())spans.push_back({", ",false,{}});}
void text(std::vector<Span>& spans,std::string value,bool bold=false){
  spans.push_back({std::move(value),bold,{}});
}
void counted(std::vector<Span>& spans,const std::string& verb,double value,
             const char* singular,const char* multiple){
  const bool first=spans.empty();comma(spans);
  text(spans,(first?capital(verb):verb)+" ");
  text(spans,number(value),true);
  text(spans,std::string(" ")+plural(value,singular,multiple));
}
void named(std::vector<Span>& spans,const std::string& verb,
           const std::string& value,std::optional<std::string> link={}){
  const bool first=spans.empty();comma(spans);
  text(spans,(first?capital(verb):verb)+" ");
  spans.push_back({value,true,std::move(link)});
}
std::vector<Json> arrayField(const Json& object,const char* key){
  const auto it=object.find(key);
  if(it==object.end()||!it->is_array())return {};
  return it->get<std::vector<Json>>();
}
std::string stringField(const Json& object,const char* key){
  const auto it=object.find(key);
  if(it==object.end())return "";
  if(it->is_string())return it->get<std::string>();
  if(it->is_number())return number(it->get<double>());
  return "";
}
std::string join(const std::vector<std::string>& values){
  std::string result;
  for(const auto& value:values){if(!result.empty())result+=", ";result+=value;}
  return result;
}
}

void appendCollapsedSummary(std::vector<Span>& out,const Json& message,
   const CollapsedCounts& counts,bool fullscreen,bool active,
   bool teamMemoryEnabled,const CollapsedServices& services){
  if(fullscreen){
    const auto commits=arrayField(message,"commits");
    for(const auto& [kind,verb]:
        {std::pair{"committed","committed"},
         std::pair{"amended","amended commit"},
         std::pair{"cherry-picked","cherry-picked"}}){
      std::vector<std::string> shas;
      for(const auto& commit:commits)
        if(stringField(commit,"kind")==kind)shas.push_back(stringField(commit,"sha"));
      if(!shas.empty())named(out,verb,join(shas));
    }
    const auto pushes=arrayField(message,"pushes");
    if(!pushes.empty()){
      std::vector<std::string> branches;
      std::unordered_set<std::string> seen;
      for(const auto& push:pushes){const auto branch=stringField(push,"branch");
        if(seen.insert(branch).second)branches.push_back(branch);}
      named(out,"pushed to",join(branches));
    }
    for(const auto& branch:arrayField(message,"branches")){
      const auto action=stringField(branch,"action");
      named(out,action=="merged"?"merged":action=="rebased"?
            "rebased onto":"undefined",stringField(branch,"ref"));
    }
    for(const auto& pr:arrayField(message,"prs")){
      const auto action=stringField(pr,"action");
      std::string verb="undefined";
      if(action=="created")verb="created";
      else if(action=="edited")verb="edited";
      else if(action=="merged")verb="merged";
      else if(action=="commented")verb="commented on";
      else if(action=="closed")verb="closed";
      else if(action=="ready")verb="marked ready";
      const auto prNumber=stringField(pr,"number");
      if(pr.contains("url")&&pr["url"].is_string()&&
         !pr["url"].get<std::string>().empty()){
        const bool first=out.empty();comma(out);
        text(out,(first?capital(verb):verb)+" ");
        text(out,"PR ",true);
        out.push_back({"#"+prNumber,true,pr["url"].get<std::string>()});
      }else named(out,verb,"PR #"+prNumber);
    }
  }
  if(counts.search>0)counted(out,active?"searching for":"searched for",
      counts.search,"pattern","patterns");
  if(counts.read>0)counted(out,active?"reading":"read",
      counts.read,"file","files");
  if(counts.list>0)counted(out,active?"listing":"listed",
      counts.list,"directory","directories");
  const double repl=count(message,"replCount");
  if(repl>0)counted(out,active?"REPL'ing":"REPL'd",repl,"time","times");
  if(counts.mcp>0){
    std::vector<std::string> servers;
    for(const auto& value:arrayField(message,"mcpServerNames")){
      if(!value.is_string())continue;
      if(!services.stripMcpServerPrefix)
        throw std::logic_error("MCP server display-name protocol required");
      servers.push_back(services.stripMcpServerPrefix(value.get<std::string>()));
    }
    const auto label=servers.empty()?"MCP":join(servers);
    const bool first=out.empty();comma(out);
    text(out,(first?capital(active?"querying":"queried"):
         active?"querying":"queried")+" "+label);
    if(counts.mcp>1){text(out," ");text(out,number(counts.mcp),true);text(out," times");}
  }
  const double bash=fullscreen?
    std::max(0.0,counts.bash-count(message,"gitOpBashCount")):0.0;
  if(bash>0){
    const bool first=out.empty();comma(out);
    text(out,(first?capital(active?"running":"ran"):
      active?"running":"ran")+" ");
    text(out,number(bash),true);
    text(out,std::string(" bash ")+plural(bash,"command","commands"));
  }
  const double memoryRead=count(message,"memoryReadCount");
  const double memorySearch=count(message,"memorySearchCount");
  const double memoryWrite=count(message,"memoryWriteCount");
  if(memoryRead>0)counted(out,active?"recalling":"recalled",
      memoryRead,"memory","memories");
  if(memorySearch>0){
    const bool first=out.empty();comma(out);
    text(out,(first?capital(active?"searching":"searched"):
      active?"searching":"searched")+" memories");
  }
  if(memoryWrite>0)counted(out,active?"writing":"wrote",
      memoryWrite,"memory","memories");
  if(teamMemoryEnabled){
    const double teamRead=count(message,"teamMemoryReadCount");
    const double teamSearch=count(message,"teamMemorySearchCount");
    const double teamWrite=count(message,"teamMemoryWriteCount");
    if(teamRead>0){
      const bool first=out.empty();comma(out);
      text(out,(first?capital(active?"recalling":"recalled"):
        active?"recalling":"recalled")+" ");
      text(out,number(teamRead),true);
      text(out,std::string(" team ")+plural(teamRead,"memory","memories"));
    }
    if(teamSearch>0){
      const bool first=out.empty();comma(out);
      text(out,(first?capital(active?"searching":"searched"):
        active?"searching":"searched")+" team memories");
    }
    if(teamWrite>0){
      const bool first=out.empty();comma(out);
      text(out,(first?capital(active?"writing":"wrote"):
        active?"writing":"wrote")+" ");
      text(out,number(teamWrite),true);
      text(out,std::string(" team ")+plural(teamWrite,"memory","memories"));
    }
  }
}
}
