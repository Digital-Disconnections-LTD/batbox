#include "retry/Retry.hpp"
#include "streaming/Reducer.hpp"
#include "transport/RawHttp.hpp"
#include "transport/RetryAdapter.hpp"
#include "transport/SSE.hpp"
#include "transport/openai_chat/StreamDecoder.hpp"
#include "transport/openai_chat/Translate.hpp"
#include <cassert>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace batbox;
using namespace batbox::transport;
using namespace batbox::transport::openai_chat;

namespace {
Json representativeBody(){
  return Json::parse(R"JSON({
    "model":"qwen3-coder",
    "system":[{"type":"text","text":"You are Batbox.","cache_control":{"type":"ephemeral"}},
              {"type":"text","text":"Be brief."}],
    "messages":[
      {"role":"user","content":[{"type":"text","text":"What is in this picture?"},
        {"type":"image","source":{"type":"base64","media_type":"image/png","data":"iVBORw0KGgo="}}]},
      {"role":"assistant","content":[{"type":"thinking","thinking":"Look at it.","signature":"sig"},
        {"type":"text","text":"Let me check the file."},
        {"type":"tool_use","id":"toolu_01ABCDEFGHJKLMNOPQRSTUV","name":"Read","input":{"file_path":"/tmp/a.txt"}}]},
      {"role":"user","content":[{"type":"tool_result","tool_use_id":"toolu_01ABCDEFGHJKLMNOPQRSTUV",
        "content":[{"type":"text","text":"hello world"}]},
        {"type":"text","text":"Thanks","cache_control":{"type":"ephemeral"}}]}],
    "tools":[{"name":"Read","description":"Read a file","input_schema":{"type":"object",
      "properties":{"file_path":{"type":"string"}},"required":["file_path"]},
      "cache_control":{"type":"ephemeral"}},
      {"type":"web_search_20250305","name":"web_search"}],
    "tool_choice":{"type":"auto"},
    "max_tokens":4096,"temperature":1,"stop_sequences":["</done>"],
    "thinking":{"type":"enabled","budget_tokens":2048}})JSON");
}
void requestTranslation(){
  const auto expected=Json::parse(R"JSON({
    "model":"qwen3-coder",
    "messages":[
      {"role":"system","content":"You are Batbox.\n\nBe brief."},
      {"role":"user","content":[{"type":"text","text":"What is in this picture?"},
        {"type":"image_url","image_url":{"url":"data:image/png;base64,iVBORw0KGgo="}}]},
      {"role":"assistant","content":"Let me check the file.","tool_calls":[{"id":"toolu_01ABCDEFGHJKLMNOPQRSTUV",
        "type":"function","function":{"name":"Read","arguments":"{\"file_path\":\"/tmp/a.txt\"}"}}]},
      {"role":"tool","tool_call_id":"toolu_01ABCDEFGHJKLMNOPQRSTUV","content":"hello world"},
      {"role":"user","content":"Thanks"}],
    "tools":[{"type":"function","function":{"name":"Read","description":"Read a file",
      "parameters":{"type":"object","properties":{"file_path":{"type":"string"}},"required":["file_path"]}}}],
    "tool_choice":"auto",
    "max_tokens":4096,"temperature":1,"stop":["</done>"],
    "stream":true,"stream_options":{"include_usage":true}})JSON");
  const auto translated=translateRequest(representativeBody(),true,ChatQuirks{});
  if(translated!=expected){std::cerr<<translated.dump(2)<<'\n';assert(false);}

  ChatQuirks quirks;
  quirks.maxTokensField="max_completion_tokens";quirks.systemRole="developer";
  quirks.toolIds="short";quirks.reasoning="reasoning_content";quirks.usageInStream=false;
  const auto custom=translateRequest(representativeBody(),true,quirks);
  assert(custom.at("max_completion_tokens")==4096&&!custom.contains("max_tokens"));
  assert(custom.at("messages")[0].at("role")=="developer");
  const auto shortId=custom.at("messages")[2].at("tool_calls")[0].at("id").get<std::string>();
  assert(shortId.size()==9&&shortId!="toolu_01A");
  assert(custom.at("messages")[3].at("tool_call_id")==shortId);
  assert(custom.at("messages")[2].at("reasoning_content")=="Look at it.");
  assert(custom.at("chat_template_kwargs")==Json({{"enable_thinking",true}}));
  assert(!custom.contains("stream_options"));
  assert(translateRequest(representativeBody(),true,quirks).at("messages")[3].at("tool_call_id")==shortId);

  quirks=ChatQuirks{};quirks.reasoning="openai";
  assert(translateRequest(representativeBody(),false,quirks).at("reasoning_effort")=="low");
  auto big=representativeBody();big["thinking"]["budget_tokens"]=12000;
  assert(translateRequest(big,false,quirks).at("reasoning_effort")=="high");
  quirks.reasoning="openrouter";
  assert(translateRequest(representativeBody(),false,quirks).at("reasoning")==Json({{"max_tokens",2048}}));
  const auto plain=translateRequest(representativeBody(),false,ChatQuirks{});
  assert(plain.at("stream")==false&&!plain.contains("stream_options"));
  assert(!plain.contains("reasoning_effort")&&!plain.contains("chat_template_kwargs"));
  assert(!plain.at("messages")[2].contains("reasoning_content"));

  auto choices=representativeBody();
  choices["tool_choice"]={{"type","any"},{"disable_parallel_tool_use",true}};
  auto forced=translateRequest(choices,false,ChatQuirks{});
  assert(forced.at("tool_choice")=="required"&&forced.at("parallel_tool_calls")==false);
  choices["tool_choice"]={{"type","tool"},{"name","Read"}};
  assert(translateRequest(choices,false,ChatQuirks{}).at("tool_choice")==
    Json({{"type","function"},{"function",{{"name","Read"}}}}));

  auto results=Json::parse(R"JSON({"model":"m","max_tokens":10,"messages":[
    {"role":"assistant","content":[{"type":"tool_use","id":"a1","name":"Shot","input":{}},
                                   {"type":"tool_use","id":"","name":"Bash","input":{"command":"false"}}]},
    {"role":"user","content":[
      {"type":"tool_result","tool_use_id":"a1","content":[{"type":"image","source":{"type":"base64",
        "media_type":"image/jpeg","data":"AAAA"}},{"type":"text","text":"captured"}]},
      {"type":"tool_result","tool_use_id":"","is_error":true,"content":"exit 1"}]}]})JSON");
  const auto hoisted=translateRequest(results,false,ChatQuirks{});
  const auto& messages=hoisted.at("messages");
  assert(messages.size()==4);
  assert(messages[0].at("content").is_null()&&messages[0].at("tool_calls").size()==2);
  assert(messages[0].at("tool_calls")[0].at("id")=="a1");
  const auto emptyId=messages[0].at("tool_calls")[1].at("id").get<std::string>();
  assert(emptyId.starts_with("call_"));
  assert(messages[1]==Json({{"role","tool"},{"tool_call_id","a1"},{"content","captured"}}));
  assert(messages[2]==Json({{"role","tool"},{"tool_call_id",emptyId},{"content","Error: exit 1"}}));
  assert(messages[3].at("role")=="user"&&messages[3].at("content")[1].at("image_url").at("url")==
    "data:image/jpeg;base64,AAAA");
}

