#include "application_mcp_elicitation/ModelBinding.hpp"
#include "messages/TextContent.hpp"
#include <stdexcept>

namespace batbox::application_mcp_elicitation {
DateTimeQuery makeDateTimeQuery(event_loop::Loop& owner,
    std::shared_ptr<application_model::LocalModelService> model){
  return [&owner,model=std::move(model)](const Json& systemPoetry,
      const std::string& userPoetry,std::stop_token signal){
    event_loop::Deferred<std::string> output(owner);
    try{
      if(!model)throw std::logic_error("Date parsing model is unavailable");
      model::SimpleQueryInputs query;
      query.systemPoetry=systemPoetry;query.userPoetry=userPoetry;query.signal=signal;
      query.options={{"querySource","mcp_datetime_parse"},{"agents",Json::array()},
        {"isNonInteractiveSession",false},{"hasAppendSystemPrompt",false},
        {"mcpTools",Json::array()},{"enablePromptCaching",false}};
      model->querySmallFast(std::move(query)).then(
        [output](const model::MessageRef* value,const std::exception_ptr& error){
          if(error){output.reject(error);return;}
          try{
            if(!value||!*value)throw std::runtime_error("Date parsing returned no response");
            output.resolve(messages::extractTextContent(
              value::property(value::property(**value,"message"),"content")));
          }catch(...){output.reject(std::current_exception());}
        });
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
}
}
