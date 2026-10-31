#include "application/ModelBinding.hpp"
#include "application/Async.hpp"
#include "client_factory/tls/IndigenousClientBinding.hpp"
#include "normalization/Attachments.hpp"
#include "memory_prefetch/Prefetch.hpp"
#include "poetryEntries/SystemPoetry.hpp"
#include "requests/RequestBuilder.hpp"
#include "runtime/Platform.hpp"
#include "shell/ShellQuote.hpp"
#include "gadgets/InputNormalization.hpp"
#include "gadgets/Schema.hpp"
#include "gadgets/live_roster/LiveRoster.hpp"
#include <cmath>
#include <mutex>
#include <stdexcept>

namespace batbox::application {
namespace {
gadgets::GadgetInputServices inputBindings(){
  gadgets::GadgetInputServices values;
  values.bashGadgetName="Bash";values.fileEditGadgetName="Edit";
  values.fileWriteGadgetName="Write";values.exitPlanModeGadgetName="ExitPlanMode";
  return values;
}
gadgets::GadgetSchemaServices schemaBindings(){
  gadgets::GadgetSchemaServices services;
  services.jsonStringify=[](const Json& value){return value.dump();};
  services.convertInputSchema=[](const gadgets::Gadget& gadget){
    if(gadget.inputSchema.is_object())return gadget.inputSchema;
    if(gadget.inputJSONSchema.is_object())return gadget.inputJSONSchema;
    throw std::invalid_argument("Tool has no JSON input schema: "+gadget.name);
  };
  services.strictGadgetsEnabled=[] {return false;};
  services.modelSupportsStructuredOutputs=[](const std::string&){return false;};
  services.batSwarmsEnabled=[] {return false;};
  services.directProvider=[] {return false;};
  services.exitPlanModeGadgetName="ExitPlanMode";
  services.batGadgetName="Agent";
  services.wire={"name","description","input_schema","strict",
    "eager_input_streaming","defer_loading","cache_control"};
  return services;
}
Json normalizeAttachmentForGadgets(
    const std::shared_ptr<messages::Services>& messages,
    const gadgets::Gadgets& active,const Json& value){
  normalization::AttachmentServices services;
  services.messageServices=messages.get();
  services.batGadgetName="Agent";services.fileReadGadgetName="Read";
  services.bashGadgetName="Bash";services.fileEditGadgetName="Edit";
  services.fileWriteGadgetName="Write";services.globGadgetName="Glob";
  services.grepGadgetName="Grep";
  services.missionOutputGadgetName="TaskOutput";
  services.memoryHeader=[](const std::string& path,const Json& mtime){
    return memory_prefetch::memoryHeader(path,value::numberOr(mtime),
      static_cast<double>(platform::nowMillis()));
  };
  services.formatNumber=[](double number){return value::numberToString(number);};
  services.jsonStringify=[](const Json& item){return item.dump();};
  services.shellQuote=[](const Json& arguments){
    return shell_quote::quote(arguments.get<std::vector<Json>>());
  };
  for(const auto* gadget:active){
    if(!gadget)continue;
    if(gadget->name=="Read")services.mapFileReadResult=gadget->mapGadgetResultToGadgetResultBlockParam;
    else if(gadget->name=="Bash")services.mapBashResult=gadget->mapGadgetResultToGadgetResultBlockParam;
  }
  return normalization::normalizeSimpleAttachmentForAPI(value,services);
}
}
std::function<Json(const Json&)> makeAttachmentNormalizer(
    const std::shared_ptr<messages::Services>& messages,
    std::shared_ptr<gadgets::live_roster::Roster> roster){
  if(!messages)throw std::invalid_argument("Attachment message services required");
  const std::weak_ptr<gadgets::live_roster::Roster> weak=roster;
  return [messages,weak](const Json& value){
    // File restoration after compact uses the same result formatters as real
    // gadgets. Retain this immutable generation for the entire mapping call;
    // roster refresh may publish a new generation from the owner loop.
    const auto live=weak.lock();
    const auto snapshot=live?live->current():nullptr;
    gadgets::Gadgets active;
    if(snapshot){
      if(const auto* read=snapshot->find("Read"))active.push_back(read);
      if(const auto* bash=snapshot->find("Bash"))active.push_back(bash);
    }
    return normalizeAttachmentForGadgets(messages,active,value);
  };
}
std::shared_ptr<application_model::LocalModelService> makeLocalModel(
    event_loop::Loop& owner,const Config& config,
    const std::shared_ptr<messages::Services>& messages,
    std::function<void(const std::string&)> diagnostic,
    std::shared_ptr<gadgets::live_roster::Roster> roster){
  if(config.model.empty())throw std::invalid_argument(
    "Set --model or BATBOX_MODEL to a model served by the Messages API");
  if(!messages)throw std::invalid_argument("Model message services required");
  application_model::Config model;
  model.client.baseUrl=config.baseUrl;
  model.client.messagesPath="/v1/messages";
  model.mainModel=model.smallModel=model.visionModel=config.model;
  model.timeBasedMicrocompact=config.settings.value("timeBasedMicrocompact",false);
  if(config.settings.contains("timeBasedMicrocompactGapMinutes")){
    const auto gap=config.settings.at("timeBasedMicrocompactGapMinutes").get<double>();
    if(!std::isfinite(gap)||gap<=0)
      throw std::invalid_argument("timeBasedMicrocompactGapMinutes must be positive");
    model.timeBasedGapMinutes=gap;
  }
  if(config.settings.contains("timeBasedMicrocompactKeepRecent")){
    const auto count=config.settings.at("timeBasedMicrocompactKeepRecent").get<int>();
    if(count<=0)throw std::invalid_argument("timeBasedMicrocompactKeepRecent must be positive");
    model.timeBasedKeepRecent=count;
  }
  const auto addProfile=[&model,legacyImplicit=config.profiles.empty()](const std::string& name,
      const InferenceProfile& configured){
    application_model::ModelProfile profile;
    profile.id=name;profile.wireModel=configured.model;
    profile.contextTokens=static_cast<double>(configured.contextWindow);
    profile.maxOutputTokens=static_cast<double>(configured.maxOutputTokens);
    profile.poetryStashing=configured.poetryStashing;
    profile.thinking=configured.thinking;
    profile.adaptiveThinking=configured.adaptiveThinking;
    profile.structuredOutputs=configured.structuredOutputs;
    profile.effort=configured.effort;
    profile.sendDisabledThinking=configured.sendDisabledThinking;
    profile.legacyImplicitProfile=legacyImplicit;
    profile.client.emplace();
    profile.client->baseUrl=configured.baseUrl;
    profile.client->messagesPath=configured.messagesPath;
    profile.client->headers=configured.headers;
    profile.authHeader=configured.authHeader;
    profile.costs={{"inputTokens",0},{"outputTokens",0},
      {"promptCacheReadTokens",0},{"promptCacheWriteTokens",0},
      {"webSearchRequests",0}};
    model.profiles.emplace(name,std::move(profile));
  };
  if(config.profiles.empty())addProfile(config.model,selectedInferenceProfile(config));
  else for(const auto& [name,profile]:config.profiles)addProfile(name,profile);
  model.poetrySplit.dynamicBoundary=poetryEntries::kSystemPoetryDynamicBoundary;
  model.poetrySplit.attributionPrefix=
    "You are Batbox, an interactive coding assistant.";
  model.retryWire.retryAfter="retry-after";
  model.retryWire.shouldRetry="x-should-retry";
  model.nonInteractive=config.print;
  application_model::Imports imports;
  imports.authToken=[config]{return resolveProfileAuthToken(config,config.model);};
  imports.authTokenForModel=[config](const std::string& name){
    return resolveProfileAuthToken(config,name);};
  imports.invalidateAuthToken=[]{};
  imports.debug=[diagnostic](const std::string& scope,const std::string& value){
    if(diagnostic)diagnostic(scope+": "+value);
  };
  imports.diagnostic=imports.debug;
  imports.configureClientNetwork=client_factory::tls::makeIndigenousTlsClientConfigurator(
    [diagnostic](const std::string& message,const std::string& level){
      if(diagnostic)diagnostic(level+": "+message);
    });
  imports.buildParamsSegment=[](const Json& retry,
      requests::RequestCapture& capture,
      const requests::BuilderDependencies& builder,
      const requests::WireProtocol& wire,
      const requests::BuilderHeaders& headers){
    return requests::buildParamsFromContext(retry,capture,builder,wire,headers);
  };
  imports.normalizeAttachmentForAPI=makeAttachmentNormalizer(messages,std::move(roster));
  imports.normalizeAttachmentForGadgets=[messages](const Json& value,
      const gadgets::Gadgets& active){
    return normalizeAttachmentForGadgets(messages,active,value);
  };
  imports.normalizeGadgetInputForAPI=[](const gadgets::Gadget& gadget,const Json& input){
    return gadgets::normalizeGadgetInputForAPI(gadget,input,inputBindings());
  };
  auto stash=std::make_shared<gadgets::GadgetSchemaStash>();
  auto mutex=std::make_shared<std::mutex>();
  const auto schema=[stash,mutex](const gadgets::Gadget& gadget,
      const gadgets::GadgetSchemaOptions& options){
    std::lock_guard lock(*mutex);
    constexpr std::size_t maxEntries=128,maxBytes=4*1024*1024;
    const auto before=stash->entries.size();
    auto result=gadgets::gadgetToAPISchema(gadget,options,*stash,schemaBindings());
    if(stash->entries.size()!=before){
      std::size_t bytes=0;
      for(const auto& [key,entry]:stash->entries){
        bytes+=key.size()+entry.name.size()+entry.description.size()+
          entry.inputSchema.dump().size();
        if(bytes>maxBytes)break;
      }
      if(stash->entries.size()>maxEntries||bytes>maxBytes)stash->clear();
    }
    return result;
  };
  imports.gadgetSchemaSync=schema;
  imports.gadgetSchemaAsync=[&owner,schema](const gadgets::Gadget& gadget,
      const gadgets::GadgetSchemaOptions& options){
    return runInWorker<Json>(owner,[&gadget,options,schema]{return schema(gadget,options);});
  };
  return std::make_shared<application_model::LocalModelService>(
    owner,std::move(model),std::move(imports));
}
}
