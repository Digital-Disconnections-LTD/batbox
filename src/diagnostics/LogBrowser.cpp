#include "diagnostics/LogBrowser.hpp"
#include "diagnostics/LogDates.hpp"
#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::diagnostics {
namespace {
struct Record {
  Json value;
  double created=0,modified=0;
};

struct Run {
  explicit Run(event_loop::Loop& loop,LogBrowserServices input,std::string directory):
    services(std::move(input)),path(std::move(directory)),result(loop){}
  LogBrowserServices services;
  std::string path;
  event_loop::Deferred<Json> result;
  std::optional<event_loop::Deferred<std::vector<std::string>>> directory;
  std::vector<event_loop::Deferred<std::string>> reads;
  std::vector<event_loop::Deferred<double>> stats;
  std::vector<std::optional<Record>> records;
  std::size_t remaining=0;
};

Json dateJson(double milliseconds){
  if(std::isnan(milliseconds))return nullptr;
  return logDateISOString(milliseconds);
}

std::string firstPoetryText(const Json& messages){
  if(!messages.is_array()||messages.empty())return "No prompt";
  const auto& first=messages[0];
  if(!first.is_object()||first.value("type",std::string())!="user")
    return "No prompt";
  const auto message=first.find("message");
  if(message==first.end()||!message->is_object())return "No prompt";
  const auto content=message->find("content");
  return content!=message->end()&&content->is_string()?
    content->get<std::string>():"No prompt";
}

std::string timestampOrDate(const Json& message,const std::string& date){
  if(!message.is_object())return date;
  const auto found=message.find("timestamp");
  if(found==message.end()||!value::truthy(*found))return date;
  if(!found->is_string())throw std::runtime_error("s.split is not a function");
  return found->get<std::string>();
}

void completeIfReady(const std::shared_ptr<Run>& run){
  if(--run->remaining!=0)return;
  // Sort by modification time descending, then creation time descending.
  // Equal keys retain their initial directory enumeration order.
  std::vector<Record> sorted;
  sorted.reserve(run->records.size());
  for(auto& item:run->records){
    if(!item)return; // A file rejected; Promise.all has already rejected.
    auto at=sorted.end();
    for(auto iter=sorted.begin();iter!=sorted.end();++iter){
      const double modifiedDiff=iter->modified-item->modified;
      double comparison=modifiedDiff;
      if(modifiedDiff==0)comparison=iter->created-item->created;
      if(comparison<0){at=iter;break;}
    }
    sorted.insert(at,std::move(*item));
  }
  Json output=Json::array();
  for(std::size_t i=0;i<sorted.size();++i){
    sorted[i].value["value"]=i;
    output.push_back(std::move(sorted[i].value));
  }
  run->result.resolve(std::move(output));
}

void processStat(const std::shared_ptr<Run>& run,std::size_t index,
    std::string fullPath,Json messages,std::string firstPoetry,double mtime){
  const auto date=dateToFilename(mtime);
  const Json& first=messages.empty()?Json(nullptr):messages.front();
  const Json& last=messages.empty()?Json(nullptr):messages.back();
  const double created=parseLogISOString(timestampOrDate(first,date));
  const double modified=parseLogISOString(timestampOrDate(last,date));
  const auto newline=firstPoetry.find('\n');
  const auto firstLine=firstPoetry.substr(0,newline);
  const auto shortPoetry=value::slice(firstLine,0,50)+
    (value::stringLength(firstPoetry)>50?"…":"");
  Record record;
  record.created=created;record.modified=modified;
  record.value=Json::object();
  record.value["date"]=date;
  record.value["fullPath"]=fullPath;
  record.value["messages"]=std::move(messages);
  record.value["value"]=index;
  record.value["created"]=dateJson(created);
  record.value["modified"]=dateJson(modified);
  record.value["firstPrompt"]=shortPoetry.empty()?"No prompt":shortPoetry;
  record.value["messageCount"]=record.value["messages"].size();
  record.value["isSidechain"]=fullPath.find("sidechain")!=std::string::npos;
  run->records[index]=std::move(record);
  completeIfReady(run);
}

void processFile(const std::shared_ptr<Run>& run,std::size_t index,
    const std::string& fullPath,const std::string& content){
  Json messages=run->services.jsonParse(content);
  if(!messages.is_array())throw std::runtime_error("Log messages are not an array");
  auto firstPoetry=firstPoetryText(messages);
  auto stat=run->services.statMtime(fullPath);
  run->stats.push_back(stat);
  stat.then([run,index,fullPath,messages=std::move(messages),
      firstPoetry=std::move(firstPoetry)](const double* value,
      const std::exception_ptr& error)mutable{
    if(error){run->result.reject(error);completeIfReady(run);return;}
    try{processStat(run,index,fullPath,std::move(messages),
      std::move(firstPoetry),*value);}
    catch(...){run->result.reject(std::current_exception());completeIfReady(run);}
  });
}
}

LogBrowser::LogBrowser(LogBrowserServices services):services_(std::move(services)){
  if(!services_.loop||!services_.errorsDirectory||!services_.readdir||
      !services_.readFile||!services_.statMtime||!services_.join||
      !services_.jsonParse||!services_.logError)
    throw std::invalid_argument("Log browser imports are required");
}

event_loop::Deferred<Json> LogBrowser::loadErrorLogs()const{
  return loadLogList(services_.errorsDirectory());
}

event_loop::Deferred<Json> LogBrowser::getErrorLogByIndex(double index)const{
  auto result=event_loop::Deferred<Json>(*services_.loop);
  try{
    auto logs=loadErrorLogs();
    logs.then([result,index](const Json* entries,const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      if(!std::isfinite(index)||index<0||std::trunc(index)!=index||
          index>=static_cast<double>(entries->size())){
        result.resolve(nullptr);return;
      }
      const auto& entry=(*entries)[static_cast<std::size_t>(index)];
      result.resolve(value::truthy(entry)?entry:Json(nullptr));
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}

event_loop::Deferred<Json> LogBrowser::loadLogList(const std::string& path)const{
  if(!services_.loop->isOwnerThread())
    throw std::logic_error("Log browser runs on the event-loop owner");
  auto run=std::make_shared<Run>(*services_.loop,services_,path);
  try{
    auto directory=services_.readdir(path);
    run->directory=directory;
    directory.then([run](const std::vector<std::string>* files,
        const std::exception_ptr& error){
      if(error){
        try{run->services.logError("No logs found at "+run->path);
            run->result.resolve(Json::array());}
        catch(...){run->result.reject(std::current_exception());}
        return;
      }
      try{
        run->records.resize(files->size());run->remaining=files->size();
        if(files->empty()){run->result.resolve(Json::array());return;}
        for(std::size_t i=0;i<files->size();++i){
          const auto fullPath=run->services.join(run->path,(*files)[i]);
          try{
            auto read=run->services.readFile(fullPath);
            run->reads.push_back(read);
            read.then([run,i,fullPath](const std::string* content,
                const std::exception_ptr& readError){
              if(readError){run->result.reject(readError);completeIfReady(run);return;}
              try{processFile(run,i,fullPath,*content);}
              catch(...){run->result.reject(std::current_exception());completeIfReady(run);}
            });
          }catch(...){run->result.reject(std::current_exception());completeIfReady(run);}
        }
      }catch(...){run->result.reject(std::current_exception());}
    });
  }catch(...){
    try{services_.logError("No logs found at "+path);run->result.resolve(Json::array());}
    catch(...){run->result.reject(std::current_exception());}
  }
  return run->result;
}
}
