#include "bats/BatMissionBinding.hpp"

namespace batbox::bats {
void bindLocalBatMissionLifecycle(AsyncLifecycleImports& lifecycle,
  BatMissionStateServices& state,BatMissionNotificationServices& notification,
  BatMissionProgressImports& progress){
  lifecycle.createProgressTracker=[](){return createProgressTracker();};
  lifecycle.createActivityDescriptionResolver=[](const gadgets::Gadgets& gadgets){
    return createActivityDescriptionResolver(gadgets);};
  lifecycle.isLocalBatMission=[](const Json& mission){return isLocalBatMission(mission);};
  lifecycle.updateProgressFromMessage=[&progress](Json& tracker,const Json& message,
    const ActivityResolver& resolver,const gadgets::Gadgets& gadgets){
    updateProgressFromMessage(tracker,message,resolver,gadgets,progress);};
  lifecycle.getProgressUpdate=[](const Json& tracker){return getProgressUpdate(tracker);};
  lifecycle.getTokenCountFromTracker=[](const Json& tracker){
    return getTokenCountFromTracker(tracker);};
  lifecycle.updateAsyncBatProgress=[&state](const std::string& id,
    const Json& progress,const RootSetAppState& rootSet){
    BatMissionStateServices adapter=state;adapter.setAppState=rootSet;
    updateBatProgress(id,progress,adapter);};
  lifecycle.emitMissionProgressEvent=state.emitMissionProgress;
  lifecycle.completeAsyncBat=[&state](const Json& result,
    const BatMessageRefs& refs,const RootSetAppState& rootSet){
    BatMissionStateServices adapter=state;adapter.setAppState=rootSet;
    completeBatMission(result,adapter,&refs);};
  lifecycle.killAsyncBat=[&state](const std::string& id,
    const BatMessageRefs& refs,const RootSetAppState& rootSet){
    BatMissionStateServices adapter=state;adapter.setAppState=rootSet;
    killAsyncBat(id,adapter,&refs);};
  lifecycle.failAsyncBat=[&state](const std::string& id,const std::string& error,
    const BatMessageRefs& refs,const RootSetAppState& rootSet){
    BatMissionStateServices adapter=state;adapter.setAppState=rootSet;
    failBatMission(id,error,adapter,&refs);};
  lifecycle.enqueueBatNotification=[&notification](const Json& args,
    const RootSetAppState& rootSet){
    BatMissionNotificationServices adapter=notification;adapter.setAppState=rootSet;
    enqueueBatNotification(args,adapter);};
}
} // namespace batbox::bats
