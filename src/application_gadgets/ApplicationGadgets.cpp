#include "application_gadgets/ApplicationGadgets.hpp"
#include "application_gadgets/IndigenousFiles.hpp"
#include "formatting/Format.hpp"
#include "gadgets/bash/BashGadget.hpp"
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <utility>

namespace batbox::application_gadgets {
namespace {
std::string localMonthYear(){
  static constexpr const char* months[]={"January","February","March","April",
    "May","June","July","August","September","October","November","December"};
  if(const auto* overrideDate=std::getenv("BATBOX_OVERRIDE_DATE")){
    const std::string date=overrideDate;
    if(date.size()>=7&&date[4]=='-'&&date[5]>='0'&&date[5]<='9'&&
       date[6]>='0'&&date[6]<='9'){
      const int month=(date[5]-'0')*10+(date[6]-'0');
      if(month>=1&&month<=12)return std::string(months[month-1])+" "+date.substr(0,4);
    }
  }
  const auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm local{};
#ifdef _WIN32
  if(localtime_s(&local,&now)!=0)throw std::runtime_error("Local date conversion failed");
#else
  if(!localtime_r(&now,&local))throw std::runtime_error("Local date conversion failed");
#endif
  return std::string(months[local.tm_mon])+" "+std::to_string(local.tm_year+1900);
}
template<class F>void need(const F& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("Application tool import required: ")+name);
}
void prepareBash(Bash& bash,const Inputs& inputs){
  auto& services=bash.services;
  if(!services.shell)throw std::invalid_argument("Application managed shell required");
  if(services.originalCwd.empty()){
    need(inputs.cwd,"working directory");services.originalCwd=inputs.cwd();
  }
  bash_gadget::bindIndigenousCommandSplitters(services);
  bash_gadget::bindIndigenousBashSchema(services,bash.maxTimeoutMs);
  need(services.logDebug,"shell diagnostic log");
  bash_gadget::bindIndigenousHintParser(services,
    bash_gadget::HintProtocol{bash.hintTagName,services.logDebug});
  if(!bash.cwdPolicy.getCwd)bash.cwdPolicy.getCwd=[shell=services.shell]{return shell->cwd();};
  if(!bash.cwdPolicy.getOriginalCwd)
    bash.cwdPolicy.getOriginalCwd=[original=services.originalCwd]{return original;};
  if(!bash.cwdPolicy.setCwd)
    bash.cwdPolicy.setCwd=[shell=services.shell](const std::string& cwd){shell->setCwd(cwd);};
  bash_gadget::bindIndigenousCwdPolicy(services,std::move(bash.cwdPolicy));
  // The shell owner binds sandboxing, mission roster, permission, image,
  // simulated-edit and visible UI services. None has an implicit allow path.
}
void prepareWeb(Web& web,const Files& files,
    const messages::Services& messages){
  if(!web.searxng)web.searxng=std::make_shared<Searxng>(web.searxngEndpoint);
  if(!web.scrapling){
    if(web.scraplingCommand.empty())throw std::invalid_argument(
      "Application Scrapling command or connection required");
    web.scrapling=std::make_shared<Scrapling>(web.scraplingCommand);
  }
  if(!web.search.localMonthYear)web.search.localMonthYear=localMonthYear;
  need(web.fetch.permissionRule,"WebFetch permission rules");
  need(web.fetch.skipPreflight,"WebFetch preflight setting");
  if(!web.fetch.skipPreflight())need(web.fetch.domainPreflight,
    "WebFetch domain preflight");
  if(!web.fetch.querySmallModel&&web.makeQueryServices&&web.getSmallFastModel)
    web.fetch.querySmallModel=gadgets::web::bindSmallModel(
      web.makeQueryServices,messages,web.getSmallFastModel);
  need(web.fetch.querySmallModel,"WebFetch small model");
  if(!web.fetch.nowMs)web.fetch.nowMs=files.nowMs;
  need(web.fetch.nowMs,"WebFetch clock");
  if(!web.fetch.formatFileSize)
    web.fetch.formatFileSize=[](std::size_t bytes){
      return formatting::formatFileSize(static_cast<double>(bytes));};
}
}

Prepared prepare(Inputs inputs){
  if(!inputs.messages)throw std::invalid_argument("Application message owner required");
  // The five file factories share a single bounded stash. Their per-call
  // active stash sidecars can still replace this owner for subagents.
  auto indigenous=makeIndigenousFileServices(inputs);
  prepareBash(inputs.bash,inputs);
  prepareWeb(inputs.web,inputs.files,*inputs.messages);
  gadgets::live_roster::FactoryInputs result;
  result.makeBatGadget=std::move(inputs.makeBatGadget);
  if(!result.makeBatGadget)result.bat=std::move(inputs.bat);
  result.bash=std::move(inputs.bash.services);
  if(inputs.bash.owner||inputs.bash.asyncMissionHost){
    if(!inputs.bash.owner||!inputs.bash.asyncMissionHost)
      throw std::invalid_argument("Application async Bash owner and task host required together");
    auto* owner=inputs.bash.owner;
    auto host=std::move(*inputs.bash.asyncMissionHost);
    result.makeBash=[owner,host=std::move(host)](bash_gadget::Services services)mutable{
      return bash_gadget::makeAsyncBashGadget(std::move(services),*owner,std::move(host));
    };
  }
  result.read=std::move(indigenous.read);
  result.write=std::move(indigenous.write);
  result.edit=std::move(indigenous.edit);
  result.glob=std::move(indigenous.glob);
  result.grep=std::move(indigenous.grep);
  result.fileBindings=std::move(inputs.files.presentation);
  result.makeFileBindings=std::move(inputs.files.makePresentation);
  result.search=std::move(inputs.web.search);
  result.fetch=std::move(inputs.web.fetch);
  result.searxng=std::move(inputs.web.searxng);
  result.scrapling=std::move(inputs.web.scrapling);
  result.mcpConnections=std::move(inputs.mcpConnections);
  result.mcpServices=std::move(inputs.mcpServices);
  result.additionalBaseGadgets=std::move(inputs.additionalGadgets);
  return {std::move(result),std::move(inputs.messages)};
}

Live makeLive(Inputs inputs){
  auto ready=prepare(std::move(inputs));
  auto snapshot=gadgets::live_roster::makeSnapshot(std::move(ready.factory));
  // Read can construct a meta message for images and PDFs.
  // Retain that service with the Gadget itself through retired generations.
  for(auto& gadget:snapshot->owned)if(gadget.name=="Read"){
    auto call=std::move(gadget.call);
    gadget.call=[call=std::move(call),messages=ready.messages](
      const Json& input,const gadgets::GadgetCallContext& context,
      const std::function<std::future<Json>(const Json&)>& canUseGadget,
      const Json& parent,const std::function<void(const gadgets::GadgetProgress&)>& progress){
      (void)messages;
      return call(input,context,canUseGadget,parent,progress);
    };
  }
  return {std::move(snapshot),std::move(ready.messages)};
}
} // namespace batbox::application_gadgets
