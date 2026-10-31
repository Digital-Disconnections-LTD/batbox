#include "gadget_results/Storage.hpp"

namespace batbox::gadget_results {
event_loop::Deferred<Json> maybePersistLargeGadgetResult(event_loop::Loop& loop,
  const StorageImports& imports,const Json& block,const std::string& name,std::optional<double> threshold){
  event_loop::Deferred<Json> output(loop);
  try{
    const auto& content=value::property(block,"content");
    if(isGadgetResultContentEmpty(content)){
      auto result=block;result["content"]="("+name+" completed with no output)";
      output.resolve(std::move(result));return output;
    }
    if(!value::truthy(content)||hasImageBlock(content)||contentSize(content)<=threshold.value_or(MAX_GADGET_RESULT_BYTES)){
      output.resolve(block);return output;
    }
    persistGadgetResult(loop,imports,content,block.at("tool_use_id")).then(
      [output,block](const Json* result,const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        try{
          if(isPersistError(*result)){output.resolve(block);return;}
          auto replaced=block;replaced["content"]=buildLargeGadgetResultMessage(*result);
          output.resolve(std::move(replaced));
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<Json> processPreMappedGadgetResultBlock(event_loop::Loop& loop,
  const StorageImports& imports,const Json& block,const std::string& name,double maximum,const Json& overrides){
  try{return event_loop::adoptAsyncReturn(maybePersistLargeGadgetResult(loop,imports,block,name,
    getPersistenceThreshold(name,maximum,overrides)));}
  catch(...){event_loop::Deferred<Json> output(loop);output.reject(std::current_exception());return output;}
}
event_loop::Deferred<Json> processGadgetResultBlock(event_loop::Loop& loop,
  const StorageImports& imports,const std::string& name,double maximum,
  const std::function<Json(const Json&,const std::string&)>& mapResult,const Json& result,
  const std::string& id,const Json& overrides){
  try{return event_loop::adoptAsyncReturn(maybePersistLargeGadgetResult(loop,imports,mapResult(result,id),name,
    getPersistenceThreshold(name,maximum,overrides)));}
  catch(...){event_loop::Deferred<Json> output(loop);output.reject(std::current_exception());return output;}
}
} // namespace batbox::gadget_results
