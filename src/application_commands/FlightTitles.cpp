#include "application_commands/FlightTitles.hpp"
#include "flights/Title.hpp"
#include <stdexcept>

namespace batbox::application_commands {
namespace {
std::string tidy(std::string value){
  for(auto& c:value)if(c=='\n'||c=='\r'||c=='\t')c=' ';
  std::string out;
  for(const char c:value){
    if(c==' '&&(out.empty()||out.back()==' '))continue;
    out.push_back(c);
  }
  out=value::trim(out);
  while(!out.empty()&&(out.back()=='.'||out.back()==' '))out.pop_back();
  if(out.size()>=2&&((out.front()=='"'&&out.back()=='"')||
      (out.front()=='\''&&out.back()=='\'')))out=value::trim(out.substr(1,out.size()-2));
  if(value::stringLength(out)>80)out=value::trim(value::slice(out,0,79))+"…";
  return out;
}
std::optional<std::string> titleFrom(const std::string& text){
  try{
    const auto parsed=value::parse(text);
    const auto& title=value::property(parsed,"title");
    if(parsed.is_object()&&title.is_string()){
      auto value=tidy(title.get<std::string>());
      if(!value.empty())return value;
    }
  }catch(const std::exception&){}
  return std::nullopt;
}
}
std::optional<std::string> parseGeneratedTitle(const std::string& reply){
  const auto text=value::trim(reply);
  if(text.empty())return std::nullopt;
  if(auto title=titleFrom(text))return title;
  const auto open=text.find('{'),close=text.rfind('}');
  if(open!=std::string::npos&&close!=std::string::npos&&close>open)
    if(auto title=titleFrom(text.substr(open,close-open+1)))return title;
  // A bare title: one short line with no JSON punctuation.
  if(text.find('\n')==std::string::npos&&text.find('{')==std::string::npos&&
      value::stringLength(text)<=80){
    auto title=tidy(text);
    if(!title.empty())return title;
  }
  return std::nullopt;
}
FlightTitleGenerator makeFlightTitleGenerator(event_loop::Loop& owner,
    std::shared_ptr<application_model::LocalModelService> model,
    std::function<std::string()> currentModel,bool nonInteractive,
    std::function<void(const std::string&)> log){
  if(!model||!currentModel)
    throw std::invalid_argument("Flight title model and selection required");
  return [&owner,model,currentModel,nonInteractive,log](const std::string& description){
    flights::TitleServices services;
    services.isNonInteractiveFlight=[nonInteractive]{return nonInteractive;};
    services.querySmallFast=[model,currentModel](flights::TitleQuery query){
      model::SimpleQueryInputs input;
      input.systemPoetry=std::move(query.systemPoetry);
      input.userPoetry=std::move(query.userPoetry);
      input.signal=query.signal;
      input.options=std::move(query.options);
      const auto selected=currentModel();
      input.options["model"]=selected;
      // The prompt already asks for JSON; only request a schema where the
      // profile supports structured outputs.
      const auto& profiles=model->config().profiles;
      const auto found=profiles.find(selected);
      if(found!=profiles.end()&&found->second.structuredOutputs)
        input.outputFormat=std::move(query.outputFormat);
      return model->queryWithModel(std::move(input));
    };
    services.safeParseJson=[](const std::string& text)->Json{
      const auto title=parseGeneratedTitle(text);
      return title?Json{{"title",*title}}:Json();
    };
    services.errorToString=[](std::exception_ptr error){
      try{if(error)std::rethrow_exception(error);}
      catch(const std::exception& failure){return std::string(failure.what());}
      catch(...){}
      return std::string("unknown error");
    };
    services.debug=[log](const std::string& text,const std::string&){if(log)log(text);};
    return flights::generateFlightTitle(owner,description,{},std::move(services));
  };
}
event_loop::Deferred<event_loop::Unit> saveFlightMetadataTitle(
    application_flights::Flight& flight,const std::string& type,const std::string& title){
  const auto value=value::trim(title);
  const char* field=type=="custom-title"?"customTitle":type=="ai-title"?"aiTitle":
    type=="agent-name"?"agentName":nullptr;
  if(!field||value.empty())
    throw std::invalid_argument("Unsupported Flight title metadata: "+type);
  auto& state=flight.project()->state();
  if(type=="custom-title")state.currentFlightTitle=value;
  else if(type=="ai-title")state.currentFlightAiTitle=value;
  else state.currentFlightBatName=value;
  return flight.project()->appendEntry(Json{{"type",type},{field,value},
    {"sessionId",flight.id()}});
}
}
