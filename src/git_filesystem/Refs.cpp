#include "git_filesystem/GitFilesystemInternal.hpp"
#include <stdexcept>

namespace batbox::git_filesystem {
Deferred<std::optional<std::string>> GitFilesystem::getCommonDir(const std::string& gitDir){
  Deferred<std::optional<std::string>> output(loop_);
  try{services_.readFile(joinPath(gitDir,"commondir")).then(
    [output,gitDir](const std::string* text,const std::exception_ptr& error){
      if(error){output.resolve(std::nullopt);return;}
      try{output.resolve(resolvePath(gitDir,value::trim(*text)));}
      catch(...){output.resolve(std::nullopt);}
    });}catch(...){output.resolve(std::nullopt);}
  return output;
}
Deferred<std::optional<std::string>> GitFilesystem::readRawSymref(
  const std::string& gitDir,const std::string& refPath,const std::string& branchPrefix){
  Deferred<std::optional<std::string>> output(loop_);
  try{services_.readFile(joinPath(gitDir,refPath)).then(
    [output,branchPrefix](const std::string* text,const std::exception_ptr& error){
      if(error){output.resolve(std::nullopt);return;}
      const auto content=value::trim(*text);
      if(content.rfind("ref:",0)==0){
        const auto target=value::trim(content.substr(4));
        if(target.rfind(branchPrefix,0)==0){
          const auto name=target.substr(branchPrefix.size());
          if(isSafeRefName(name)){output.resolve(name);return;}
        }
      }
      output.resolve(std::nullopt);
    });}catch(...){output.resolve(std::nullopt);}
  return output;
}
Deferred<std::optional<std::string>> GitFilesystem::resolveRefInDir(
  const std::string& dir,const std::string& ref){
  Deferred<std::optional<std::string>> output(loop_);
  auto packed=[this,output,dir,ref]{
    try{services_.readFile(joinPath(dir,"packed-refs")).then(
      [output,ref](const std::string* text,const std::exception_ptr& error){
        if(error){output.resolve(std::nullopt);return;}
        std::size_t start=0;
        for(;;){
          const auto end=text->find('\n',start);
          const auto line=text->substr(start,end==std::string::npos?
            std::string::npos:end-start);
          if(!line.empty()&&line[0]!='#'&&line[0]!='^'){
            const auto space=line.find(' ');
            if(space!=std::string::npos&&line.substr(space+1)==ref){
              const auto sha=line.substr(0,space);
              output.resolve(isValidGitSha(sha)?std::optional(sha):std::nullopt);
              return;
            }
          }
          if(end==std::string::npos)break;
          start=end+1;
        }
        output.resolve(std::nullopt);
      });}catch(...){output.resolve(std::nullopt);}
  };
  try{services_.readFile(joinPath(dir,ref)).then(
    [this,output,dir,packed](const std::string* text,const std::exception_ptr& error){
      if(error){packed();return;}
      try{
        const auto content=value::trim(*text);
        if(content.rfind("ref:",0)==0){
          const auto target=value::trim(content.substr(4));
          if(!isSafeRefName(target)){output.resolve(std::nullopt);return;}
          event_loop::adoptAsyncReturn(resolveRef(dir,target)).then(
            [output](const std::optional<std::string>* value,const std::exception_ptr& followError){
            if(followError)output.reject(followError);else output.resolve(*value);
          });
          return;
        }
        output.resolve(isValidGitSha(content)?std::optional(content):std::nullopt);
      }catch(...){output.reject(std::current_exception());}
    });}catch(...){packed();}
  return output;
}
Deferred<std::optional<std::string>> GitFilesystem::resolveRef(
  const std::string& gitDir,const std::string& ref){
  Deferred<std::optional<std::string>> output(loop_);
  try{resolveRefInDir(gitDir,ref).then([this,output,gitDir,ref](
    const std::optional<std::string>* value,const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    if(*value){output.resolve(*value);return;}
    try{getCommonDir(gitDir).then([this,output,gitDir,ref](
      const std::optional<std::string>* common,const std::exception_ptr& commonError){
      if(commonError){output.reject(commonError);return;}
      if(*common&&**common!=gitDir){
        try{event_loop::adoptAsyncReturn(resolveRefInDir(**common,ref)).then(
          [output](const std::optional<std::string>* resolved,const std::exception_ptr& readError){
          if(readError)output.reject(readError);else output.resolve(*resolved);
        });}catch(...){output.reject(std::current_exception());}
      }else output.resolve(std::nullopt);
    });}catch(...){output.reject(std::current_exception());}
  });}catch(...){output.reject(std::current_exception());}
  return output;
}
Deferred<std::optional<Head>> GitFilesystem::readGitHead(const std::string& gitDir){
  Deferred<std::optional<Head>> output(loop_);
  try{services_.readFile(joinPath(gitDir,"HEAD")).then(
    [this,output,gitDir](const std::string* text,const std::exception_ptr& error){
      if(error){output.resolve(std::nullopt);return;}
      try{
        const auto content=value::trim(*text);
        if(content.rfind("ref:",0)==0){
          const auto ref=value::trim(content.substr(4));
          constexpr const char* branchPrefix="refs/heads/";
          if(ref.rfind(branchPrefix,0)==0){
            const auto name=ref.substr(std::char_traits<char>::length(branchPrefix));
            output.resolve(isSafeRefName(name)?std::optional(Head{"branch",name}):std::nullopt);
            return;
          }
          if(!isSafeRefName(ref)){output.resolve(std::nullopt);return;}
          resolveRef(gitDir,ref).then([output](
            const std::optional<std::string>* sha,const std::exception_ptr& readError){
            if(readError)output.resolve(std::nullopt);
            else output.resolve(Head{"detached",sha->value_or(std::string())});
          });
          return;
        }
        output.resolve(isValidGitSha(content)?std::optional(Head{"detached",content}):std::nullopt);
      }catch(...){output.resolve(std::nullopt);}
    });}catch(...){output.resolve(std::nullopt);}
  return output;
}
} // namespace batbox::git_filesystem
