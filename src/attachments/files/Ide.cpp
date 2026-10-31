#include "attachments/files/FileAttachments.hpp"
#include <stdexcept>

namespace batbox::attachments::files {
namespace {
event_loop::Deferred<Json> ready(event_loop::Loop& loop,Json value){
  event_loop::Deferred<Json> result(loop);result.resolve(std::move(value));
  return result;
}
std::string text(const Json& value){
  return value.is_string()?value.get<std::string>():std::string();
}
}
event_loop::Deferred<Json> getSelectedLinesFromIDE(
    const Json& selection,execution::Context& context,const Services& services){
  if(!services.connectedIdeName||!services.getAppState||
     !services.isFileReadDenied||!services.displayPath)
    throw std::invalid_argument("IDE selection service imports required");
  const auto ide=services.connectedIdeName(value::property(
    value::property(context.data,"options"),"mcpClients"));
  const auto& start=value::property(selection,"lineStart");
  const auto& content=value::property(selection,"text");
  const auto& path=value::property(selection,"filePath");
  if(!ide||ide->empty()||start.is_null()||!value::truthy(content)||!value::truthy(path))
    return ready(services.loop,Json::array());
  const auto filename=text(path);
  if(services.isFileReadDenied(filename,services.getAppState()))
    return ready(services.loop,Json::array());
  const double lineStart=value::numberOr(start);
  const double lineCount=value::numberOr(value::property(selection,"lineCount"));
  return ready(services.loop,Json::array({Json{
    {"type","selected_lines_in_ide"},{"ideName",*ide},
    {"lineStart",lineStart},{"lineEnd",lineStart+lineCount-1},
    {"filename",filename},{"content",content},
    {"displayPath",services.displayPath(filename)}}}));
}
event_loop::Deferred<Json> getOpenedFileFromIDE(
    const Json& selection,execution::Context& context,const Services& services){
  const auto& path=value::property(selection,"filePath");
  if(!value::truthy(path)||value::truthy(value::property(selection,"text")))
    return ready(services.loop,Json::array());
  if(!services.getAppState||!services.isFileReadDenied||!services.nestedForFile)
    throw std::invalid_argument("IDE opened-file service imports required");
  const auto filename=text(path);
  const auto state=services.getAppState();
  if(services.isFileReadDenied(filename,state))
    return ready(services.loop,Json::array());
  event_loop::Deferred<Json> output(services.loop);
  services.nestedForFile(filename,context,state).then(
    [output,filename](const Json* value,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      Json attachments=value?*value:Json::array();
      attachments.push_back(Json{{"type","opened_file_in_ide"},
        {"filename",filename}});
      output.resolve(std::move(attachments));
    });
  return output;
}
} // namespace batbox::attachments::files
