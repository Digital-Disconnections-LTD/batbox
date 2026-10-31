#include "diagnostics/DebugLog.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace batbox::diagnostics {
namespace {
bool includes(const std::vector<std::string>& values,const std::string& item){
  return std::find(values.begin(),values.end(),item)!=values.end();
}
bool starts(const std::string& value,const std::string& prefix){
  return value.rfind(prefix,0)==0;
}
std::string lower(std::string value){
  for(char& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}
bool envTruthy(const std::optional<std::string>& raw){
  if(!raw)return false;
  const auto value=lower(value::trim(*raw));
  return value=="1"||value=="true"||value=="yes"||value=="on";
}
int levelOrder(const std::string& level){
  if(level=="verbose")return 0;
  if(level=="debug")return 1;
  if(level=="info")return 2;
  if(level=="warn")return 3;
  if(level=="error")return 4;
  return -1;
}
std::string dirname(const std::string& path){
  const auto parent=std::filesystem::path(path).parent_path().string();
  return parent.empty()?".":parent;
}
}

struct DebugLog::State:std::enable_shared_from_this<State> {
  explicit State(DebugLogServices value):services(std::move(value)),pendingWrite(*services.loop){
    pendingWrite.resolve({});
  }
  DebugLogServices services;
  std::optional<std::string> minLevel;
  std::optional<bool> debugMode,stderrMode;
  std::optional<std::optional<std::string>> debugFile;
  std::optional<std::optional<DebugFilter>> filter;
  bool runtimeDebugEnabled=false,hasFormattedOutput=false,symlinkStarted=false;
  std::unique_ptr<BufferedWriter> writer;
  DebugMission pendingWrite;
  std::optional<std::string> ensuredDir;

