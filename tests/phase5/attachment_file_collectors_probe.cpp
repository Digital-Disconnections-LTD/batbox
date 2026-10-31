#include "attachments/files/FileAttachments.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace batbox;
namespace {
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr error;
  pending.then([&](const T* result,const std::exception_ptr& failure){
    error=failure;if(result)value=*result;
  });
  for(int i=0;i<2000&&!value&&!error;++i){
    loop.runReady();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if(error)std::rethrow_exception(error);
  if(!value)throw std::runtime_error("File attachment did not settle");
  return *value;
}
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
  event_loop::Deferred<T> result(loop);result.resolve(std::move(value));
  return result;
}
template<class T>event_loop::Deferred<T> failed(event_loop::Loop& loop,
    const std::string& message){
  event_loop::Deferred<T> result(loop);
  result.reject(std::make_exception_ptr(std::runtime_error(message)));
  return result;
}
Json run(const Json& input){
  event_loop::Loop loop;
  attachments::files::Services services(loop);
  Json trace=Json::array();
  const auto path=input.value("path",std::string("/workspace/a.txt"));
  const auto app=std::make_shared<app_state::AppState>();
  services.getAppState=[&]{trace.push_back(Json::array({"appState"}));return app;};
  services.isFileReadDenied=[&](const std::string& filename,
      const app_state::AppStateRef&){
    trace.push_back(Json::array({"deny",filename}));
    return input.value("denied",false);
  };
  services.expandPath=[](const std::string& filename){
    return filename.starts_with('/')?filename:"/workspace/"+filename;
  };
  services.displayPath=[](const std::string& filename){
    return filename.starts_with("/workspace/")?
      filename.substr(std::string("/workspace/").size()):filename;
  };
  services.withinReadSizeLimit=[&](const std::string&,std::size_t){
    return !input.value("large",false);
  };
  services.stat=[&](const std::string& filename){
    trace.push_back(Json::array({"stat",filename}));
    if(input.value("statError",false))return failed<attachments::files::Stat>(loop,"stat");
    return ready(loop,attachments::files::Stat{
      input.value("size",0.0),input.value("directory",false)});
  };
  services.readDirectory=[&](const std::string& filename){
    trace.push_back(Json::array({"readdir",filename}));
    if(input.value("readdirError",false))
      return failed<std::vector<std::string>>(loop,"readdir");
    return ready(loop,input.value("entries",std::vector<std::string>{}));
  };
  services.pdfPageCount=[&](const std::string& filename){
    trace.push_back(Json::array({"pages",filename}));
    if(input.value("pageError",false))
      return failed<std::optional<int>>(loop,"pages");
    std::optional<int> pages;
    if(input.contains("pages")&&!input["pages"].is_null())
      pages=input["pages"].get<int>();
    return ready(loop,pages);
  };
  services.modificationTime=[&](const std::string& filename){
    trace.push_back(Json::array({"mtime",filename}));
    if(input.value("mtimeError",false))return failed<double>(loop,"mtime");
    return ready(loop,input.value("mtime",0.0));
  };
  services.validateRead=[&](const Json& request,execution::Context&){
    trace.push_back(Json::array({"validate",request}));
    gadgets::ValidationResult result;result.result=input.value("valid",true);
    return ready(loop,result);
  };
  int reads=0;
  services.callRead=[&](const Json& request,execution::Context&){
    trace.push_back(Json::array({"read",request}));
    ++reads;
    const auto error=input.value("readError",std::string());
    event_loop::Deferred<gadgets::GadgetResult> result(loop);
    if(reads==1&&error=="tokens")
      result.reject(std::make_exception_ptr(
        gadgets::files::MaxFileReadTokenExceededError(30000,25000,"tokens")));
    else if(reads==1&&error=="size")
      result.reject(std::make_exception_ptr(
        filesystem::FileTooLargeError(300000,262144,"size")));
    else if(error=="other"||(reads>1&&input.value("truncatedError",false)))
      result.reject(std::make_exception_ptr(std::runtime_error("read")));
    else{
      gadgets::GadgetResult value;
      value.data=input.value("readData",Json{{"type","text"},
        {"file",{{"content","hello"}}}});
      result.resolve(std::move(value));
    }
    return result;
  };
  services.connectedIdeName=[&](const Json&){
    trace.push_back(Json::array({"ideName"}));
    if(input.contains("ideName")&&!input["ideName"].is_null())
      return std::optional<std::string>(input["ideName"].get<std::string>());
    return std::optional<std::string>();
  };
  services.nestedForFile=[&](const std::string& filename,
      execution::Context&,const app_state::AppStateRef&){
    trace.push_back(Json::array({"nested",filename}));
    return ready(loop,input.value("nested",Json::array()));
  };
  execution::Context context;
  context.data={{"options",{{"mcpClients",Json::array()}}}};
  context.readFileState=file_state_stash::createFileStateStashWithSizeLimit(100);
  if(input.contains("prior")&&!input["prior"].is_null()){
    const auto& prior=input["prior"];
    context.readFileState->set(path,file_state_stash::FileState{
      prior.value("content",std::string()),prior.value("timestamp",0.0)});
  }
  const auto op=input.at("op").get<std::string>();
  Json result;
  if(op=="pdf"){
    const auto value=await(loop,attachments::files::tryGetPDFReference(path,services));
    result=value?*value:Json();
  }else if(op=="generate"){
    attachments::files::ReadOptions options;
    const auto& raw=value::property(input,"options");
    if(raw.is_object()&&raw.contains("offset"))options.offset=raw["offset"].get<double>();
    if(raw.is_object()&&raw.contains("limit"))options.limit=raw["limit"].get<double>();
    const auto mode=input.value("mode",std::string("at-mention"))=="compact"?
      attachments::files::Mode::Compact:attachments::files::Mode::AtMention;
    const auto value=await(loop,attachments::files::generateFileAttachment(
      path,context,mode,options,services));
    result=value?*value:Json();
  }else if(op=="mentions"){
    result=await(loop,attachments::files::processAtMentionedFiles(
      input.value("text",std::string()),context,services));
  }else if(op=="selected"){
    result=await(loop,attachments::files::getSelectedLinesFromIDE(
      input.value("selection",Json()),context,services));
  }else if(op=="opened"){
    result=await(loop,attachments::files::getOpenedFileFromIDE(
      input.value("selection",Json()),context,services));
  }else throw std::invalid_argument("Unknown collector operation");
  return {{"result",result},{"trace",trace}};
}
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}
}}