std::vector<Json> decode(const std::string& sse,std::size_t requestBytes=400,std::size_t chunk=7){
  SSEDecoder frames;
  StreamDecoder decoder("wire-model",requestBytes);
  std::vector<Json> events;
  for(std::size_t i=0;i<sse.size();i+=chunk)
    for(const auto& event:frames.decode(std::string_view(sse).substr(i,chunk)))
      for(auto& translated:decoder.consume(event))events.push_back(std::move(translated));
  for(const auto& event:frames.flush())
    for(auto& translated:decoder.consume(event))events.push_back(std::move(translated));
  for(auto& translated:decoder.finish())events.push_back(std::move(translated));
  return events;
}
struct Reduced { Json messages=Json::array(); Json usage; Json stopReason; };
Reduced reduce(const std::vector<Json>& events){
  streaming::ReducerConfig config;
  config.model="profile::wire-model";
  config.randomUUID=[]{return std::string("uuid");};
  config.timestamp=[]{return std::string("now");};
  config.calculateUSDCost=[](const std::string&,const Json&){return 0.0;};
  config.addToTotalFlightCost=[](double,const Json&,const std::string&){return 0.0;};
  config.getErrorMessageIfRefusal=[](const Json&,const std::string&)->std::optional<Json>{return std::nullopt;};
  config.createAssistantAPIErrorMessage=[](const std::string& text,const std::string&,const std::string&){
    return Json{{"type","assistant"},{"error",text}};};
  streaming::Reducer reducer(std::move(config));
  Reduced result;
  for(const auto& event:events)(void)reducer.consume(event);
  reducer.finish();
  for(const auto& message:reducer.messages())result.messages.push_back(*message);
  result.usage=reducer.usage();result.stopReason=reducer.stopReason();
  return result;
}
std::string frames(const std::vector<std::string>& chunks,bool done=true){
  std::string sse;
  for(const auto& chunk:chunks)sse+="data: "+chunk+"\n\n";
  if(done)sse+="data: [DONE]\n\n";
  return sse;
}
std::vector<std::string> types(const std::vector<Json>& events){
  std::vector<std::string> names;
  for(const auto& event:events)names.push_back(event.at("type").get<std::string>());
  return names;
}
template<class Error>std::string throws(const std::function<void()>& action){
  try{action();}catch(const Error& error){return error.what();}
  assert(false&&"expected an exception");return {};
}

