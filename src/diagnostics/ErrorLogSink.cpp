#include "diagnostics/ErrorLogSink.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <stdexcept>
#include <utility>
#include <vector>

namespace batbox::diagnostics {
namespace {
std::string joinPath(const std::string& dir,const std::string& name){
  return (std::filesystem::path(dir)/name).string();
}
std::string dirname(const std::string& path){
  const auto parent=std::filesystem::path(path).parent_path().string();
  return parent.empty()?".":parent;
}
std::string errorString(const ErrorValue& error){
  if(error.name.empty())return error.message;
  if(error.message.empty())return error.name;
  return error.name+": "+error.message;
}
std::string errorText(const ErrorValue& error){
  return error.stack&& !error.stack->empty()?*error.stack:error.message;
}
std::string unknownString(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_null())return "null";
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_array()){
    std::string result;
    bool first=true;
    for(const auto& item:value){
      if(!first)result+=",";
      first=false;
      if(!item.is_null())result+=unknownString(item);
    }
    return result;
  }
  return "[object Object]";
}
}
std::string dateToLogFilename(const std::string& isoDate){
  std::string result=isoDate;
  for(auto& c:result)if(c==':'||c=='.')c='-';
  return result;
}
std::optional<std::string> extractServerMessage(const Json& data){
  if(data.is_string())return data.get<std::string>();
  if(data.is_object()){
    const auto& message=value::property(data,"message");
    if(message.is_string())return message.get<std::string>();
    const auto& nested=value::property(data,"error");
    if(nested.is_object()){
      const auto& nestedMessage=value::property(nested,"message");
      if(nestedMessage.is_string())return nestedMessage.get<std::string>();
    }
  }
  return std::nullopt;
}
struct ErrorLogSink::State:std::enable_shared_from_this<State>{
  ErrorLogServices services;
  std::string dateStamp;
  std::vector<std::pair<std::string,std::shared_ptr<BufferedWriter>>> writers;
  State(ErrorLogServices value,std::string startupISO):
    services(std::move(value)),dateStamp(dateToLogFilename(startupISO)){}
  std::string errorsPath()const{
    return joinPath(services.errorsDirectory(),dateStamp+".jsonl");
  }
  std::string mcpPath(const std::string& server)const{
    return joinPath(services.mcpLogsDirectory(server),dateStamp+".jsonl");
  }
  BufferedWriter& writer(const std::string& path){
    for(auto& [name,existing]:writers)if(name==path)return *existing;
    const auto dir=dirname(path);
    auto writeFn=[weak=weak_from_this(),path,dir](const std::string& content){
        const auto self=weak.lock();
        if(!self)return;
        try{self->services.appendFileSync(path,content);}
        catch(...){
          self->services.mkdirSync(dir);
          self->services.appendFileSync(path,content);
        }
      };
    auto options=services.makeBufferedWriterServices
      ? services.makeBufferedWriterServices(writeFn)
      : indigenousBufferedWriterServices(*services.loop,writeFn);
    options.flushIntervalMs=1000;
    options.maxBufferSize=50;
    auto created=std::make_shared<BufferedWriter>(std::move(options));
    writers.emplace_back(path,created);
    services.registerCleanup([created]{created->dispose();});
    return *created;
  }
  void writeJson(const std::string& path,const Json& object){
    writer(path).write(value::stringify(object)+"\n");
  }
  void appendInternal(const std::string& path,const Json& message){
    if(!services.isInternalUser())return;
    Json entry={{"timestamp",services.timestampISO()}};
    if(message.is_object())for(auto it=message.begin();it!=message.end();++it)
      entry[it.key()]=it.value();
    entry["cwd"]=services.cwd();
    entry["userType"]=services.internalUserTypeLabel();
    entry["sessionId"]=services.flightId();
    entry["version"]=services.version();
    writeJson(path,entry);
  }
  void logError(const ErrorValue& error){
    const auto rendered=errorText(error);
    std::string context;
    if(error.isHttpClientError&&error.requestUrl){
      std::string parts="url="+*error.requestUrl;
      if(error.responseStatus)parts+=",status="+std::to_string(*error.responseStatus);
      if(const auto body=extractServerMessage(error.responseData))parts+=",body="+*body;
      context="["+parts+"] ";
    }
    services.logForDebugging(error.name+": "+context+rendered,"error");
    appendInternal(errorsPath(),Json{{"error",context+rendered}});
  }
  void logMCPError(const std::string& server,const std::string& rendered,
      const std::string& errorText){
    services.logForDebugging("MCP server \""+server+"\" "+rendered,"error");
    writeJson(mcpPath(server),Json{{"error",errorText},
      {"timestamp",services.timestampISO()},{"sessionId",services.flightId()},
      {"cwd",services.cwd()}});
  }
  void logMCPDebug(const std::string& server,const std::string& message){
    services.logForDebugging("MCP server \""+server+"\": "+message,"debug");
    writeJson(mcpPath(server),Json{{"debug",message},
      {"timestamp",services.timestampISO()},{"sessionId",services.flightId()},
      {"cwd",services.cwd()}});
  }
};
ErrorLogSink::ErrorLogSink(ErrorLogServices services,std::string startupISO){
  if(!services.loop||!services.errorsDirectory||!services.mcpLogsDirectory||
      !services.timestampISO||!services.flightId||!services.cwd||!services.version||
      !services.isInternalUser||!services.internalUserTypeLabel||
      !services.logForDebugging||!services.appendFileSync||!services.mkdirSync||
      !services.registerCleanup||!services.attachErrorLogSink)
    throw std::invalid_argument("Error log sink imports are required");
  state_=std::make_shared<State>(std::move(services),std::move(startupISO));
}
std::string ErrorLogSink::getErrorsPath()const{return state_->errorsPath();}
std::string ErrorLogSink::getMCPLogsPath(const std::string& server)const{
  return state_->mcpPath(server);
}
void ErrorLogSink::logError(const ErrorValue& error){state_->logError(error);}
void ErrorLogSink::logMCPError(const std::string& server,const ErrorValue& error){
  state_->logMCPError(server,errorString(error),errorText(error));
}
void ErrorLogSink::logMCPUnknownError(const std::string& server,const Json& error){
  const auto rendered=unknownString(error);
  state_->logMCPError(server,rendered,rendered);
}
void ErrorLogSink::logMCPDebug(const std::string& server,const std::string& message){
  state_->logMCPDebug(server,message);
}
void ErrorLogSink::flushLogWritersForTesting(){
  for(auto& [path,writer]:state_->writers)writer->flush();
}
void ErrorLogSink::clearLogWritersForTesting(){
  for(auto& [path,writer]:state_->writers)writer->dispose();
  state_->writers.clear();
}
void ErrorLogSink::initializeErrorLogSink(){
  auto state=state_;
  ErrorLogCallbacks callbacks;
  callbacks.logError=[state](const ErrorValue& error){state->logError(error);};
  callbacks.logMCPError=[state](const std::string& server,const ErrorValue& error){
    state->logMCPError(server,errorString(error),errorText(error));};
  callbacks.logMCPUnknownError=[state](const std::string& server,const Json& error){
    const auto rendered=unknownString(error);
    state->logMCPError(server,rendered,rendered);};
  callbacks.logMCPDebug=[state](const std::string& server,const std::string& message){
    state->logMCPDebug(server,message);};
  callbacks.getErrorsPath=[state]{return state->errorsPath();};
  callbacks.getMCPLogsPath=[state](const std::string& server){return state->mcpPath(server);};
  state_->services.attachErrorLogSink(std::move(callbacks));
  state_->services.logForDebugging("Error log sink initialized","debug");
}
}
