#include "gadgets/bash/BashGadget.hpp"
#include "formatting/Format.hpp"
#include <algorithm>

namespace batbox::bash_gadget {
namespace {
std::string field(const Json& value,const char* key){
  return value.is_object()&&value.contains(key)&&value[key].is_string()?value[key].get<std::string>():"";
}
bool flag(const Json& value,const char* key){
  return value.is_object()&&value.contains(key)&&value[key].is_boolean()&&value[key].get<bool>();
}
std::string trimEnd(const std::string& value){
  const auto units=value::utf16CodeUnits(value);std::size_t end=units.size();
  while(end>0&&value::trim(value::fromUtf16(std::u16string_view(&units[end-1],1))).empty())--end;
  return value::fromUtf16(std::u16string_view(units.data(),end));
}
std::string processStdout(const std::string& value){
  std::size_t at=0;
  while(at<value.size()){
    const auto newline=value.find('\n',at);if(newline==std::string::npos)break;
    if(!value::trim(std::string_view(value).substr(at,newline-at)).empty())break;
    at=newline+1;
  }
  return trimEnd(value.substr(at));
}
std::pair<std::string,bool> preview(const std::string& value,std::size_t maxBytes){
  const auto units=value::utf16CodeUnits(value);
  if(units.size()<=maxBytes)return {value,false};
  const auto newline=units.substr(0,maxBytes).rfind(u'\n');
  const auto cut=newline!=std::u16string::npos&&newline>maxBytes*0.5?newline:maxBytes;
  return {value::fromUtf16(std::u16string_view(units.data(),cut)),true};
}
std::string largeMessage(const std::string& path,double originalSize,const std::string& text){
  const auto [sample,more]=preview(text,2000);
  return "<persisted-output>\nOutput too large ("+formatting::formatFileSize(originalSize)+
    "). Full output saved to: "+path+"\n\nPreview (first "+
    formatting::formatFileSize(2000)+"):\n"+sample+(more?"\n...\n":"\n")+
    "</persisted-output>";
}
}
Json mapGadgetResult(const Json& output,const std::string& gadgetUseID,
  const std::function<std::string(const std::string&)>& getMissionOutputPath,
  double assistantBlockingBudgetMs){
  if(output.is_object()&&output.contains("structuredContent")&&output["structuredContent"].is_array()&&
     !output["structuredContent"].empty())
    return {{"tool_use_id",gadgetUseID},{"type","tool_result"},{"content",output["structuredContent"]}};
  const auto stdoutText=field(output,"stdout");
  if(flag(output,"isImage")){
    const auto image=buildImageGadgetResult(stdoutText,gadgetUseID);if(!image.is_null())return image;
  }
  std::string processed=stdoutText.empty()?stdoutText:processStdout(stdoutText);
  const auto persistedPath=field(output,"persistedOutputPath");
  if(!persistedPath.empty()){
    const double size=output.contains("persistedOutputSize")&&output["persistedOutputSize"].is_number()?
      output["persistedOutputSize"].get<double>():0;
    processed=largeMessage(persistedPath,size,processed);
  }
  const auto stderrText=field(output,"stderr");std::string error=value::trim(stderrText);
  const bool interrupted=flag(output,"interrupted");
  if(interrupted){if(!stderrText.empty())error+='\n';
    error+="<error>Command was aborted before completion</error>";}
  std::string background;
  const auto missionId=field(output,"backgroundTaskId");
  if(!missionId.empty()){
    if(!getMissionOutputPath)throw std::logic_error("getTaskOutputPath dependency not supplied");
    const auto path=getMissionOutputPath(missionId);
    if(flag(output,"assistantAutoBackgrounded")){
      background="Command exceeded the assistant-mode blocking budget ("+
        std::to_string(static_cast<long long>(assistantBlockingBudgetMs/1000))+"s) and was moved to the background with ID: "+
        missionId+". It is still running — you will be notified when it completes. Output is being written to: "+path+
        ". In assistant mode, delegate long-running work to a subagent or use run_in_background to keep this conversation responsive.";
    }else if(flag(output,"backgroundedByUser"))
      background="Command was manually backgrounded by user with ID: "+missionId+". Output is being written to: "+path;
    else background="Command running in background with ID: "+missionId+". Output is being written to: "+path;
  }
  std::string content;
  for(const auto& part:{processed,error,background})if(!part.empty()){
    if(!content.empty())content+='\n';
    content+=part;
  }
  return {{"tool_use_id",gadgetUseID},{"type","tool_result"},{"content",content},{"is_error",interrupted}};
}
} // namespace batbox::bash_gadget
