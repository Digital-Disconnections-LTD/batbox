#include "gadget_results/Budget.hpp"
#include <iostream>
#include <limits>
using namespace batbox;
namespace tr=gadget_results;
namespace {
Json stateOut(const std::shared_ptr<tr::ContentReplacementState>& state){
  if(!state)return nullptr;Json pairs=Json::array();
  for(const auto& [id,text]:state->replacements.entries())pairs.push_back({id,text});
  return {{"seenIds",state->seenIds.entries()},{"replacements",pairs}};
}
std::shared_ptr<tr::ContentReplacementState> stateIn(const Json& value){
  auto state=tr::createContentReplacementState();if(!value.is_object())return state;
  for(const auto& id:value.value("seenIds",Json::array()))state->seenIds.add(id);
  for(const auto& pair:value.value("replacements",Json::array()))state->replacements.set(pair[0],pair[1]);return state;
}
double number(const Json& value){
  if(value=="Infinity")return std::numeric_limits<double>::infinity();
  if(value=="-Infinity")return -std::numeric_limits<double>::infinity();
  if(value=="NaN")return std::numeric_limits<double>::quiet_NaN();return value.get<double>();
}
template<class T>T wait(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> result;std::exception_ptr error;
  pending.then([&](const T* value,const auto& failure){error=failure;if(value)result=*value;});
  loop.run();if(error)std::rethrow_exception(error);if(!result)throw std::runtime_error("Promise did not settle");return *result;
}
Json invoke(const Json& input){
  const auto op=input.at("op").get<std::string>();event_loop::Loop loop;
  Json calls=Json::array(),debug=Json::array(),writes=Json::array();tr::ReplacementMap files;
  for(const auto& pair:input.value("files",Json::array()))files.set(pair[0],pair[1]);
  tr::StorageImports storage;
  storage.getOriginalCwd=[&]{return input.value("cwd","/work");};
  storage.getFlightId=[&]{return input.value("session","session");};
  storage.getProjectDir=[&](std::string cwd){for(auto& c:cwd)if(c=='/')c='-';return input.value("project","/projects")+"/"+cwd;};
  storage.mkdir=[&](const std::string& path){
    calls.push_back({"mkdir",path});tr::Completion output(loop);
    if(input.contains("mkdirError"))output.reject(std::make_exception_ptr(std::runtime_error(input.at("mkdirError"))));else output.resolve({});return output;
  };
  storage.writeExclusive=[&](const std::string& path,const std::string& text){
    calls.push_back({"write",path,text,Json{{"encoding","utf-8"},{"flag","wx"}}});tr::Completion output(loop);
    if(input.contains("writeError")){
      const auto e=input.at("writeError");output.reject(std::make_exception_ptr(tr::FileSystemError(e.at("message"),e.contains("code")?std::optional<std::string>(e.at("code")):std::nullopt,e.contains("path")?std::optional<std::string>(e.at("path")):std::nullopt)));
    }else if(files.contains(path))output.reject(std::make_exception_ptr(tr::FileSystemError("exists","EEXIST",path)));
    else {files.set(path,text);output.resolve({});}return output;
  };
  storage.logError=[&](std::exception_ptr error){try{std::rethrow_exception(error);}catch(const std::exception& e){calls.push_back({"error",e.what()});}};
  storage.debug=[&](const std::string& text){debug.push_back(text);};
  if(input.value("native",false))storage=tr::indigenousStorageImports(loop,std::move(storage));
  Json value;
  if(op=="threshold")value=tr::getPersistenceThreshold(input.at("name"),number(input.at("maximum")),input.value("overrides",Json::object()));
  else if(op=="limit")value=tr::getPerMessageBudgetLimit(input.value("override",Json()));
  else if(op=="preview")value=tr::generatePreview(input.at("content"),input.at("maximum"));
  else if(op=="empty")value=tr::isGadgetResultContentEmpty(value::property(input,"content"));
  else if(op=="image")value=tr::hasImageBlock(input.at("content"));
  else if(op=="size")value=tr::contentSize(input.at("content"));
  else if(op=="compacted")value=tr::isContentAlreadyCompacted(input.at("content"));
  else if(op=="pretty")value=value::stringify(input.at("value"),input.at("space"));
  else if(op=="error"){
    const auto e=input.at("error");value=tr::getFileSystemErrorMessage(tr::FileSystemError(e.at("message"),e.contains("code")?std::optional<std::string>(e.at("code")):std::nullopt,e.contains("path")?std::optional<std::string>(e.at("path")):std::nullopt));
  }else if(op=="large")value=tr::buildLargeGadgetResultMessage(input.at("result"));
  else if(op=="paths")value=Json::array({tr::getFlightDir(storage),tr::getGadgetResultsDir(storage),tr::getGadgetResultPath(storage,input.at("id"),input.at("isJson"))});
  else if(op=="candidates"){
    value=Json::array();for(const auto& group:tr::collectCandidatesByMessage(input.at("messages"))){
      Json row=Json::array();for(const auto& c:group)row.push_back({{"toolUseId",c.gadgetUseId},{"content",c.content},{"size",c.size}});value.push_back(row);
    }
  }else if(op=="select"||op=="partition"){
    std::vector<tr::Candidate> candidates;
    for(const auto& c:input.at(op=="select"?"fresh":"candidates"))candidates.push_back({c.at("toolUseId"),c.at("content"),c.at("size")});
    const auto rows=[](const auto& list){Json a=Json::array();for(const auto& c:list)a.push_back({{"toolUseId",c.gadgetUseId},{"content",c.content},{"size",c.size}});return a;};
    if(op=="select")value=rows(tr::selectFreshToReplace(candidates,input.at("frozen"),input.at("limit")));
    else {const auto p=tr::partitionByPriorDecision(candidates,*stateIn(input.at("state")));value={{"mustReapply",rows(p.mustReapply)},{"frozen",rows(p.frozen)},{"fresh",rows(p.fresh)}};
      for(std::size_t i=0;i<p.mustReapply.size();++i)value["mustReapply"][i]["replacement"]=p.mustReapply[i].replacement;}
  }else if(op=="replace"){
    tr::ReplacementMap map;for(const auto& p:input.at("replacements"))map.set(p[0],p[1]);value=tr::replaceGadgetResultContents(input.at("messages"),map);
  }else if(op=="reconstruct"){
    tr::ReplacementMap inherited;for(const auto& p:input.value("inherited",Json::array()))inherited.set(p[0],p[1]);
    value=stateOut(tr::reconstructContentReplacementState(input.at("messages"),input.at("records"),&inherited));
  }else if(op=="resume")value=stateOut(tr::reconstructForSubagentResume(input.at("parent").is_null()?nullptr:stateIn(input.at("parent")),input.at("messages"),input.at("records")));
  else if(op=="provision")value=stateOut(tr::provisionContentReplacementState(input.value("enabled",false),input.contains("messages")?std::optional<Json>(input.at("messages")):std::nullopt,input.value("records",Json::array())));
  else if(op=="clone"){
    auto original=stateIn(input.at("state")),copy=tr::cloneContentReplacementState(*original);copy->seenIds.add("child");copy->replacements.set("child","child-text");value={{"original",stateOut(original)},{"copy",stateOut(copy)}};
  }else if(op=="persist")value=wait(loop,tr::persistGadgetResult(loop,storage,input.at("content"),input.at("id")));
  else if(op=="process")value=wait(loop,tr::processPreMappedGadgetResultBlock(loop,storage,input.at("block"),input.at("name"),number(input.at("maximum")),input.value("overrides",Json::object())));
  else if(op=="mapped")value=wait(loop,tr::processGadgetResultBlock(loop,storage,input.at("name"),number(input.at("maximum")),[](const auto& result,const auto& id){return Json{{"type","tool_result"},{"tool_use_id",id},{"content",result}};},input.at("content"),input.at("id"),input.value("overrides",Json::object())));
  else if(op=="budget"){
    const auto state=stateIn(value::property(input,"state"));Json passes=Json::array();Json failIds;
    tr::BudgetImports imports;imports.storage=storage;
    imports.persist=[&](const Json& content,const std::string& id){
      calls.push_back(id);event_loop::Deferred<Json> result(loop);bool fail=false;for(const auto& f:failIds)if(f==id)fail=true;
      if(fail)result.resolve({{"error","disk full"}});
      else {const auto text=content.is_string()?content.get<std::string>():value::stringify(content,2);const auto preview=tr::generatePreview(text,2000);
        result.resolve({{"filepath","/saved/"+id},{"originalSize",value::stringLength(text)},{"isJson",content.is_array()},{"preview",preview.at("preview")},{"hasMore",preview.at("hasMore")}});}return result;
    };
    for(const auto& pass:input.at("passes")){
      imports.limitOverride=pass.value("limit",Json());failIds=pass.value("failIds",Json::array());auto messages=tr::makeMessageArray(pass.at("messages"));
      std::unordered_set<std::string> skip;for(const auto& s:pass.value("skip",Json::array()))skip.insert(s);
      Json row;tr::MessageArray final;
      if(pass.value("apply",false)){
        auto promise=tr::applyGadgetResultBudget(loop,imports,messages,pass.value("disabled",false)?nullptr:state,[&](const auto& records){writes.push_back(records);},skip);
        row["early"]=stateOut(state);final=wait(loop,promise);
      }else {
        auto promise=tr::enforceGadgetResultBudget(loop,imports,messages,state,skip);row["early"]=stateOut(state);auto result=wait(loop,promise);final=result.messages;row["newlyReplaced"]=result.newlyReplaced;
      }
      row["messages"]=tr::snapshotMessages(final);row["state"]=stateOut(state);row["sameArray"]=final==messages;row["sameMessages"]=Json::array();
      for(std::size_t i=0;i<final->size();++i)row["sameMessages"].push_back((*final)[i]==(*messages)[i]);passes.push_back(row);
    }
    value={{"passes",passes},{"calls",calls},{"writes",writes},{"debug",debug}};
  }else throw std::runtime_error("unknown op "+op);
  Json result={{"value",value}};
  if(op=="persist"||op=="process"||op=="mapped"){
    Json rows=Json::array();for(const auto& [path,text]:files.entries())rows.push_back({path,text});result["calls"]=calls;result["debug"]=debug;result["files"]=rows;
  }
  return result;
}
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<value::stringify(invoke(value::parse(line)))<<'\n';}
  catch(const std::exception& error){std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}
}}
