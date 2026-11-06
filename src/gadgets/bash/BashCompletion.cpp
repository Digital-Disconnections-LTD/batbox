#include "gadgets/bash/BashCompletion.hpp"
#include "gadgets/bash/Accumulator.hpp"
#include <filesystem>

namespace batbox::bash_gadget {
namespace {
std::string stringField(const Json& value,const char* key){
  return value.is_object()&&value.contains(key)&&value[key].is_string()?value[key].get<std::string>():"";
}
bool boolField(const Json& value,const char* key){
  return value.is_object()&&value.contains(key)&&value[key].is_boolean()&&value[key].get<bool>();
}
std::string endTrim(const std::string& value){
  const auto units=value::utf16CodeUnits(value);std::size_t end=units.size();
  while(end>0&&value::trim(value::fromUtf16(std::u16string_view(&units[end-1],1))).empty())--end;
  return value::fromUtf16(std::u16string_view(units.data(),end));
}
}
BashPreparedResult prepareBashCompletion(const Json& input,
  const gadgets::GadgetCallContext& context,shell::ExecResult result,
  const Services& services,const std::function<Json()>& getGadgetPermissionContext){
  BashPreparedResult prepared;prepared.command=std::move(result);
  prepared.mainThread=stringField(context.data,"agentId").empty();
  const auto command=stringField(input,"command");
  EndTruncatingAccumulator stdoutAccumulator;
  try{
    const bool isInterrupt=prepared.command.interrupted&&
      (context.currentAbortReason?context.currentAbortReason():
        stringField(context.data,"abortReason"))=="interrupt";
    stdoutAccumulator.append(endTrim(prepared.command.stdoutText)+"\n");
    prepared.interpretation=interpretCommandResult(command,prepared.command.code,
      prepared.command.stdoutText,"",services.splitCommand);
    if(boolField(prepared.interpretation,"isError")&&!isInterrupt&&prepared.command.code!=0)
      stdoutAccumulator.append("Exit code "+std::to_string(prepared.command.code));
    if(prepared.mainThread){
      const auto permissionContext=getGadgetPermissionContext?getGadgetPermissionContext():
        context.data.value("toolPermissionContext",Json());
      if(services.resetCwdIfOutsideProject(permissionContext))
        prepared.shellReset=stdErrAppendShellResetMessage("",services.originalCwd);
    }
    if(prepared.command.preSpawnError)throw std::runtime_error(*prepared.command.preSpawnError);
    if(boolField(prepared.interpretation,"isError")&&!isInterrupt)
      prepared.failureOutput=prepared.command.stdoutText;
  }catch(...){if(services.clearGadgetJSX)services.clearGadgetJSX(context.data);throw;}
  if(services.clearGadgetJSX)services.clearGadgetJSX(context.data);
  if(!prepared.failureOutput)prepared.stdoutText=stdoutAccumulator.toString();
  return prepared;
}
void throwIfBashFailed(const BashPreparedResult& prepared,
  const BashPersistedOutput& persisted){
  if(!prepared.failureOutput)return;
  const auto& command=prepared.command;
  std::string output;
  if(command.stdoutTail||!persisted.path.empty()){
    const double size=persisted.originalSize?static_cast<double>(*persisted.originalSize):
      static_cast<double>(command.outputFileSize.value_or(0));
    output=persistedOutputMessage(persisted.path,size,*prepared.failureOutput,
      command.stdoutTail.value_or(std::string()));
  }else output=*prepared.failureOutput;
  // Batbox's own note on how the command ended (a timeout, a size kill)
  // follows the output, as a terminal would print it.
  if(!command.stderrText.empty()){
    if(!output.empty()&&output.back()!='\n')output+='\n';
    output+=command.stderrText;
  }
  throw ShellError("",std::move(output),command.code,command.interrupted);
}
BashPersistedOutput persistBashOutput(const shell::ExecResult& result,
  const Services& services){
  if(!result.outputFilePath||result.outputFilePath->empty()||
    !result.outputMissionId||result.outputMissionId->empty())return {};
  BashPersistedOutput persisted;
  try{
    const auto size=std::filesystem::file_size(*result.outputFilePath);
    persisted.originalSize=size;
    const auto dest=services.getGadgetResultPath(*result.outputMissionId);
    std::filesystem::create_directories(std::filesystem::path(dest).parent_path());
    constexpr std::uintmax_t maxPersisted=64ull*1024*1024;
    if(size>maxPersisted)std::filesystem::resize_file(*result.outputFilePath,maxPersisted);
    try{std::filesystem::create_hard_link(*result.outputFilePath,dest);}
    catch(...){std::filesystem::copy_file(*result.outputFilePath,dest,
      std::filesystem::copy_options::overwrite_existing);}
    persisted.path=dest;
  }catch(...){}
  return persisted;
}
BashFinalInput prepareBashFinalInput(const Json& input,
  const BashPreparedResult& prepared,const Services& services){
  const auto command=stringField(input,"command");
  const auto stripped=stripEmptyLines(prepared.stdoutText);
  const auto extracted=services.extractHints(stripped,command);
  if(!extracted.is_object()||!extracted.contains("stripped")||!extracted["stripped"].is_string()||
     !extracted.contains("hints")||!extracted["hints"].is_array())
    throw std::runtime_error("extractHints returned invalid data");
  const auto clean=extracted["stripped"].get<std::string>();
  if(prepared.mainThread)for(const auto& hint:extracted["hints"])services.recordHint(hint);
  return BashFinalInput{clean,isImageOutput(clean)};
}
gadgets::GadgetResult finishBashFinalOutput(const Json& input,
  const BashPreparedResult& prepared,const BashPersistedOutput& persisted,
  const BashFinalInput& finalInput,const std::optional<std::string>& resized,
  const Services& services){
  const auto command=stringField(input,"command");
  const bool image=finalInput.isImage&&resized&&!resized->empty();
  const std::string& compressed=image?*resized:finalInput.cleanOutput;
  std::string stderrText=prepared.command.stderrText;
  if(!prepared.shellReset.empty()){
    if(!stderrText.empty())stderrText+='\n';
    stderrText+=prepared.shellReset;
  }
  Json data={{"stdout",compressed},{"stderr",std::move(stderrText)},
    {"interrupted",prepared.command.interrupted},{"isImage",image},
    {"noOutputExpected",isSilentBashCommand(command,services.splitWithOperators)}};
  if(prepared.interpretation.contains("message"))
    data["returnCodeInterpretation"]=prepared.interpretation["message"];
  if(prepared.command.backgroundMissionId)data["backgroundTaskId"]=*prepared.command.backgroundMissionId;
  if(prepared.command.backgroundedByUser)data["backgroundedByUser"]=true;
  if(prepared.command.assistantAutoBackgrounded||
    prepared.command.assistantAutoBackgroundedOwnProperty)
    data["assistantAutoBackgrounded"]=prepared.command.assistantAutoBackgrounded;
  if(!persisted.path.empty())data["persistedOutputPath"]=persisted.path;
  if(persisted.originalSize)data["persistedOutputSize"]=*persisted.originalSize;
  else if(prepared.command.stdoutTail&&prepared.command.outputFileSize)
    data["persistedOutputSize"]=*prepared.command.outputFileSize;
  if(!image&&prepared.command.stdoutTail){
    const auto tail=services.extractHints(*prepared.command.stdoutTail,command);
    data["persistedOutputTail"]=tail.is_object()&&tail.contains("stripped")&&
      tail["stripped"].is_string()?tail["stripped"]:Json(*prepared.command.stdoutTail);
  }
  gadgets::GadgetResult output;output.data=std::move(data);return output;
}
gadgets::GadgetResult finishBashCompletion(const Json& input,
  const BashPreparedResult& prepared,const BashPersistedOutput& persisted,
  const Services& services){
  const auto finalInput=prepareBashFinalInput(input,prepared,services);
  std::optional<std::string> resized;
  if(finalInput.isImage)resized=services.resizeImage(finalInput.cleanOutput,prepared.command);
  return finishBashFinalOutput(input,prepared,persisted,finalInput,resized,services);
}
} // namespace batbox::bash_gadget
