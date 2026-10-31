#include "instructions/hooks/runtime/IndigenousCommandPreparation.hpp"
#include "shell/CommandQuoting.hpp"
#include "shell/WindowsPaths.hpp"
#include "runtime/JsonValue.hpp"
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
    for(const auto& item:value){
      if(!first)result+=",";
      first=false;
      if(!item.is_null())result+=optionValue(item);
    }
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
std::string optionEnvKey(const std::string& name){
  std::string result;
  for(unsigned char unit:name){
    const bool identifier=(unit>='A'&&unit<='Z')||
      (unit>='a'&&unit<='z')||(unit>='0'&&unit<='9')||unit=='_';
    result+=identifier?static_cast<char>(std::toupper(unit)):'_';
  }
  return result;
}
bool flightEnvironmentEvent(const std::string& event){
  return event=="SessionStart"||event=="Setup"||
    event=="CwdChanged"||event=="FileChanged";
}
#ifdef _WIN32
bool shellScriptCommand(const std::string& command){
  const auto trimmed=value::trim(command);
  std::size_t at=0;
  while((at=trimmed.find(".sh",at))!=std::string::npos){
    const auto end=at+3;
    if(end==trimmed.size()||std::isspace(static_cast<unsigned char>(trimmed[end]))||
       trimmed[end]=='"')return true;
    at=end;
  }
  return false;
}
#endif
struct Preparation:std::enable_shared_from_this<Preparation>{
  event_loop::Loop& loop;
  IndigenousCommandConfig config;
  FullCommandRequest request;
  event_loop::Deferred<IndigenousProcessRequest> output;
  IndigenousProcessRequest process;
  std::string projectRoot;
  Json pluginOptions;
  bool hasPluginOptions=false;
  std::string toHookPath(const std::string& path)const{
#ifdef _WIN32
    if(process.shellType!="powershell")return shell::windowsPathToPosixPath(path);
#endif
    return path;
  }
  Preparation(event_loop::Loop& owner,IndigenousCommandConfig settings,
    FullCommandRequest invocation):loop(owner),config(std::move(settings)),
    request(std::move(invocation)),output(owner){}
  void fail(std::exception_ptr error){output.reject(error);}
  void start(){
    try{
      if(!config.getProjectRoot||!config.getCwd||!config.getOriginalCwd||
        !config.subprocessEnv||!config.pathExists||!config.shellPrefix)
        throw std::invalid_argument("Hook command environment imports required");
      auto shellType=field(request.base.hook,"shell");
      if(shellType.empty())shellType=config.defaultShell;
      if(shellType!="bash"&&shellType!="powershell")
        throw std::invalid_argument("Unsupported hook shell: "+shellType);
#ifndef _WIN32
      if(shellType=="powershell")
        throw std::invalid_argument("PowerShell hook process adapter required");
#endif
      process.shellType=shellType;
#ifdef _WIN32
      if(shellType=="powershell"){
        const auto path=shell::findPowerShellPath();
        if(!path)throw std::runtime_error("PowerShell hook requested but no pwsh or powershell executable was found on PATH");
        process.shellPath=*path;
      }else process.shellPath=shell::findGitBashPath();
#else
      process.shellPath=config.shellPath;
#endif
      process.command=field(request.base.hook,"command");
      process.input=request.base.jsonInput;
      process.signal=request.base.signal;
      process.timeoutMs=config.defaultTimeoutMs;
      if(truthField(request.base.hook,"timeout")&&
        request.base.hook.at("timeout").is_number())
        process.timeoutMs=request.base.hook.at("timeout").get<double>()*1000;
      projectRoot=config.getProjectRoot();
      if(request.base.pluginRoot){
        config.pathExists(*request.base.pluginRoot).then(
          [self=shared_from_this()](const bool* exists,
            const std::exception_ptr& error){
            if(error){self->fail(error);return;}
            if(!*exists){
              auto message="Plugin directory does not exist: "+
                *self->request.base.pluginRoot;
              if(self->request.base.pluginId)
                message+=" ("+*self->request.base.pluginId+
                  " — run /plugin to reinstall)";
              self->fail(std::make_exception_ptr(std::runtime_error(message)));
              return;
            }
            try{self->afterPluginCheck();}
            catch(...){self->fail(std::current_exception());}
          });
      }else afterPluginCheck();
    }catch(...){fail(std::current_exception());}
  }
  void afterPluginCheck(){
    if(request.base.pluginRoot){
      replaceLiteral(process.command,"${"+config.pluginRootVariable+"}",
        toHookPath(*request.base.pluginRoot));
      if(request.base.pluginId){
        if(!config.getPluginDataDir||!config.loadPluginOptions||
          !config.substituteUserConfig)
          throw std::invalid_argument("Hook plugin imports required");
        const auto dataPath=config.getPluginDataDir(*request.base.pluginId);
        replaceLiteral(process.command,"${"+config.pluginDataVariable+"}",toHookPath(dataPath));
        pluginOptions=config.loadPluginOptions(*request.base.pluginId);
        hasPluginOptions=true;
        process.command=config.substituteUserConfig(process.command,pluginOptions);
      }
    }
#ifdef _WIN32
    if(process.shellType!="powershell"&&shellScriptCommand(process.command)&&
       value::trim(process.command).rfind("bash ",0)!=0)
      process.command="bash "+process.command;
#endif
    const auto prefix=config.shellPrefix();
    if(process.shellType!="powershell"&&prefix&&!prefix->empty())
      process.command=shell_command::formatShellPrefixCommand(*prefix,process.command);
    process.environment=config.subprocessEnv();
    process.environment[config.projectDirVariable]=toHookPath(projectRoot);
    if(request.base.pluginRoot){
      process.environment[config.pluginRootVariable]=toHookPath(*request.base.pluginRoot);
      if(request.base.pluginId)
        process.environment[config.pluginDataVariable]=
          toHookPath(config.getPluginDataDir(*request.base.pluginId));
    }
    if(hasPluginOptions&&pluginOptions.is_object())
      for(auto it=pluginOptions.begin();it!=pluginOptions.end();++it)
        process.environment[config.pluginOptionPrefix+optionEnvKey(it.key())]=
          optionValue(it.value());
    if(request.skillRoot)
      process.environment[config.pluginRootVariable]=toHookPath(*request.skillRoot);
    if(process.shellType!="powershell"&&
       flightEnvironmentEvent(request.base.hookEvent)&&request.hookIndex){
      if(!config.getHookEnvFilePath)
        throw std::invalid_argument("Hook environment-file import required");
      config.getHookEnvFilePath(request.base.hookEvent,*request.hookIndex).then(
        [self=shared_from_this()](const std::string* path,
          const std::exception_ptr& error){
          if(error){self->fail(error);return;}
          self->process.environment[self->config.environmentFileVariable]=
            self->toHookPath(*path);
          try{self->checkCwd();}
          catch(...){self->fail(std::current_exception());}
        });
    }else checkCwd();
  }
  void checkCwd(){
    const auto cwd=config.getCwd();
    config.pathExists(cwd).then([self=shared_from_this(),cwd](const bool* exists,
      const std::exception_ptr& error){
      if(error){self->fail(error);return;}
      try{
        self->process.cwd=*exists?cwd:self->config.getOriginalCwd();
        if(!*exists&&self->config.logDebug)
          self->config.logDebug("Hooks: cwd "+cwd+
            " not found, falling back to original cwd","warn");
        self->output.resolve(std::move(self->process));
      }catch(...){self->fail(std::current_exception());}
    });
  }
};
}
event_loop::Deferred<IndigenousProcessRequest> prepareIndigenousHookCommand(
  event_loop::Loop& loop,const IndigenousCommandConfig& config,
  const FullCommandRequest& request){
  if(!loop.isOwnerThread())throw std::logic_error("Hook process owner required");
  auto run=std::make_shared<Preparation>(loop,config,request);
  run->start();return run->output;
}
} // namespace batbox::instructions::hooks::runtime
