#include "gadget_results/Storage.hpp"
#include "formatting/Format.hpp"
#include "shell/PosixPath.hpp"

namespace batbox::gadget_results {
std::string getFlightDir(const StorageImports& imports){
  const auto cwd=imports.getOriginalCwd();
  const auto project=imports.getProjectDir(cwd);
  const auto flight=imports.getFlightId();
  return shell::joinPosixPath(project,flight);
}
std::string getGadgetResultsDir(const StorageImports& imports){
  return shell::joinPosixPath(getFlightDir(imports),GADGET_RESULTS_SUBDIR);
}
std::string getGadgetResultPath(const StorageImports& imports,const std::string& id,bool isJson){
  return shell::joinPosixPath(getGadgetResultsDir(imports),id+(isJson?".json":".txt"));
}
Completion ensureGadgetResultsDir(event_loop::Loop& loop,const StorageImports& imports){
  Completion completed(loop);
  try{imports.mkdir(getGadgetResultsDir(imports)).then([completed](const auto*,const auto&){completed.resolve({});});}
  catch(...){completed.resolve({});}
  return completed;
}
event_loop::Deferred<Json> persistGadgetResult(event_loop::Loop& loop,const StorageImports& imports,
  const Json& content,const std::string& id){
  event_loop::Deferred<Json> completed(loop);const bool isJson=content.is_array();
  try{
    if(isJson)for(const auto& block:content)if(block.at("type")!="text"){
      completed.resolve({{"error","Cannot persist tool results containing non-text content"}});return completed;
    }
    ensureGadgetResultsDir(loop,imports).then([&loop,imports,content,id,isJson,completed](const auto*,const auto& failure){
      if(failure){completed.reject(failure);return;}
      try{
        const auto path=getGadgetResultPath(imports,id,isJson);
        const auto text=isJson?value::stringify(content,2):content.get<std::string>();
        const auto finish=[completed,text,path,isJson]{
          const auto preview=generatePreview(text,PREVIEW_SIZE_BYTES);
          completed.resolve({{"filepath",path},{"originalSize",value::stringLength(text)},
            {"isJson",isJson},{"preview",preview.at("preview")},{"hasMore",preview.at("hasMore")}});
        };
        const auto written=[imports,completed,text,path,finish](const auto*,const std::exception_ptr& error){
          try{
            if(error){
              try{std::rethrow_exception(error);}
              catch(const FileSystemError& fs){
                if(fs.code!="EEXIST"){
                  if(imports.logError)imports.logError(error);
                  completed.resolve({{"error",getFileSystemErrorMessage(fs)}});return;
                }
              }catch(const std::exception& e){
                if(imports.logError)imports.logError(error);
                completed.resolve({{"error",e.what()}});return;
              }
            }else if(imports.debug)imports.debug("Persisted tool result to "+path+
              " ("+formatting::formatFileSize(value::stringLength(text))+")");
            finish();
          }catch(...){completed.reject(std::current_exception());}
        };
        try{imports.writeExclusive(path,text).then(written);}
        catch(...){written(static_cast<const event_loop::Unit*>(nullptr),std::current_exception());}
      }catch(...){completed.reject(std::current_exception());}
    });
  }catch(...){completed.reject(std::current_exception());}
  return completed;
}
} // namespace batbox::gadget_results
