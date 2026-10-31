#include "git/GitignoreUpdate.hpp"
#include <cerrno>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace batbox::git {
using event_loop::Deferred;
using event_loop::Unit;
namespace {
template<class T>void need(const std::function<T>& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Gitignore binding required: ")+name);
}
bool isEnoent(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const std::system_error& system){
    return system.code()==std::errc::no_such_file_or_directory;
  }catch(...){return false;}
}
}
Deferred<bool> isPathGitignored(event_loop::Loop& loop,
    const std::string& filePath,const std::string& cwd,
    const GitignoreServices& services){
  Deferred<bool> result(loop);
  try{
    need(services.execFileNoThrowWithCwd,"execFileNoThrowWithCwd");
    process::ExecOptions options;
    options.preserveOutputOnError=false;
    options.cwd=cwd;
    services.execFileNoThrowWithCwd("git",{"check-ignore",filePath},options)
      .then([result](const process::ExecResult* value,
          const std::exception_ptr& error){
        if(error)result.reject(error);
        else result.resolve(value->code==0);
      });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
std::string getGlobalGitignorePath(const GitignoreServices& services){
  need(services.homedir,"homedir");
  return (std::filesystem::path(services.homedir())/".config"/"git"/"ignore")
    .string();
}
Deferred<Unit> addFileGlobRuleToGitignore(event_loop::Loop& loop,
    const std::string& filename,const std::optional<std::string>& cwd,
    const GitignoreServices& services){
  Deferred<Unit> output(loop);
  // Capture state in every pending Promise reaction so the asynchronous call
  // remains live after the returned Deferred is dropped.
  struct Job:std::enable_shared_from_this<Job> {
    Deferred<Unit> output;
    GitignoreServices services;
    event_loop::Loop& loop;
    std::string filename,cwd,entry,path;
    Job(Deferred<Unit> out,GitignoreServices imports,event_loop::Loop& owner,
        std::string name,std::string directory):output(std::move(out)),
        services(std::move(imports)),loop(owner),filename(std::move(name)),
        cwd(std::move(directory)){}
    void fail(const std::exception_ptr& error){
      try{need(services.logError,"logError");
        services.logError(error);output.resolve(Unit{});
      }catch(...){output.reject(std::current_exception());}
    }
    void writeNew(){
      try{
        need(services.io.writeUtf8,"writeUtf8");
        services.io.writeUtf8(path,entry+"\n").then(
          [self=shared_from_this()](const Unit*,const std::exception_ptr& error){
            if(error)self->fail(error);else self->output.resolve(Unit{});
          });
      }catch(...){fail(std::current_exception());}
    }
    void readExisting(){
      try{
        need(services.io.readUtf8,"readUtf8");
        services.io.readUtf8(path).then(
          [self=shared_from_this()](const std::string* content,
              const std::exception_ptr& error){
            if(error){
              if(isEnoent(error))self->writeNew();
              else self->fail(error);
              return;
            }
            if(content->find(self->entry)!=std::string::npos){
              self->output.resolve(Unit{});return;
            }
            try{
              need(self->services.io.appendUtf8,"appendUtf8");
              self->services.io.appendUtf8(self->path,
                "\n"+self->entry+"\n").then(
                  [self](const Unit*,const std::exception_ptr& appendError){
                    if(appendError)self->fail(appendError);
                    else self->output.resolve(Unit{});
                  });
            }catch(...){self->fail(std::current_exception());}
          });
      }catch(...){fail(std::current_exception());}
    }
    void afterIgnored(bool ignored){
      if(ignored){output.resolve(Unit{});return;}
      try{
        path=getGlobalGitignorePath(services);
        need(services.io.mkdirRecursive,"mkdirRecursive");
        services.io.mkdirRecursive(
          std::filesystem::path(path).parent_path().string()).then(
          [self=shared_from_this()](const Unit*,const std::exception_ptr& error){
            if(error)self->fail(error);else self->readExisting();
          });
      }catch(...){fail(std::current_exception());}
    }
    void afterRepo(bool inRepo){
      if(!inRepo){output.resolve(Unit{});return;}
      entry="**/"+filename;
      const auto testPath=filename.ends_with('/')?
        filename+"sample-file.txt":filename;
      isPathGitignored(loop,testPath,cwd,services).then(
        [self=shared_from_this()](const bool* ignored,
            const std::exception_ptr& error){
          if(error)self->fail(error);
          else self->afterIgnored(*ignored);
        });
    }
    void start(){
      try{
        need(services.dirIsInGitRepo,"dirIsInGitRepo");
        services.dirIsInGitRepo(cwd).then(
          [self=shared_from_this()](const bool* inRepo,
              const std::exception_ptr& error){
            if(error)self->fail(error);else self->afterRepo(*inRepo);
          });
      }catch(...){fail(std::current_exception());}
    }
  };
  std::string directory;
  if(cwd)directory=*cwd;
  else{
    try{need(services.getCwd,"getCwd");directory=services.getCwd();}
    catch(...){output.reject(std::current_exception());return output;}
  }
  try{
    auto job=std::make_shared<Job>(output,services,loop,filename,directory);
    job->start();
  }catch(...){
    try{need(services.logError,"logError");
      services.logError(std::current_exception());output.resolve(Unit{});
    }catch(...){output.reject(std::current_exception());}
  }
  return output;
}
GitignoreServices bindGitignoreServices(event_loop::Loop& loop,
    GitBasics& basics,process::ExecImports processImports,
    std::function<std::string()> getCwd,
    std::function<void(std::exception_ptr)> logError){
  GitignoreServices services;
  services.dirIsInGitRepo=[&basics](const std::string& cwd){
    return basics.dirIsInGitRepo(cwd);};
  services.execFileNoThrowWithCwd=[&loop,imports=std::move(processImports)](
      const std::string& file,const std::vector<std::string>& args,
      const process::ExecOptions& options){
    return process::execFileNoThrowWithCwd(loop,file,args,options,imports);
  };
  services.homedir=indigenousHomedir;
  services.getCwd=std::move(getCwd);
  services.logError=std::move(logError);
  services.io=indigenousGitignoreIO(loop);
  return services;
}
std::function<void(const std::string&,const std::string&)>
makeSettingsGitignoreScheduler(event_loop::Loop& loop,
    GitignoreServices services){
  return [&loop,services=std::move(services)](
      const std::string& filename,const std::string& cwd){
    (void)addFileGlobRuleToGitignore(loop,filename,cwd,services);
  };
}
}
