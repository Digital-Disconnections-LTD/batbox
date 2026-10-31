#include "attachments/local/LocalCollectors.hpp"
#include <stdexcept>

namespace batbox::attachments::local {
namespace {
bool bashAvailable(const execution::Context& context,const Services& services){
  return gadgets::findGadgetByName(context.gadgets,services.bashGadgetName)!=nullptr;
}
event_loop::Deferred<Json> ready(event_loop::Loop& loop,Json value){
  event_loop::Deferred<Json> output(loop);output.resolve(std::move(value));
  return output;
}
}
event_loop::Deferred<Json> getDiagnosticAttachments(
    const execution::Context& context,const Services& services){
  if(!bashAvailable(context,services))return ready(services.loop,Json::array());
  event_loop::Deferred<Json> output(services.loop);
  try{
    if(!services.getNewDiagnostics)
      throw std::invalid_argument("IDE diagnostic tracker import required");
    services.getNewDiagnostics().then(
      [output](const Json* diagnostics,const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        if(!diagnostics||!diagnostics->is_array()){
          output.reject(std::make_exception_ptr(std::invalid_argument(
            "IDE diagnostic batch must be an array")));return;
        }
        if(diagnostics->empty()){output.resolve(Json::array());return;}
        output.resolve(Json::array({Json{{"type","diagnostics"},
          {"files",*diagnostics},{"isNew",true}}}));
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
Json getLspDiagnosticAttachments(const execution::Context& context,
    const Services& services){
  if(!bashAvailable(context,services))return Json::array();
  if(!services.logDebug)
    throw std::invalid_argument("LSP diagnostic debug sink required");
  services.logDebug("LSP Diagnostics: getLSPDiagnosticAttachments called");
  try{
    if(!services.checkLspDiagnostics)
      throw std::invalid_argument("LSP diagnostic registry import required");
    const auto pending=services.checkLspDiagnostics();
    if(!pending.is_array())
      throw std::invalid_argument("LSP diagnostic sets must be an array");
    if(pending.empty())return Json::array();
    services.logDebug("LSP Diagnostics: Found "+std::to_string(pending.size())+
      " pending diagnostic set(s)");
    Json attachments=Json::array();
    for(const auto& set:pending)
      attachments.push_back(Json{{"type","diagnostics"},
        {"files",value::property(set,"files")},{"isNew",true}});
    if(!services.clearLspDiagnostics)
      throw std::invalid_argument("LSP diagnostic clear import required");
    services.clearLspDiagnostics();
    services.logDebug("LSP Diagnostics: Cleared "+std::to_string(pending.size())+
      " delivered diagnostic(s) from registry");
    services.logDebug("LSP Diagnostics: Returning "+
      std::to_string(attachments.size())+" diagnostic attachment(s)");
    return attachments;
  }catch(...){
    if(!services.logError)throw;
    try{std::rethrow_exception(std::current_exception());}
    catch(const std::exception& error){
      services.logError(std::make_exception_ptr(std::runtime_error(
        std::string("Failed to get LSP diagnostic attachments: ")+error.what())));
    }catch(...){
      services.logError(std::make_exception_ptr(std::runtime_error(
        "Failed to get LSP diagnostic attachments: Unknown error")));
    }
    return Json::array();
  }
}
event_loop::Deferred<Json> getUnifiedMissionAttachments(const Services& services){
  event_loop::Deferred<Json> output(services.loop);
  try{
    if(!services.getAppState)
      throw std::invalid_argument("Live AppState import required");
    const auto app=services.getAppState();
    if(!services.generateMissionAttachments)
      throw std::invalid_argument("Task attachment generator import required");
    services.generateMissionAttachments(app).then(
      [output,services](const MissionBatch* batch,
          const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        try{
          if(!batch||!batch->attachments.is_array())
            throw std::invalid_argument("Task attachment batch required");
          if(!services.applyMissionOffsetsAndEvictions)
            throw std::invalid_argument("Task offset update import required");
          services.applyMissionOffsetsAndEvictions(batch->updatedOffsets,
            batch->evictedIds);
          Json attachments=Json::array();
          for(const auto& mission:batch->attachments){
            const auto& id=value::property(mission,"taskId");
            if(!services.missionOutputPath)
              throw std::invalid_argument("Task output path import required");
            Json item={{"type","task_status"},{"taskId",id},
              {"taskType",value::property(mission,"taskType")},
              {"status",value::property(mission,"status")},
              {"description",value::property(mission,"description")},
              {"deltaSummary",value::property(mission,"deltaSummary")},
              {"outputFilePath",services.missionOutputPath(id.get<std::string>())}};
            attachments.push_back(std::move(item));
          }
          output.resolve(std::move(attachments));
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::attachments::local
