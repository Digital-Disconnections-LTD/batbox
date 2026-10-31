#include "cleanup/Retention.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <regex>
#include <stdexcept>

namespace batbox::cleanup {
Result addCleanupResults(Result a,Result b){
  return {a.messages+b.messages,a.errors+b.errors};
}
std::string filenameToIsoString(const std::string& filename){
  const auto dot=filename.find('.');
  const auto first=filename.substr(0,dot);
  static const std::regex stamp(R"(T(\d{2})-(\d{2})-(\d{2})-(\d{3})Z)");
  return std::regex_replace(first,stamp,"T$1:$2:$3.$4Z",std::regex_constants::format_first_only);
}
double cutoffDateMs(double nowMs,std::optional<double> days){
  return nowMs-days.value_or(30.0)*24.0*60.0*60.0*1000.0;
}
namespace {
bool isNoEntry(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const std::system_error& value){return value.code()==std::errc::no_such_file_or_directory;}
  catch(...){return false;}
}
void check(const RetentionServices& services){
  if(!services.fs.readdir||!services.fs.unlink||!services.fs.rmdir||
    !services.parseDateMs||!services.logError||!services.joinPath)
    throw std::invalid_argument("retention cleanup imports required");
}
struct FileSweep:std::enable_shared_from_this<FileSweep>{
  event_loop::Loop& loop;
  std::string dir;
  double cutoff;
  bool messagePath;
  RetentionServices services;
  event_loop::Deferred<Result> result;
  Result counts;
  std::vector<Dirent> files;
  std::size_t index=0;
  FileSweep(event_loop::Loop& owner,std::string path,double limit,bool isMessage,
    RetentionServices imports):loop(owner),dir(std::move(path)),cutoff(limit),
    messagePath(isMessage),services(std::move(imports)),result(owner){}
  void start(){
    auto self=shared_from_this();
    try{services.fs.readdir(dir).then([self](const std::vector<Dirent>* value,
      const std::exception_ptr& error){
      if(error){if(!isNoEntry(error))self->services.logError(error);
        self->result.resolve(self->counts);return;}
      self->files=*value;self->next();
    });}catch(...){auto error=std::current_exception();
      if(!isNoEntry(error))services.logError(error);
      result.resolve(counts);
    }
  }
  void next(){
    while(index<files.size()){
      const auto file=files[index++];
      try{
        const auto date=services.parseDateMs(filenameToIsoString(file.name));
        if(!date||!(*date<cutoff))continue;
        auto self=shared_from_this();
        services.fs.unlink(services.joinPath(dir,file.name)).then(
          [self](const event_loop::Unit*,const std::exception_ptr& error){
            if(error)self->services.logError(error);
            else if(self->messagePath)++self->counts.messages;
            else ++self->counts.errors;
            self->next();
          });
        return;
      }catch(...){services.logError(std::current_exception());}
    }
    result.resolve(counts);
  }
};
struct MessageSweep:std::enable_shared_from_this<MessageSweep>{
  event_loop::Loop& loop;
  std::string errors,base;
  double cutoff;
  RetentionServices services;
  event_loop::Deferred<Result> result;
  Result counts;
  std::vector<std::string> dirs;
  std::size_t index=0;
  MessageSweep(event_loop::Loop& owner,std::string errorPath,std::string basePath,
    double limit,RetentionServices imports):loop(owner),errors(std::move(errorPath)),
    base(std::move(basePath)),cutoff(limit),services(std::move(imports)),result(owner){}
  void start(){
    auto self=shared_from_this();
    cleanupOldFilesInDirectory(loop,errors,cutoff,false,services).then(
      [self](const Result* value,const std::exception_ptr& error){
        if(error){self->result.reject(error);return;}
        self->counts=*value;self->readBase();
      });
  }
  void readBase(){
    auto self=shared_from_this();
    try{services.fs.readdir(base).then([self](const std::vector<Dirent>* value,
      const std::exception_ptr& error){
      if(error){self->result.resolve(self->counts);return;}
      for(const auto& entry:*value){
        if(entry.isDirectory&&entry.name.starts_with("mcp-logs-"))
          self->dirs.push_back(self->services.joinPath(self->base,entry.name));
      }
      self->next();
    });}catch(...){result.resolve(counts);}
  }
  void next(){
    if(index>=dirs.size()){result.resolve(counts);return;}
    const auto dir=dirs[index++];auto self=shared_from_this();
    cleanupOldFilesInDirectory(loop,dir,cutoff,true,services).then(
      [self,dir](const Result* value,const std::exception_ptr& error){
        if(error){if(!isNoEntry(error))self->services.logError(error);
          self->result.resolve(self->counts);return;}
        self->counts=addCleanupResults(self->counts,*value);
        try{self->services.fs.rmdir(dir).then([self](const event_loop::Unit*,
          const std::exception_ptr&){self->next();});}
        catch(...){self->next();}
      });
  }
};
} // namespace
event_loop::Deferred<Result> cleanupOldFilesInDirectory(event_loop::Loop& loop,
  const std::string& dir,double cutoff,bool isMessage,const RetentionServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("retention cleanup requires loop owner");
  check(services);
  auto sweep=std::make_shared<FileSweep>(loop,dir,cutoff,isMessage,services);
  sweep->start();return sweep->result;
}
event_loop::Deferred<Result> cleanupOldMessageFiles(event_loop::Loop& loop,
  const std::string& errors,const std::string& base,double cutoff,
  const RetentionServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("retention cleanup requires loop owner");
  check(services);
  auto sweep=std::make_shared<MessageSweep>(loop,errors,base,cutoff,services);
  sweep->start();return sweep->result;
}
} // namespace batbox::cleanup
