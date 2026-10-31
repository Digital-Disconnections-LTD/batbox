#include "gadgets/files/FileReadInternal.hpp"
#include <stdexcept>

namespace batbox::gadgets::files {
Json mapReadResult(const Json& data,const std::string& gadgetUseID,
                   const FileReadServices& services,std::optional<double> memoryMtimeMs){
  const std::string type=data.at("type").get<std::string>();
  const Json& file=data.at("file");
  if(type=="image")return Json{{"tool_use_id",gadgetUseID},{"type","tool_result"},
    {"content",Json::array({Json{{"type","image"},{"source",{{"type","base64"},
      {"data",file.at("base64")},{"media_type",file.at("type")}}}}})}};
  if(type=="notebook"){
    requireReadBinding(static_cast<bool>(services.mapNotebookCellsToGadgetResult),"mapNotebookCellsToToolResult");
    return services.mapNotebookCellsToGadgetResult(file.at("cells"),gadgetUseID);
  }
  if(type=="pdf"||type=="parts"){
    requireReadBinding(static_cast<bool>(services.formatFileSize),"formatFileSize");
    const std::string path=file.at("filePath").get<std::string>();
    const auto size=services.formatFileSize(file.at("originalSize").get<double>());
    const std::string content=type=="pdf"
      ? "PDF file read: "+path+" ("+size+")"
      : "PDF pages extracted: "+std::to_string(file.at("count").get<int>())+
        " page(s) from "+path+" ("+size+")";
    return Json{{"tool_use_id",gadgetUseID},{"type","tool_result"},{"content",content}};
  }
  if(type=="file_unchanged")return Json{{"tool_use_id",gadgetUseID},{"type","tool_result"},
    {"content",services.fileUnchangedStub}};
  if(type!="text")throw std::invalid_argument("Unknown FileRead result type: "+type);
  const auto content=file.at("content").get<std::string>();
  std::string output;
  if(!content.empty()){
    if(memoryMtimeMs){
      requireReadBinding(static_cast<bool>(services.memoryFreshnessNote),"memoryFreshnessNote");
      output+=services.memoryFreshnessNote(*memoryMtimeMs);
    }
    requireReadBinding(static_cast<bool>(services.compactLinePrefixEnabled),"compactLinePrefixEnabled");
    output+=addLineNumbers(content,file.at("startLine").get<std::size_t>(),services.compactLinePrefixEnabled());
    requireReadBinding(static_cast<bool>(services.shouldIncludeFileReadMitigation),"shouldIncludeFileReadMitigation");
    if(services.shouldIncludeFileReadMitigation())output+=services.mitigationReminder;
  }else if(file.at("totalLines").get<std::size_t>()==0){
    output="<system-reminder>Warning: the file exists but the contents are empty.</system-reminder>";
  }else{
    output="<system-reminder>Warning: the file exists but is shorter than the provided offset ("+
      std::to_string(file.at("startLine").get<std::size_t>())+"). The file has "+
      std::to_string(file.at("totalLines").get<std::size_t>())+" lines.</system-reminder>";
  }
  return Json{{"tool_use_id",gadgetUseID},{"type","tool_result"},{"content",output}};
}
} // namespace batbox::gadgets::files
