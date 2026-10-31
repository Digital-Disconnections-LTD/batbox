#include "query_engine/LifecycleInternal.hpp"

namespace batbox::query_engine::lifecycle {
Json Run::resultBase(const std::string& subtype,bool isError,double turns){
  Json sdk{{"type","result"},{"subtype",subtype}};
  sdk["duration_ms"]=services->now()-startTime;
  sdk["duration_api_ms"]=services->getTotalAPIDuration();
  sdk["is_error"]=isError;sdk["num_turns"]=turns;sdk["stop_reason"]=lastStopReason;
  sdk["session_id"]=services->getFlightId();sdk["total_cost_usd"]=services->getTotalCost();
  sdk["usage"]=*state->totalUsage;sdk["modelUsage"]=services->getModelUsage();
  Json denials=Json::array();
  for(const auto& denial:*state->permissionDenials){
    Json entry{{"tool_name",denial.gadgetName},{"tool_use_id",denial.gadgetUseID}};
    put(entry,"tool_input",deref(denial.input));denials.push_back(std::move(entry));
  }
  sdk["permission_denials"]=std::move(denials);
  put(sdk,"fast_mode_state",services->getFastModeState(mainLoopModel,deref(initialAppState->get("fastMode"))));
  sdk["uuid"]=services->randomUUID();return sdk;
}
void Run::limitResult(const std::string& subtype,double turns,std::string error){
  Json sdk=resultBase(subtype,true,turns);sdk["errors"]=Json::array({std::move(error)});
  terminalAfterEmit=true;emit(std::move(sdk),Stage::Done);
}
void Run::postSwitch(){
  if(cfg.maxBudgetUsd&&services->getTotalCost()>=*cfg.maxBudgetUsd){
    flushThen([this]{limitResult("error_max_budget_usd",turnCount,
      "Reached maximum budget ($"+value::numberToString(*cfg.maxBudgetUsd)+")");});return;
  }
  stage=Stage::RetryCheck;
}
void Run::retryCheck(){
  if(hasType(*message,"user")&&cfg.jsonSchema&&truth(*cfg.jsonSchema)){
    const double calls=services->countGadgetCalls(state->mutableMessages,services->syntheticOutputGadgetName);
    const double delta=calls-initialStructuredOutputCalls;
    const auto configured=services->env("MAX_STRUCTURED_OUTPUT_RETRIES");
    const double maximum=value::parseInt10(configured&&!configured->empty()?*configured:"5");
    if(delta>=maximum){
      flushThen([this,maximum]{limitResult("error_max_structured_output_retries",turnCount,
        "Failed to provide valid structured output after "+value::numberToString(maximum)+" attempts");});return;
    }
  }
  stage=Stage::QueryNext;
}
void Run::finishQuery(){
  result.reset();
  for(auto it=messages->rbegin();it!=messages->rend();++it){
    if(hasType(**it,"assistant")||hasType(**it,"user")){result=*it;break;}
  }
  resultType=result?string(prop(*result,"type")):"undefined";
  lastContentType="n/a";
  if(result&&hasType(*result,"assistant")){
    const auto& content=prop(prop(*result,"message"),"content");
    const auto type=content.is_array()&&!content.empty()?prop(content.back(),"type"):undefined();
    lastContentType=nullish(type)?"none":string(type);
  }
  flushThen([this]{stage=Stage::FinalResult;});
}
void Run::finalResult(){
  if(!services->isResultSuccessful(result,lastStopReason)){
    Json sdk=resultBase("error_during_execution",true,turnCount);
    const auto all=services->getInMemoryErrors();
    std::size_t start=0;
    if(errorLogWatermark){
      for(auto index=all.size();index>0;--index){
        if(all[index-1]==errorLogWatermark){start=index;break;}
      }
    }
    Json errors=Json::array({"[ede_diagnostic] result_type="+resultType+
      " last_content_type="+lastContentType+" stop_reason="+string(lastStopReason)});
    for(auto index=start;index<all.size();++index){
      const auto& value=prop(*all[index],"error");errors.push_back(value.is_discarded()?Json(nullptr):value);
    }
    sdk["errors"]=std::move(errors);emit(std::move(sdk),Stage::Done);return;
  }
  std::string textResult;bool apiError=false;
  if(result&&hasType(*result,"assistant")){
    const auto& content=prop(prop(*result,"message"),"content");
    if(content.is_array()&&!content.empty()){
      const auto& last=content.back();
      if(prop(last,"type")=="text"&&!services->isSyntheticMessage(prop(last,"text")))
        textResult=string(prop(last,"text"));
    }
    apiError=truth(prop(*result,"isApiErrorMessage"));
  }
  Json sdk=resultBase("success",apiError,turnCount);sdk["result"]=textResult;
  put(sdk,"structured_output",structuredOutput);emit(std::move(sdk),Stage::Done);
}
}
