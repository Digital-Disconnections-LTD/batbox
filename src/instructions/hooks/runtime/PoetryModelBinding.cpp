#include "instructions/hooks/runtime/PoetryModelBinding.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
void bindPoetryModelBackend(event_loop::Loop& loop,PoetryHookServices& poetry,
  PoetryModelBackend backend){
  if(!backend)throw std::invalid_argument("Prompt model backend required");
  poetry.queryModelWithoutStreaming=[&loop,backend=std::move(backend)](
    PoetryQueryRequest call){
    event_loop::Deferred<Json> output(loop);
    try{
      if(!call.gadgets||!call.gadgets->is_array())
        throw std::invalid_argument("Prompt ToolUseContext tools required");
      if(!call.getGadgetPermissionContext)
        throw std::invalid_argument("Prompt permission-context getter required");
      model::SimpleQueryRequest request;
      request.sourceContext=std::move(call.sourceContext);
      request.input.messages=std::move(call.messages);
      request.input.systemPoetry=std::move(call.systemPoetry);
      request.input.thinkingConfig=std::move(call.thinkingConfig);
      request.input.gadgets=std::move(*call.gadgets);
      request.signal=call.signal;
      request.getGadgetPermissionContext=
        std::move(call.getGadgetPermissionContext);
      request.input.options=Json::object();
      request.input.options["model"]=call.options.at("model");
      request.input.options["isNonInteractiveSession"]=true;
      request.input.options["hasAppendSystemPrompt"]=false;
      request.input.options["agents"]=Json::array();
      request.input.options["querySource"]="hook_prompt";
      request.input.options["mcpTools"]=Json::array();
      if(call.options.contains("agentId"))
        request.input.options["agentId"]=call.options.at("agentId");
      else request.undefinedOptionKeys.insert("agentId");
      request.input.options["outputFormat"]=call.options.at("outputFormat");
      request.undefinedOptionKeys.insert("toolChoice");
      request.ownOptionKeys={"getToolPermissionContext","model",
        "toolChoice","isNonInteractiveSession","hasAppendSystemPrompt",
        "agents","querySource","mcpTools","agentId","outputFormat"};
      backend(std::move(request)).then(
        [output](const model::MessageRef* message,
          const std::exception_ptr& failure){
          if(failure){output.reject(failure);return;}
          if(!message||!*message){output.reject(std::make_exception_ptr(
            std::runtime_error("Prompt model message missing")));return;}
          const auto& envelope=**message;
          const auto& assistant=value::property(envelope,"message");
          if(value::property(envelope,"type")!="assistant"||
              !assistant.is_object()||
              !value::property(assistant,"content").is_array()){
            output.reject(std::make_exception_ptr(std::runtime_error(
              "Prompt model assistant content is missing")));
            return;
          }
          output.resolve(Json{{"message",assistant}});
        });
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
}
void bindPoetryOwnerModel(event_loop::Loop& loop,PoetryHookServices& poetry,
  query::SimpleQueryFactory factory,
  query::OwnerPreflightAwaits awaitedImports,
  query::OwnerRequestAwaits requestAwaits){
  bindPoetryModelBackend(loop,poetry,query::bindSimpleQueryModel(loop,
    std::move(factory),std::move(awaitedImports),
    std::move(requestAwaits)));
}
} // namespace batbox::instructions::hooks::runtime
