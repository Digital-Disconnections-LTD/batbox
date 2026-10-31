#include "instructions/hooks/runtime/IndigenousCommand.hpp"
#include "instructions/hooks/runtime/IndigenousCommandProcess.hpp"
#include "shell/CommandQuoting.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
std::string field(const Json& value,const char* name){
  return value.is_object()&&value.contains(name)&&value.at(name).is_string()
    ?value.at(name).get<std::string>():"";
}
bool truthField(const Json& value,const char* name){
  return value.is_object()&&value.contains(name)&&value::truthy(value.at(name));
}
std::string optionValue(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_null())return "null";
  if(value.is_array()){
    std::string result;bool first=true;
    for(const auto& item:value){if(!first)result+=",";first=false;
      if(!item.is_null())result+=optionValue(item);}
    return result;
  }
  return "[object Object]";
}
void replaceLiteral(std::string& text,const std::string& token,const std::string& value){
  std::size_t from=0;
  while((from=text.find(token,from))!=std::string::npos){
    text.replace(from,token.size(),value);from+=value.size();
  }
}
bool flightEnvironmentEvent(const std::string& event){
  return event=="SessionStart"||event=="Setup"||
    event=="CwdChanged"||event=="FileChanged";
}
bool diagnosticEvent(const std::string& event){
  return event=="SessionStart"||event=="Setup"||event=="SessionEnd";
}
std::string optionEnvKey(const std::string& name){
  std::string result;
  for(unsigned char unit:name){
    result+=(std::isalnum(unit)||unit=='_')?
      static_cast<char>(std::toupper(unit)):'_';
  }
  return result;
}
struct IndigenousRun:std::enable_shared_from_this<IndigenousRun>{
  event_loop::Loop& loop;
  IndigenousCommandConfig config;
  CommandRequest request;
  event_loop::Deferred<CommandOutcome> output;
  IndigenousProcessRequest process;
  Json pluginOptions;
  bool hasPluginOptions=false,emitDiagnostics=false;
  double startedMs=0;
  explicit IndigenousRun(event_loop::Loop& owner,IndigenousCommandConfig settings,
    CommandRequest invocation):loop(owner),config(std::move(settings)),
    request(std::move(invocation)),output(owner){}
  void fail(std::exception_ptr error){output.reject(error);}
  void start(){
    try{
      if(!config.getProjectRoot||!config.getCwd||!config.getOriginalCwd||
        !config.subprocessEnv||!config.pathExists||!config.shellPrefix)
        throw std::invalid_argument("Hook command environment imports required");
      if(field(request.hook,"shell")!=""&&
        field(request.hook,"shell")!="bash")
        throw std::invalid_argument("PowerShell hook process adapter required");
      if(truthField(request.hook,"async")||truthField(request.hook,"asyncRewake"))
        throw std::invalid_argument("Async hook registry adapter required");
      process.shellPath=config.shellPath;
      process.command=field(request.hook,"command");
      process.input=request.jsonInput;
      process.signal=request.signal;
      process.timeoutMs=config.defaultTimeoutMs;
      if(request.hook.is_object()&&request.hook.contains("timeout")&&
        value::truthy(request.hook.at("timeout"))&&
        request.hook.at("timeout").is_number())
        process.timeoutMs=request.hook.at("timeout").get<double>()*1000;
      process.environment=config.subprocessEnv();
      process.environment[config.projectDirVariable]=config.getProjectRoot();
      if(request.pluginRoot){
        config.pathExists(*request.pluginRoot).then(
          [self=shared_from_this()](const bool* exists,const std::exception_ptr& error){
            if(error){self->fail(error);return;}
            if(!*exists){
              auto message="Plugin directory does not exist: "+*self->request.pluginRoot;
              if(self->request.pluginId)
                message+=" ("+*self->request.pluginId+" — run /plugin to reinstall)";
              self->fail(std::make_exception_ptr(std::runtime_error(message)));return;
            }
            try{self->afterPluginCheck();}catch(...){self->fail(std::current_exception());}
          });
      }else afterPluginCheck();
    }catch(...){fail(std::current_exception());}
  }
  void afterPluginCheck(){
    if(request.pluginRoot){
      replaceLiteral(process.command,"${"+config.pluginRootVariable+"}",
        *request.pluginRoot);
      process.environment[config.pluginRootVariable]=*request.pluginRoot;
      if(request.pluginId){
        if(!config.getPluginDataDir||!config.loadPluginOptions||
          !config.substituteUserConfig)
          throw std::invalid_argument("Hook plugin data and user-config imports required");
        const auto dataPath=config.getPluginDataDir(*request.pluginId);
        replaceLiteral(process.command,"${"+config.pluginDataVariable+"}",dataPath);
        process.environment[config.pluginDataVariable]=dataPath;
        pluginOptions=config.loadPluginOptions(*request.pluginId);
        hasPluginOptions=true;
        process.command=config.substituteUserConfig(process.command,pluginOptions);
        if(pluginOptions.is_object())for(auto it=pluginOptions.begin();it!=pluginOptions.end();++it)
          process.environment[config.pluginOptionPrefix+optionEnvKey(it.key())]=
            optionValue(it.value());
      }
    }
    const auto prefix=config.shellPrefix();
    if(prefix&&!prefix->empty())
      process.command=shell_command::formatShellPrefixCommand(*prefix,process.command);
    if(flightEnvironmentEvent(request.hookEvent)){
      if(!config.getHookEnvFilePath)
        throw std::invalid_argument("Hook environment-file import required");
      config.getHookEnvFilePath(request.hookEvent,request.index).then(
        [self=shared_from_this()](const std::string* path,
          const std::exception_ptr& error){
          if(error){self->fail(error);return;}
          self->process.environment[self->config.environmentFileVariable]=*path;
          try{self->checkCwd();}catch(...){self->fail(std::current_exception());}
        });
    }else checkCwd();
  }
  void checkCwd(){
    const auto cwd=config.getCwd();
    config.pathExists(cwd).then(
      [self=shared_from_this(),cwd](const bool* exists,
        const std::exception_ptr& error){
        if(error){self->fail(error);return;}
        try{
          self->process.cwd=*exists?cwd:self->config.getOriginalCwd();
          if(!*exists&&self->config.logDebug)
            self->config.logDebug("Hooks: cwd "+cwd+
              " not found, falling back to original cwd","warn");
          self->spawn();
        }catch(...){self->fail(std::current_exception());}
      });
  }
  void spawn(){
    emitDiagnostics=diagnosticEvent(request.hookEvent);
    if(emitDiagnostics){
      if(!config.nowMillis||!config.logDiagnostics)
        throw std::invalid_argument("Hook local diagnostics imports required");
      startedMs=config.nowMillis();
      config.logDiagnostics("hook_spawn_started",
        Json{{"hook_event_name",request.hookEvent},{"index",request.index}});
    }
    spawnHookProcess(loop,std::move(process)).then(
      [self=shared_from_this()](const CommandOutcome* result,
        const std::exception_ptr& error){
        try{
          if(self->emitDiagnostics){
            Json details={{"hook_event_name",self->request.hookEvent},
              {"index",self->request.index},
              {"duration_ms",self->config.nowMillis()-self->startedMs},
              {"exit_code",error?1:result->status},
              {"aborted",error?false:result->aborted}};
            self->config.logDiagnostics("hook_spawn_completed",details);
          }
          if(error)self->output.reject(error);else self->output.resolve(*result);
        }catch(...){self->fail(std::current_exception());}
      });
  }
};
}
std::function<event_loop::Deferred<CommandOutcome>(CommandRequest)>
indigenousCommandHook(event_loop::Loop& loop,IndigenousCommandConfig config){
  return [&loop,config=std::move(config)](CommandRequest request){
    if(!loop.isOwnerThread())throw std::logic_error("Hook process owner required");
    auto run=std::make_shared<IndigenousRun>(loop,config,std::move(request));
    run->start();return run->output;
  };
}
} // namespace batbox::instructions::hooks::runtime
