#include "user_input/InputInternal.hpp"
#include "messages/TextContent.hpp"
#include "user_input/CommandMessages.hpp"
#include <stdexcept>

namespace batbox::user_input::internal {
InputState::InputState(UserInputRequest value,UserInputServices& deps):
  request(std::move(value)),services(deps),normalizedInput(request.input),result(deps.loop){}
void fail(const std::shared_ptr<InputState>& state,std::exception_ptr error){
  state->result.reject(error);
}
std::string text(const Json& value){
  return value.is_string()?value.get<std::string>():std::string{};
}
void checkpoint(InputState& state,const std::string& name){
  if(state.services.queryCheckpoint)state.services.queryCheckpoint(name);
}
void appendMetadata(const std::shared_ptr<InputState>& state,InputResult& result){
  if(state->imageMetadata.empty())return;
  Json content=Json::array();
  for(const auto& item:state->imageMetadata)
    content.push_back(Json{{"type","text"},{"text",item}});
  result.messages.push_back(messages::createUserMessage(
    Json{{"content",content},{"isMeta",true}},state->services.messages));
}
void finishBase(const std::shared_ptr<InputState>& state,InputResult result){
  try{
    appendMetadata(state,result);
    checkpoint(*state,"query_process_user_input_base_end");
    if(!result.shouldQuery){state->result.resolve(std::move(result));return;}
    runHooks(state,std::move(result));
  }catch(...){fail(state,std::current_exception());}
}
void prepareInput(const std::shared_ptr<InputState>& state){
  try{
    const auto& input=state->request.input;
    if(input.is_string()){
      state->inputString=input.get<std::string>();
      processPasted(state);return;
    }
    if(!input.is_array())throw std::invalid_argument("Prompt input must be text or content blocks");
    if(input.empty()){processPasted(state);return;}
    if(state->blockIndex==0)checkpoint(*state,"query_image_processing_start");
    if(state->blockIndex<input.size()){
      const auto block=input.at(state->blockIndex++);
      if(value::property(block,"type")!="image"){
        state->processed.push_back(block);
        prepareInput(state);return;
      }
      if(!state->services.resizeImageBlock)
        throw std::invalid_argument("Missing user input binding: resizeImageBlock");
      auto resized=state->services.resizeImageBlock(block);
      resized.then([state](const Json* value,const std::exception_ptr& error){
        if(error){fail(state,error);return;}
        try{
          const auto& dims=value::property(*value,"dimensions");
          if(value::truthy(dims)){
            if(!state->services.createImageMetadataText)
              throw std::invalid_argument("Missing user input binding: createImageMetadataText");
            const auto metadata=state->services.createImageMetadataText(dims,std::nullopt);
            if(metadata&& !metadata->empty())state->imageMetadata.push_back(*metadata);
          }
          state->processed.push_back(value->at("block"));
          prepareInput(state);
        }catch(...){fail(state,std::current_exception());}
      });
      return;
    }
    state->normalizedInput=state->processed;
    checkpoint(*state,"query_image_processing_end");
    const auto& last=state->processed.back();
    if(value::property(last,"type")=="text"){
      state->inputString=text(value::property(last,"text"));
      state->preceding=Json::array();
      for(std::size_t i=0;i+1<state->processed.size();++i)
        state->preceding.push_back(state->processed[i]);
    }else state->preceding=state->processed;
    processPasted(state);
  }catch(...){fail(state,std::current_exception());}
}
} // namespace batbox::user_input::internal

namespace batbox::user_input {
event_loop::Deferred<InputResult> processUserInput(const UserInputRequest& request,
  UserInputServices& services){
  auto state=std::make_shared<internal::InputState>(request,services);
  try{
    if(request.mode=="prompt"&&request.input.is_string()&&!request.isMeta&&
       services.setUserInputOnProcessing)
      services.setUserInputOnProcessing(request.input.get<std::string>());
    internal::checkpoint(*state,"query_process_user_input_base_start");
    if(!services.getAppState)throw std::invalid_argument("Missing user input binding: getAppState");
    state->appState=services.getAppState();
    const auto& permission=value::property(value::property(state->appState,
      "toolPermissionContext"),"mode");
    if(permission.is_string())state->permissionMode=permission.get<std::string>();
    internal::prepareInput(state);
  }catch(...){state->result.reject(std::current_exception());}
  return state->result;
}
} // namespace batbox::user_input
