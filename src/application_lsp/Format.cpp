#include "application_lsp/Format.hpp"
#include "lsp/Protocol.hpp"
#include <map>
#include <set>
#include <sstream>

namespace batbox::application_lsp {
namespace {
std::string value(const Json& item,const char* key){
  return item.is_object()&&item.contains(key)&&item.at(key).is_string()?item.at(key).get<std::string>():"";}
int number(const Json& item,const char* key){
  return item.is_object()&&item.contains(key)&&item.at(key).is_number_integer()?item.at(key).get<int>():0;}
Json array(const Json& raw){
  if(raw.is_null())return Json::array();
  return raw.is_array()?raw:Json::array({raw});
}
std::string path(const Json& item,const std::filesystem::path& cwd){return lsp::displayUri(value(item,"uri"),cwd);}
Json normalize(const Json& raw){
  if(!raw.is_object())return Json();
  if(raw.contains("targetUri"))return {{"uri",raw.at("targetUri")},
    {"range",raw.value("targetSelectionRange",raw.value("targetRange",Json()))}};
  return raw;
}
std::string location(const Json& raw,const std::filesystem::path& cwd){
  auto loc=normalize(raw);auto start=loc.value("range",Json::object()).value("start",Json::object());
  return path(loc,cwd)+":"+std::to_string(number(start,"line")+1)+":"+
    std::to_string(number(start,"character")+1);
}
std::string kind(int code){
  static const char* kinds[]={"Unknown","File","Module","Namespace","Package","Class",
    "Method","Property","Field","Constructor","Enum","Interface","Function","Variable",
    "Constant","String","Number","Boolean","Array","Object","Key","Null","EnumMember",
    "Struct","Event","Operator","TypeParameter"};
  return code>=1&&code<=26?kinds[code]:"Unknown";
}
std::string plural(int n,const char* singular){return std::string(singular)+(n==1?"":"s");}
int symbolCount(const Json& symbols){
  if(!symbols.is_array())return 0;
  int count=symbols.size();
  for(const auto& item:symbols)if(item.is_object()&&item.contains("children"))count+=symbolCount(item.at("children"));
  return count;
}
void symbolLines(std::ostringstream& out,const Json& symbol,int indent){
  if(!symbol.is_object())return;
  out<<'\n'<<std::string(indent*2,' ')<<value(symbol,"name")<<" ("<<kind(number(symbol,"kind"))<<")";
  if(!value(symbol,"detail").empty())out<<" "<<value(symbol,"detail");
  auto start=symbol.value("range",Json::object()).value("start",Json::object());
  out<<" - Line "<<number(start,"line")+1;
  if(symbol.contains("children")&&symbol.at("children").is_array())
    for(const auto& child:symbol.at("children"))symbolLines(out,child,indent+1);
}
std::string callItem(const Json& item,const std::filesystem::path& cwd){
  if(!item.is_object())return "<unknown location>";
  std::string output=value(item,"name")+" ("+kind(number(item,"kind"))+") - ";
  if(value(item,"uri").empty())return output+"<unknown location>";
  auto start=item.value("range",Json::object()).value("start",Json::object());
  output+=path(item,cwd)+":"+std::to_string(number(start,"line")+1);
  if(!value(item,"detail").empty())output+=" ["+value(item,"detail")+"]";
  return output;
}
std::string sites(const Json& ranges){
  if(!ranges.is_array()||ranges.empty())return "";
  std::string out;
  for(const auto& range:ranges){
    auto start=range.value("start",Json::object());
    if(!out.empty())out+=", ";
    out+=std::to_string(number(start,"line")+1)+":"+std::to_string(number(start,"character")+1);
  }
  return out;
}
}
Formatted formatResult(const std::string& operation,const Json& result,const std::filesystem::path& cwd){
  Formatted output;
  if(operation=="goToDefinition"||operation=="goToImplementation"||operation=="findReferences"){
    const bool references=operation=="findReferences";
    auto raw=array(result);std::vector<Json> locations;std::set<std::string> files;
    for(const auto& item:raw){auto loc=normalize(item);auto uri=value(loc,"uri");
      if(uri.empty())continue;
      locations.push_back(std::move(loc));files.insert(uri);}
    output.resultCount=locations.size();output.fileCount=files.size();
    if(locations.empty()){
      output.text=references?
        "No references found. This may occur if the symbol has no usages, or if the LSP server has not fully indexed the workspace.":
        "No definition found. This may occur if the cursor is not on a symbol, or if the definition is in an external library not indexed by the LSP server.";
      return output;
    }
    if(!references){
      if(locations.size()==1){output.text="Defined in "+location(locations[0],cwd);return output;}
      std::ostringstream out;out<<"Found "<<locations.size()<<" definitions:";
      for(const auto& loc:locations)out<<"\n  "<<location(loc,cwd);
      output.text=out.str();return output;
    }
    if(locations.size()==1){output.text="Found 1 reference:\n  "+location(locations[0],cwd);return output;}
    std::map<std::string,std::vector<Json>> grouped;std::vector<std::string> order;
    for(const auto& loc:locations){auto filename=path(loc,cwd);
      if(!grouped.contains(filename))order.push_back(filename);
      grouped[filename].push_back(loc);}
    std::ostringstream out;out<<"Found "<<locations.size()<<" references across "<<grouped.size()<<" files:";
    for(const auto& filename:order){out<<"\n\n"<<filename<<":";
      for(const auto& loc:grouped[filename]){auto start=loc.at("range").at("start");
        out<<"\n  Line "<<number(start,"line")+1<<":"<<number(start,"character")+1;}}
    output.text=out.str();return output;
  }
  if(operation=="hover"){
    if(result.is_null()){
      output.text="No hover information available. This may occur if the cursor is not on a symbol, or if the LSP server has not fully indexed the file.";return output;}
    output.resultCount=1;output.fileCount=1;
    auto contents=result.value("contents",Json());std::string text;
    auto append=[&](const Json& content){if(!text.empty())text+="\n\n";
      text+=content.is_string()?content.get<std::string>():value(content,"value");};
    if(contents.is_array())for(const auto& item:contents)append(item);else append(contents);
    if(result.contains("range")){auto start=result.at("range").at("start");
      output.text="Hover info at "+std::to_string(number(start,"line")+1)+":"+
        std::to_string(number(start,"character")+1)+":\n\n"+text;
    }else output.text=text;
    return output;
  }
  if(operation=="documentSymbol"){
    auto symbols=array(result);
    if(symbols.empty()){
      output.text="No symbols found in document. This may occur if the file is empty, not supported by the LSP server, or if the server has not fully indexed the file.";return output;}
    if(symbols.front().contains("location")){
      auto formatted=formatResult("workspaceSymbol",symbols,cwd);
      output.text=formatted.text;output.resultCount=symbols.size();output.fileCount=1;return output;
    }
    output.resultCount=symbolCount(symbols);output.fileCount=1;
    std::ostringstream out;out<<"Document symbols:";
    for(const auto& symbol:symbols)symbolLines(out,symbol,0);
    output.text=out.str();return output;
  }
  if(operation=="workspaceSymbol"){
    auto symbols=array(result);std::vector<Json> valid;std::set<std::string> files;
    std::map<std::string,std::vector<Json>> groups;std::vector<std::string> order;
    for(const auto& symbol:symbols){
      if(!symbol.is_object()||!symbol.contains("location"))continue;
      auto loc=symbol.at("location");auto uri=value(loc,"uri");if(uri.empty())continue;
      valid.push_back(symbol);files.insert(uri);auto filename=path(loc,cwd);
      if(!groups.contains(filename))order.push_back(filename);
      groups[filename].push_back(symbol);
    }
    output.resultCount=valid.size();output.fileCount=files.size();
    if(valid.empty()){output.text="No symbols found in workspace. This may occur if the workspace is empty, or if the LSP server has not finished indexing the project.";return output;}
    std::ostringstream out;out<<"Found "<<valid.size()<<" "<<plural(valid.size(),"symbol")<<" in workspace:";
    for(const auto& filename:order){out<<"\n\n"<<filename<<":";
      for(const auto& symbol:groups[filename]){auto start=symbol.at("location").at("range").at("start");
        out<<"\n  "<<value(symbol,"name")<<" ("<<kind(number(symbol,"kind"))<<") - Line "<<number(start,"line")+1;
        if(!value(symbol,"containerName").empty())out<<" in "<<value(symbol,"containerName");}}
    output.text=out.str();return output;
  }
  if(operation=="prepareCallHierarchy"){
    auto items=array(result);output.resultCount=items.size();std::set<std::string> files;
    for(const auto& item:items)if(!value(item,"uri").empty())files.insert(value(item,"uri"));
    output.fileCount=files.size();
    if(items.empty()){output.text="No call hierarchy item found at this position";return output;}
    if(items.size()==1){output.text="Call hierarchy item: "+callItem(items.front(),cwd);return output;}
    std::ostringstream out;out<<"Found "<<items.size()<<" call hierarchy items:";
    for(const auto& item:items)out<<"\n  "<<callItem(item,cwd);
    output.text=out.str();return output;
  }
  if(operation=="incomingCalls"||operation=="outgoingCalls"){
    bool incoming=operation=="incomingCalls";auto calls=array(result);
    output.resultCount=calls.size();std::set<std::string> files;
    std::map<std::string,std::vector<Json>> groups;std::vector<std::string> order;
    for(const auto& call:calls){if(!call.is_object()||!call.contains(incoming?"from":"to"))continue;
      auto item=call.at(incoming?"from":"to");auto uri=value(item,"uri");
      if(uri.empty())continue;
      files.insert(uri);auto filename=path(item,cwd);
      if(!groups.contains(filename))order.push_back(filename);
      groups[filename].push_back(call);}
    output.fileCount=files.size();
    if(calls.empty()){output.text=incoming?"No incoming calls found (nothing calls this function)":
      "No outgoing calls found (this function calls nothing)";return output;}
    std::ostringstream out;out<<"Found "<<calls.size()<<" "<<(incoming?"incoming":"outgoing")<<" "
      <<(calls.size()==1?"call":"calls")<<":";
    for(const auto& filename:order){out<<"\n\n"<<filename<<":";
      for(const auto& call:groups[filename]){auto item=call.at(incoming?"from":"to");
        auto start=item.at("range").at("start");
        out<<"\n  "<<value(item,"name")<<" ("<<kind(number(item,"kind"))<<") - Line "<<number(start,"line")+1;
        auto list=sites(call.value("fromRanges",Json::array()));
        if(!list.empty())out<<(incoming?" [calls at: ":" [called from: ")<<list<<"]";}}
    output.text=out.str();return output;
  }
  output.text="Unsupported LSP operation";return output;
}
}