  std::string getMinLevel(){
    if(!minLevel){
      const auto raw=services.env("BATBOX_DEBUG_LOG_LEVEL");
      const auto value=raw?lower(value::trim(*raw)):std::string{};
      minLevel=levelOrder(value)>=0?value:"debug";
    }
    return *minLevel;
  }
  bool isStderr(){
    if(!stderrMode){const auto args=services.argv();
      stderrMode=includes(args,"--debug-to-stderr")||includes(args,"-d2e");}
    return *stderrMode;
  }
  std::optional<std::string> filePath(){
    if(!debugFile){
      std::optional<std::string> result;
      const auto args=services.argv();
      for(std::size_t i=0;i<args.size();++i){
        if(starts(args[i],"--debug-file=")){result=args[i].substr(13);break;}
        if(args[i]=="--debug-file"&&i+1<args.size()){result=args[i+1];break;}
      }
      debugFile.emplace(result);
    }
    return *debugFile;
  }
  bool isDebug(){
    if(!debugMode){
      const auto args=services.argv();
      debugMode=runtimeDebugEnabled||envTruthy(services.env("DEBUG"))||
        envTruthy(services.env("DEBUG_SDK"))||includes(args,"--debug")||
        includes(args,"-d")||isStderr()||
        std::any_of(args.begin(),args.end(),[](const auto& arg){return starts(arg,"--debug=");})||
        filePath().has_value();
    }
    return *debugMode;
  }
  std::optional<DebugFilter> debugFilter(){
    if(!filter){
      std::optional<DebugFilter> value;
      for(const auto& arg:services.argv())if(starts(arg,"--debug=")){
        value=parseDebugFilter(arg.substr(8));break;
      }
      filter.emplace(value);
    }
    return *filter;
  }
  bool shouldLog(const std::string& message){
    if(services.env("NODE_ENV")=="test"&&!isStderr())return false;
    if(!services.isInternalUser()&&!isDebug())return false;
    if(!services.nodeVersionAvailable())return false;
    return shouldShowDebugMessage(message,debugFilter());
  }
  std::string logPath(){
    if(const auto file=filePath())return *file;
    if(const auto dir=services.env("BATBOX_DEBUG_LOGS_DIR"))return *dir;
    return (std::filesystem::path(services.configHomeDir())/"debug"/
      (services.flightId()+".txt")).string();
  }
  void updateSymlink(){
    if(symlinkStarted)return;
    symlinkStarted=true;
    try{
      const auto path=logPath();
      const auto latest=(std::filesystem::path(dirname(path))/"latest").string();
      auto removed=services.unlinkAsync(latest);
      const auto self=shared_from_this();
      removed.then([self,path,latest](const event_loop::Unit*,const std::exception_ptr&){
        try{auto linked=self->services.symlinkAsync(path,latest);
          linked.then([](const event_loop::Unit*,const std::exception_ptr&){});
        }catch(...){}
      });
    }catch(...){}
  }
  DebugMission appendAsync(bool needMkdir,std::string dir,std::string path,std::string content){
    DebugMission result(*services.loop);
    const auto self=shared_from_this();
    const auto append=[self,result,path=std::move(path),content=std::move(content)]{
      try{
        auto written=self->services.appendFileAsync(path,content);
        written.then([self,result](const event_loop::Unit*,const std::exception_ptr& error){
          if(error){result.reject(error);return;}
          self->updateSymlink();
          result.resolve({});
        });
      }catch(...){result.reject(std::current_exception());}
    };
    if(needMkdir){
      try{auto made=services.mkdirAsync(dir);
        made.then([append](const event_loop::Unit*,const std::exception_ptr&){append();});
      }catch(...){append();}
    }else append();
    return result;
  }
  void handleWrite(const std::string& content){
    const auto path=logPath(),dir=dirname(path);
    const bool needMkdir=ensuredDir!=dir;
    ensuredDir=dir;
    if(isDebug()){
      if(needMkdir)try{services.mkdirSync(dir);}catch(...){}
      services.appendFileSync(path,content);
      updateSymlink();
      return;
    }
    DebugMission next(*services.loop);
    const auto self=shared_from_this();
    pendingWrite.then([self,next,needMkdir,dir,path,content](const event_loop::Unit*,
        const std::exception_ptr&){
      try{auto appended=self->appendAsync(needMkdir,dir,path,content);
        appended.then([next](const event_loop::Unit*,const std::exception_ptr&){next.resolve({});});
      }catch(...){next.resolve({});}
    });
    pendingWrite=next;
  }
  BufferedWriter& getWriter(){
    if(!writer){
      auto options=indigenousBufferedWriterServices(*services.loop,
        [weak=weak_from_this()](const std::string& content){
          if(auto self=weak.lock())self->handleWrite(content);
        });
      options.flushIntervalMs=1000;
      options.maxBufferSize=100;
      options.immediateMode=isDebug();
      writer=std::make_unique<BufferedWriter>(std::move(options));
      const auto weak=weak_from_this();
      services.registerCleanup([weak]{
        if(auto self=weak.lock()){
          if(self->writer)self->writer->dispose();
          return self->pendingWrite;
        }
        throw std::runtime_error("Debug logger was destroyed before cleanup");
      });
    }
    return *writer;
  }
};

DebugLog::DebugLog(DebugLogServices services){
  if(!services.loop||!services.env||!services.argv||!services.isInternalUser||
      !services.nodeVersionAvailable||!services.timestampISO||!services.configHomeDir||
      !services.flightId||!services.writeToStderr||!services.mkdirSync||
      !services.appendFileSync||!services.mkdirAsync||!services.appendFileAsync||
      !services.unlinkAsync||!services.symlinkAsync||!services.registerCleanup)
    throw std::invalid_argument("Debug logger imports are required");
  state_=std::make_shared<State>(std::move(services));
}
std::string DebugLog::getMinDebugLogLevel(){return state_->getMinLevel();}
bool DebugLog::isDebugMode(){return state_->isDebug();}
bool DebugLog::enableDebugLogging(){
  const bool wasActive=state_->isDebug()||state_->services.isInternalUser();
  state_->runtimeDebugEnabled=true;state_->debugMode.reset();return wasActive;
}
std::optional<DebugFilter> DebugLog::getDebugFilter(){return state_->debugFilter();}
bool DebugLog::isDebugToStdErr(){return state_->isStderr();}
std::optional<std::string> DebugLog::getDebugFilePath(){return state_->filePath();}
bool DebugLog::shouldLogDebugMessage(const std::string& message){return state_->shouldLog(message);}
void DebugLog::setHasFormattedOutput(bool value){state_->hasFormattedOutput=value;}
bool DebugLog::getHasFormattedOutput()const{return state_->hasFormattedOutput;}
void DebugLog::logForDebugging(std::string message,const std::string& level){
  const auto threshold=levelOrder(state_->getMinLevel());
  const auto actual=levelOrder(level);
  if(actual>=0&&actual<threshold)return;
  if(!state_->shouldLog(message))return;
  if(state_->hasFormattedOutput&&message.find('\n')!=std::string::npos)
    message=value::stringify(Json(message));
  auto upper=level;
  for(auto& c:upper)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  const auto output=state_->services.timestampISO()+" ["+upper+"] "+value::trim(message)+"\n";
  if(state_->isStderr()){state_->services.writeToStderr(output);return;}
  state_->getWriter().write(output);
}
std::string DebugLog::getDebugLogPath(){return state_->logPath();}
DebugMission DebugLog::flushDebugLogs(){
  if(state_->writer)state_->writer->flush();
  return state_->pendingWrite;
}
DebugMission DebugLog::cleanup(){
  if(state_->writer)state_->writer->dispose();
  return state_->pendingWrite;
}
void DebugLog::logInternalError(const std::string& context,
    const std::optional<std::string>& stack){
  if(!state_->services.isInternalUser()||!stack||stack->empty())return;
  logForDebugging("[BATBOX-ONLY] "+context+" stack trace:\n"+*stack,"error");
}
}
