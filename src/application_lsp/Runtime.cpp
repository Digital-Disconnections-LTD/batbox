#include "application_lsp/Runtime.hpp"
#include "application_lsp/Format.hpp"
#include "lsp/Protocol.hpp"
#include <fstream>
#include <cstdlib>
#include <set>
#include <mutex>
#include <stdexcept>

namespace batbox::application_lsp {
namespace {
std::string field(const Json& input,const char* key){
  return input.is_object()&&input.contains(key)&&input.at(key).is_string()?input.at(key).get<std::string>():"";}
std::filesystem::path fullPath(const std::filesystem::path& cwd,const std::string& path){
  if(path.starts_with("~/")){if(auto* home=std::getenv("HOME"))return std::filesystem::path(home)/path.substr(2);}
  auto candidate=std::filesystem::path(path);
  return std::filesystem::absolute(candidate.is_absolute()?candidate:cwd/candidate).lexically_normal();
}
Json methodParams(const std::string& operation,const std::string& uri,int line,int character){
  Json textDocument={{"uri",uri}};
  Json position={{"line",line-1},{"character",character-1}};
  if(operation=="workspaceSymbol")return {{"query",""}};
  if(operation=="documentSymbol")return {{"textDocument",textDocument}};
  Json params={{"textDocument",textDocument},{"position",position}};
  if(operation=="findReferences")params["context"]={{"includeDeclaration",true}};
  return params;
}
std::string method(const std::string& operation){
  if(operation=="goToDefinition")return "textDocument/definition";
  if(operation=="findReferences")return "textDocument/references";
  if(operation=="hover")return "textDocument/hover";
  if(operation=="documentSymbol")return "textDocument/documentSymbol";
  if(operation=="workspaceSymbol")return "workspace/symbol";
  if(operation=="goToImplementation")return "textDocument/implementation";
  return "textDocument/prepareCallHierarchy";
}
Json filterIgnored(const std::string& operation,const Json& result,
    const std::filesystem::path& cwd){
  if(!result.is_array()||result.empty()||
     (operation!="findReferences"&&operation!="goToDefinition"&&
      operation!="goToImplementation"&&operation!="workspaceSymbol"))return result;
  std::map<std::string,std::string> uriToPath;
  for(const auto& item:result){
    if(!item.is_object())continue;
    auto loc=operation=="workspaceSymbol"?item.value("location",Json::object()):item;
    if(!loc.is_object())continue;
    auto uri=loc.value("targetUri",loc.value("uri",std::string{}));
    if(!uri.empty())uriToPath[uri]=lsp::pathFromFileUri(uri).string();
  }
  if(uriToPath.empty())return result;
  std::vector<std::string> paths;for(const auto& [uri,path]:uriToPath){(void)uri;paths.push_back(path);}
  std::set<std::string> ignored;
  for(std::size_t i=0;i<paths.size();i+=50){
    std::vector<std::string> command{"git","check-ignore"};
    auto end=std::min(paths.size(),i+50);
    command.insert(command.end(),paths.begin()+i,paths.begin()+end);
    std::mutex outputMutex;std::string output;
    try{
      process::IndigenousChildOptions options;options.lineMode=false;options.cwd=cwd.string();
      options.onData=[&](const std::string& chunk,bool isStdout){
        if(isStdout){std::lock_guard lock(outputMutex);output+=chunk;}};
      process::IndigenousJsonlProcess git(command,{},std::move(options));
      int code=git.waitForExit(std::chrono::steady_clock::now()+std::chrono::seconds(5));
      if(code==0){
        std::size_t start=0;
        while(start<output.size()){
          auto endline=output.find('\n',start);if(endline==std::string::npos)endline=output.size();
          auto path=output.substr(start,endline-start);
          if(!path.empty()&&path.back()=='\r')path.pop_back();
          if(!path.empty())ignored.insert(path);
          start=endline+1;
        }
      }
    }catch(...){/* Absence of git or a timeout leaves server results visible. */}
  }
  if(ignored.empty())return result;
  Json filtered=Json::array();
  for(const auto& item:result){
    if(!item.is_object()){filtered.push_back(item);continue;}
    auto loc=operation=="workspaceSymbol"?item.value("location",Json::object()):item;
    if(!loc.is_object()){filtered.push_back(item);continue;}
    auto uri=loc.value("targetUri",loc.value("uri",std::string{}));
    if(uri.empty()||!ignored.contains(uriToPath[uri]))filtered.push_back(item);
  }
  return filtered;
}
gadgets::GadgetResult output(const Json& input,std::string text,std::optional<Formatted> formatted={}){
  gadgets::GadgetResult result;
  result.data={{"operation",field(input,"operation")},{"result",std::move(text)},
    {"filePath",field(input,"filePath")}};
  if(formatted){result.data["resultCount"]=formatted->resultCount;
    result.data["fileCount"]=formatted->fileCount;}
  return result;
}
}
Runtime::Runtime(std::filesystem::path roost,Json serverConfig):
  diagnostics_(std::make_shared<lsp::Diagnostics>()),
  manager_(roost,lsp::loadServerConfigs(roost,serverConfig),diagnostics_){}
std::shared_ptr<Runtime> Runtime::create(std::filesystem::path roost,Json serverConfig){
  return std::shared_ptr<Runtime>(new Runtime(std::move(roost),std::move(serverConfig)));
}
Runtime::~Runtime(){
  try{shutdownAsync().wait();}catch(...){}
  if(worker_.joinable())worker_.join();
}
void Runtime::enqueue(std::function<void()> job){
  std::lock_guard lock(mutex_);
  if(closing_)throw std::runtime_error("LSP workspace retired");
  if(!worker_.joinable())worker_=std::thread([this]{run();});
  jobs_.push_back(std::move(job));ready_.notify_one();
}
void Runtime::run(){
  for(;;){
    std::function<void()> job;
    {std::unique_lock lock(mutex_);ready_.wait(lock,[this]{return done_||!jobs_.empty();});
      if(done_&&jobs_.empty())return;
      job=std::move(jobs_.front());jobs_.pop_front();}
    try{job();}catch(...){}
  }
}
void Runtime::changeFile(std::string path,std::string content){
  if(!supportsFile(path))return;
  enqueue([this,path=std::move(path),content=std::move(content)]{
    try{manager_.changeFile(path,content);}catch(...){}
  });
}
void Runtime::saveFile(std::string path){
  if(!supportsFile(path))return;
  enqueue([this,path=std::move(path)]{try{manager_.saveFile(path);}catch(...){}});
}
void Runtime::closeFile(std::string path){
  if(!supportsFile(path))return;
  enqueue([this,path=std::move(path)]{
    try{manager_.closeFile(path);}catch(...){}
    diagnostics_->dropFile(lsp::fileUri(path));
  });
}
void Runtime::clearDeliveredDiagnostics(const std::string& uri){diagnostics_->clearDelivered(uri);}
Json Runtime::takeDiagnosticSets(){return diagnostics_->takeSets();}
Json Runtime::takeDiagnosticAttachments(){
  Json attachments=Json::array();
  for(const auto& set:takeDiagnosticSets())attachments.push_back({{"type","diagnostics"},
    {"files",set.at("files")},{"isNew",true}});
  return attachments;
}
std::shared_future<void> Runtime::shutdownAsync(){
  std::lock_guard lock(mutex_);
  if(shutdown_.valid())return shutdown_;
  closing_=true;
  auto promise=std::make_shared<std::promise<void>>();
  shutdown_=promise->get_future().share();
  if(!worker_.joinable()){
    diagnostics_->reset();done_=true;promise->set_value();
  }else{
    jobs_.push_back([this,promise]{
      manager_.shutdown();
      {std::lock_guard lock(mutex_);done_=true;}
      promise->set_value();ready_.notify_all();
    });ready_.notify_one();
  }
  return shutdown_;
}
std::future<gadgets::GadgetResult> Runtime::callGadget(Json input,gadgets::GadgetCallContext context){
  auto promise=std::make_shared<std::promise<gadgets::GadgetResult>>();
  auto result=promise->get_future();
  try{enqueue([this,promise,input=std::move(input),context=std::move(context)]{
    try{promise->set_value(perform(input,context));}
    catch(...){promise->set_exception(std::current_exception());}
  });}catch(...){promise->set_exception(std::current_exception());}
  return result;
}
gadgets::GadgetResult Runtime::perform(const Json& input,const gadgets::GadgetCallContext& context){
  auto operation=field(input,"operation");auto requested=field(input,"filePath");
  auto path=fullPath(manager_.roost(),requested);
  if(!manager_.hasServer(path.string()))
    return output(input,"No LSP server available for file type: "+path.extension().string());
  try{
    if(!manager_.isFileOpen(path.string())){
      auto size=std::filesystem::file_size(path);
      if(size>10000000)return output(input,"File too large for LSP analysis ("+
        std::to_string((size+999999)/1000000)+"MB exceeds 10MB limit)");
      std::ifstream in(path,std::ios::binary);
      if(!in)throw std::runtime_error("Cannot read file: "+path.string());
      std::string content((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
      manager_.openFile(path.string(),content);
    }
    auto uri=lsp::fileUri(path);
    auto line=input.at("line").get<int>(),character=input.at("character").get<int>();
    auto params=methodParams(operation,uri,line,character);
    Json response=manager_.request(path.string(),method(operation),params,context.stopToken);
    if((operation=="incomingCalls"||operation=="outgoingCalls")){
      if(!response.is_array()||response.empty())return output(input,
        "No call hierarchy item found at this position",Formatted{"",0,0});
      response=manager_.request(path.string(),operation=="incomingCalls"?
        "callHierarchy/incomingCalls":"callHierarchy/outgoingCalls",{{"item",response.at(0)}},context.stopToken);
    }
    response=filterIgnored(operation,response,manager_.roost());
    auto formatted=formatResult(operation,response,manager_.roost());
    return output(input,formatted.text,formatted);
  }catch(const std::exception& error){
    return output(input,"Error performing "+operation+": "+error.what());
  }
}
}
