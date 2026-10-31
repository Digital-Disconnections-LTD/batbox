#include "gadgets/synthetic_output/SyntheticOutputFactory.hpp"

namespace batbox::gadgets::synthetic_output {
namespace {
std::string errorText(std::exception_ptr failure){
  try{if(failure)std::rethrow_exception(failure);}
  catch(const std::exception& error){return error.what();}
  catch(...){return "unknown error";}
  return "unknown error";
}
std::string renderErrors(const AjvValidation& result){
  if(!result.errors)return "undefined";
  std::string text;
  for(const auto& issue:*result.errors){
    if(!text.empty())text+=", ";
    text+=(issue.instancePath.empty()?"root":issue.instancePath)+
      ": "+issue.message;
  }
  return text;
}
}
SyntheticOutputFactory::SyntheticOutputFactory(Gadget base,AjvBackend backend):
  base_(std::move(base)),backend_(std::move(backend)),
  owner_(std::this_thread::get_id()){
  if(!backend_.validateSchema||!backend_.compile)
    throw std::invalid_argument("Native Ajv-compatible backend required");
}
std::shared_ptr<const CreateResult> SyntheticOutputFactory::build(
  const Json& schema){
  try{
    if(auto error=backend_.validateSchema(schema))
      return std::make_shared<CreateResult>(CreateResult{{},std::move(error)});
    auto validator=backend_.compile(schema);
    if(!validator)throw std::invalid_argument("Compiled schema validator required");
    auto gadget=std::make_shared<Gadget>(base_);
    gadget->inputJSONSchema=schema;
    gadget->call=[validator=std::move(validator)](const Json& input,
      const GadgetCallContext&,const std::function<std::future<Json>(const Json&)>&,
      const Json&,const std::function<void(const GadgetProgress&)>&){
      std::promise<GadgetResult> promise;
      try{
        const auto result=validator(input);
        if(!result.valid)throw OutputSchemaError(
          "Output does not match required schema: "+renderErrors(result));
        GadgetResult output;
        output.data="Structured output provided successfully";
        output.structuredOutput=input;
        promise.set_value(std::move(output));
      }catch(...){promise.set_exception(std::current_exception());}
      return promise.get_future();
    };
    return std::make_shared<CreateResult>(CreateResult{std::move(gadget),{}});
  }catch(...){
    return std::make_shared<CreateResult>(CreateResult{{},
      errorText(std::current_exception())});
  }
}
std::shared_ptr<const CreateResult> SyntheticOutputFactory::create(
  const std::shared_ptr<const Json>& schema){
  if(std::this_thread::get_id()!=owner_)
    throw std::logic_error("Synthetic-output cache owner required");
  if(!schema)throw std::invalid_argument("JSON schema identity required");
  for(auto it=stash_.begin();it!=stash_.end();)
    if(it->first.expired())it=stash_.erase(it);
    else ++it;
  std::weak_ptr<const Json> key=schema;
  if(auto found=stash_.find(key);found!=stash_.end())return found->second;
  auto result=build(*schema);
  stash_.emplace(std::move(key),result);
  return result;
}
std::size_t SyntheticOutputFactory::stashedEntries()const{
  return stash_.size();
}
} // namespace batbox::gadgets::synthetic_output