void plainText(){
  const auto events=decode(frames({
    R"({"id":"c1","object":"chat.completion.chunk","model":"served","choices":[{"index":0,"delta":{"role":"assistant","content":""},"finish_reason":null}]})",
    R"({"id":"c1","choices":[{"index":0,"delta":{"content":"Hel"},"finish_reason":null}]})",
    R"({"id":"c1","choices":[{"index":0,"delta":{"content":"lo"},"finish_reason":null}]})",
    R"({"id":"c1","choices":[{"index":0,"delta":{},"finish_reason":"stop"}]})",
    R"({"id":"c1","choices":[],"usage":{"prompt_tokens":11,"completion_tokens":2,"total_tokens":13}})"}));
  assert(types(events)==std::vector<std::string>({"message_start","content_block_start",
    "content_block_delta","content_block_delta","content_block_stop","message_delta","message_stop"}));
  const auto& start=events[0].at("message");
  assert(start.at("model")=="wire-model"&&start.at("id").get<std::string>().starts_with("msg_"));
  assert(start.at("usage")==Json({{"input_tokens",0},{"output_tokens",0}}));
  const auto reduced=reduce(events);
  assert(reduced.messages.size()==1);
  assert(reduced.messages[0].at("message").at("content")[0].at("text")=="Hello");
  assert(reduced.messages[0].at("message").at("model")=="wire-model");
  assert(reduced.stopReason=="end_turn");
  assert(reduced.usage.at("input_tokens")==11&&reduced.usage.at("output_tokens")==2);
  assert(reduced.messages[0].at("message").at("usage").at("output_tokens")==2);
}
void parallelTools(){
  const auto events=decode(frames({
    R"({"id":"c2","choices":[{"index":0,"delta":{"role":"assistant","content":""}}]})",
    R"({"id":"c2","choices":[{"index":0,"delta":{"content":"Checking both."}}]})",
    R"({"id":"c2","choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"id":"call_AAAAAAAAAAAA","type":"function","function":{"name":"Read","arguments":""}}]}}]})",
    R"({"id":"c2","choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"function":{"arguments":"{\"file_"}}]}}]})",
    R"({"id":"c2","choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"function":{"arguments":"path\":\"/a\"}"}}]}}]})",
    R"({"id":"c2","choices":[{"index":0,"delta":{"tool_calls":[{"index":1,"id":"call_BBBBBBBBBBBB","type":"function","function":{"name":"Bash","arguments":"{\"comm"}}]}}]})",
    R"({"id":"c2","choices":[{"index":0,"delta":{"tool_calls":[{"index":1,"function":{"arguments":"and\":\"echo hi\"}"}}]}}]})",
    R"({"id":"c2","choices":[{"index":0,"delta":{},"finish_reason":"tool_calls"}]})",
    R"({"id":"c2","choices":[],"usage":{"prompt_tokens":120,"completion_tokens":30,"total_tokens":150,"prompt_tokens_details":{"cached_tokens":20}}})"}));
  std::vector<std::size_t> starts;
  std::size_t fragments=0;
  for(const auto& event:events){
    if(event.at("type")=="content_block_start")starts.push_back(event.at("index").get<std::size_t>());
    if(event.at("type")=="content_block_delta"&&event.at("delta").at("type")=="input_json_delta")++fragments;
  }
  assert(starts==std::vector<std::size_t>({0,1,2})&&fragments==4);
  const auto reduced=reduce(events);
  assert(reduced.messages.size()==3);
  assert(reduced.messages[0].at("message").at("content")[0].at("text")=="Checking both.");
  const auto& read=reduced.messages[1].at("message").at("content")[0];
  assert(read.at("type")=="tool_use"&&read.at("id")=="call_AAAAAAAAAAAA"&&read.at("name")=="Read");
  assert(read.at("input")==Json({{"file_path","/a"}}));
  const auto& bash=reduced.messages[2].at("message").at("content")[0];
  assert(bash.at("id")=="call_BBBBBBBBBBBB"&&bash.at("input")==Json({{"command","echo hi"}}));
  assert(reduced.stopReason=="tool_use");
  assert(reduced.usage.at("input_tokens")==100&&reduced.usage.at("cache_read_input_tokens")==20);
  assert(reduced.usage.at("output_tokens")==30);
}
void synthesizedIds(){
  const auto events=decode(frames({
    R"({"choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"id":"0","function":{"name":"Read","arguments":"{}"}}]}}]})",
    R"({"choices":[{"index":0,"delta":{"tool_calls":[{"index":1,"id":"0","function":{"name":"Glob","arguments":"{\"pattern\":\"*\"}"}}]}}]})",
    R"({"choices":[{"index":0,"delta":{"tool_calls":[{"index":2,"id":"","function":{"arguments":"{\"a\""}}]}}]})",
    R"({"choices":[{"index":0,"delta":{"tool_calls":[{"index":2,"function":{"name":"Grep","arguments":":1}"}}]}}]})",
    R"({"choices":[{"index":0,"delta":{},"finish_reason":"stop"}]})"},false));
  const auto reduced=reduce(events);
  assert(reduced.messages.size()==3&&reduced.stopReason=="tool_use");
  std::set<std::string> ids;
  for(const auto& message:reduced.messages){
    const auto id=message.at("message").at("content")[0].at("id").get<std::string>();
    assert(id.starts_with("toolu_")&&id.size()==30);
    ids.insert(id);
  }
  assert(ids.size()==3);
  assert(reduced.messages[2].at("message").at("content")[0].at("input")==Json({{"a",1}}));
}
void reasoningStream(){
  const auto events=decode(frames({
    R"({"choices":[{"index":0,"delta":{"role":"assistant","reasoning_content":"Let me "}}]})",
    R"({"choices":[{"index":0,"delta":{"reasoning":"think."}}]})",
    R"({"choices":[{"index":0,"delta":{"content":"Answer."}}]})",
    R"({"choices":[{"index":0,"delta":{},"finish_reason":"stop"}],"usage":{"prompt_tokens":5,"completion_tokens":7}})"}));
  const auto reduced=reduce(events);
  assert(reduced.messages.size()==2);
  const auto& thinking=reduced.messages[0].at("message").at("content")[0];
  assert(thinking.at("type")=="thinking"&&thinking.at("thinking")=="Let me think."&&thinking.at("signature")=="");
  assert(reduced.messages[1].at("message").at("content")[0].at("text")=="Answer.");
  assert(reduced.usage.at("output_tokens")==7&&reduced.usage.at("input_tokens")==5);
}
void doneWithoutUsage(){
  const auto events=decode(frames({
    R"({"choices":[{"index":0,"delta":{"content":"12345678"}}]})",
    R"({"choices":[{"index":0,"delta":{},"finish_reason":"length"}]})"}),400);
  const auto& delta=events.at(events.size()-2);
  assert(delta.at("type")=="message_delta"&&delta.at("delta").at("stop_reason")=="max_tokens");
  assert(delta.at("usage").at("input_tokens")==100&&delta.at("usage").at("output_tokens")==2);
  const auto noFinish=decode("data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"x\"}}]}\n\ndata: [DONE]\n\n");
  assert(noFinish.at(noFinish.size()-2).at("delta").at("stop_reason")=="end_turn");
  const auto empty=decode("data: [DONE]\n\n");
  assert(types(empty)==std::vector<std::string>({"message_start","message_delta","message_stop"}));
  (void)reduce(empty);
  const auto withoutDone=decode(frames({R"({"choices":[{"index":0,"delta":{"content":"ok"},"finish_reason":"stop"}]})"},false));
  assert(types(withoutDone).back()=="message_stop");
}
void streamFailures(){
  const auto overloaded=throws<APIError>([]{
    (void)decode(frames({R"({"choices":[{"index":0,"delta":{"content":"a"}}]})",
      R"({"error":{"message":"upstream overloaded","code":503}})"}));});
  assert(overloaded.find("\"type\":\"overloaded_error\"")!=std::string::npos);
  assert(overloaded.find("upstream overloaded")!=std::string::npos);
  const auto named=throws<APIError>([]{(void)decode("event: error\ndata: {\"error\":{\"message\":\"boom\"}}\n\n");});
  assert(named.find("boom")!=std::string::npos);
  assert(throws<std::runtime_error>([]{(void)decode("data: {not json}\n\n");}).find("Malformed")!=std::string::npos);
  assert(throws<std::runtime_error>([]{
    (void)decode(frames({R"({"choices":[{"index":0,"delta":{"content":"cut"}}]})"},false));}).find("ended")!=std::string::npos);
  assert(throws<std::runtime_error>([]{(void)decode(frames({
    R"({"choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"id":"call_AAAAAAAAAAAA","function":{"name":"A","arguments":"{"}}]}}]})",
    R"({"choices":[{"index":0,"delta":{"tool_calls":[{"index":1,"id":"call_BBBBBBBBBBBB","function":{"name":"B","arguments":"{"}}]}}]})",
    R"({"choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"function":{"arguments":"}"}}]}}]})"}));}).find("interleaved")!=std::string::npos);
  assert(throws<std::runtime_error>([]{(void)decode(frames({
    R"({"choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"function":{"arguments":"{}"}}]}}]})",
    R"({"choices":[{"index":0,"delta":{},"finish_reason":"tool_calls"}]})"}));}).find("without a function name")!=std::string::npos);
}
void nonStreaming(){
  const auto response=translateResponse(Json::parse(R"JSON({"id":"cmpl","object":"chat.completion","model":"served",
    "choices":[{"index":0,"message":{"role":"assistant","content":"Done.","reasoning_content":"why",
      "tool_calls":[{"id":"call_CCCCCCCCCCCC","type":"function","function":{"name":"Bash","arguments":"{\"command\":\"ls\"}"}},
                    {"id":"1","type":"function","function":{"name":"Read","arguments":"not json"}}]},
      "finish_reason":"stop"}],
    "usage":{"prompt_tokens":50,"completion_tokens":9,"prompt_tokens_details":{"cached_tokens":10}}})JSON"),"wire-model");
  assert(response.at("type")=="message"&&response.at("role")=="assistant"&&response.at("model")=="wire-model");
  const auto& content=response.at("content");
  assert(content.size()==4);
  assert(content[0]==Json({{"type","thinking"},{"thinking","why"},{"signature",""}}));
  assert(content[1]==Json({{"type","text"},{"text","Done."}}));
  assert(content[2].at("id")=="call_CCCCCCCCCCCC"&&content[2].at("input")==Json({{"command","ls"}}));
  assert(content[3].at("id").get<std::string>().starts_with("toolu_")&&content[3].at("input")==Json::object());
  assert(response.at("stop_reason")=="tool_use");
  assert(response.at("usage")==Json({{"input_tokens",40},{"output_tokens",9},
    {"cache_read_input_tokens",10},{"cache_creation_input_tokens",0}}));
  const auto empty=translateResponse(Json::parse(R"({"choices":[{"message":{"content":null},"finish_reason":"length"}]})"),"m");
  assert(empty.at("stop_reason")=="max_tokens"&&empty.at("usage").at("input_tokens")==0);
}
retry::ErrorRecord record(const APIError& error){
  try{throw error;}catch(...){return errorRecordFromException(std::current_exception());}
}
void errors(){
  const auto unauthorized=translateError(401,Json::parse(
    R"({"error":{"message":"Invalid API key","type":"invalid_request_error","code":"invalid_api_key"}})"),"",
    Json::object(),"http://127.0.0.1:9931/v1");
  assert(std::string(unauthorized.what())=="401 from http://127.0.0.1:9931: check auth_token · Invalid API key");
  assert(unauthorized.body.at("type")=="error"&&unauthorized.body.at("error").at("type")=="authentication_error");
  const auto limited=translateError(429,Json::parse(R"({"error":{"message":"slow down"}})"),"",
    Json{{"retry-after","1"}},"http://h");
  assert(limited.body.at("error").at("type")=="rate_limit_error"&&limited.headers.at("retry-after")=="1");
  assert(std::string(limited.what()).find("\"type\":\"rate_limit_error\"")!=std::string::npos);
  const auto busy=translateError(503,nullptr,"Service Unavailable",Json::object(),"http://h");
  assert(busy.body.at("error").at("message")=="Service Unavailable");
  assert(retry::is529Error(record(busy)));
  assert(translateError(500,nullptr,"",Json::object(),"http://h").body.at("error").at("type")=="api_error");
  assert(translateError(422,nullptr,"bad",Json::object(),"http://h").body.at("error").at("type")=="invalid_request_error");

  retry::Dependencies deps;
  const auto overflow=translateError(400,Json{{"error",{{"message",
    "This model's maximum context length is 32768 tokens. However, you requested 51234 tokens "
    "(1234 in the messages, 50000 in the completion). Please reduce the length of the messages or completion."}}}},
    "",Json::object(),"http://h");
  const auto parsed=retry::parseMaxTokensContextOverflowError(record(overflow),deps);
  assert(parsed&&parsed->inputTokens==1234&&parsed->maxTokens==50000&&parsed->contextLimit==32768);
  const auto newer=translateError(400,Json{{"error",{{"message",
    "'max_tokens' or 'max_completion_tokens' is too large: 50000. This model's maximum context length is "
    "32768 tokens and your request has 2000 input tokens (50000 > 32768 - 2000)."}}}},"",Json::object(),"http://h");
  const auto parsedNewer=retry::parseMaxTokensContextOverflowError(record(newer),deps);
  assert(parsedNewer&&parsedNewer->inputTokens==2000&&parsedNewer->maxTokens==50000);
  const auto tooLong=translateError(400,Json{{"error",{{"message",
    "This model's maximum context length is 32768 tokens. However, your request has 40000 input tokens. "
    "Please reduce the length of the input messages."}}}},"",Json::object(),"http://h");
  assert(std::string(tooLong.what()).find("prompt is too long: 40000 tokens > 32768 maximum")!=std::string::npos);
  const auto llama=translateError(400,Json::parse(R"({"error":{"code":400,"message":"the request exceeds the available context size, try increasing it","type":"exceed_context_size_error","n_prompt_tokens":5000,"n_ctx":4096}})"),
    "",Json::object(),"http://h");
  assert(std::string(llama.what()).find("prompt is too long: 5000 tokens > 4096 maximum")!=std::string::npos);
}
void retryPolicy(){
  assert(isLoopbackUrl("http://127.0.0.1:9931")&&isLoopbackUrl("http://localhost/v1"));
  assert(isLoopbackUrl("http://[::1]:8080")&&isLoopbackUrl("http://0.0.0.0:8000"));
  assert(!isLoopbackUrl("https://api.example.com")&&!isLoopbackUrl("http://10.0.0.5:8000"));
  struct Run { int attempts=0; std::vector<double> sleeps; std::string error; };
  const auto run=[](long status,std::optional<double> cap,const Json& headers=Json::object()){
    Run result;
    retry::Dependencies deps;
    deps.wire.retryAfter="retry-after";deps.wire.shouldRetry="x-should-retry";
    deps.maxRetriesCap=cap;
    if(cap)deps.maxRetryDelayMs=2000;
    deps.getClient=[]{return Json::object();};
    deps.operation=[&](const Json&,double,const retry::RetryContext&)->Json{
      ++result.attempts;
      throw retry::RetryFailure(record(translateError(status,nullptr,"failure",headers,"http://127.0.0.1:9931")));
    };
    deps.yield=[](const Json&){};
    deps.sleep=[&](double ms,std::stop_token){result.sleeps.push_back(ms);};
    deps.random=[]{return 0.0;};
    deps.nowMs=[]{return 0.0;};
    deps.createSystemAPIErrorMessage=[](const retry::ErrorRecord&,double,double,double){return Json::object();};
    deps.isFastModeEnabled=[]{return false;};
    deps.isSubscriber=[]{return false;};
    deps.isEnterpriseSubscriber=[]{return false;};
    deps.isDefaultPremiumModel=[](const std::string&){return false;};
    deps.getOAuthAccessToken=[]()->std::optional<std::string>{return std::nullopt;};
    deps.clearApiKeyHelperStash=[]{};
    deps.disableKeepAlive=[]{};
    deps.logForDebugging=[](const std::string&){};
    deps.logError=[](const std::string&){};
    retry::RetryOptions options;options.model="m";
    try{(void)retry::withRetry(options,deps);}catch(const std::exception& error){result.error=error.what();}
    return result;
  };
  assert(run(401,std::nullopt).attempts==1);
  assert(run(403,std::nullopt).attempts==1);
  assert(run(401,2.0).attempts==1);
  const auto server=run(500,2.0);
  assert(server.attempts==3&&server.sleeps.size()==2);
  for(const auto delay:server.sleeps)assert(delay<=2000);
  const auto limited=run(429,2.0,Json{{"retry-after","30"}});
  assert(limited.attempts==3&&limited.sleeps==std::vector<double>({2000,2000}));
  assert(run(500,std::nullopt).attempts==11);
}
} // namespace

int main(){
  requestTranslation();
  plainText();
  parallelTools();
  synthesizedIds();
  reasoningStream();
  doneWithoutUsage();
  streamFailures();
  nonStreaming();
  errors();
  retryPolicy();
  std::cout<<"OpenAI Chat translation, stream decoding, errors and retry policy checks passed\n";
  return 0;
}
