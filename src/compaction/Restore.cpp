#include "runtime/AsyncLaunch.hpp"
#include "compaction/Restore.hpp"
#include "services/tokenEstimation.hpp"
#include <algorithm>
#include <cmath>

namespace batbox::compaction {
std::string truncateToTokens(const std::string& text,double maximum) {
  if(roughTokenCountEstimation(text)<=maximum)return text;
  return value::slice(text,0,maximum*4-value::stringLength(SKILL_TRUNCATION_MARKER))+SKILL_TRUNCATION_MARKER;
}
std::set<std::string> collectReadGadgetFilePaths(const Json& messages,RestoreServices& services) {
  std::set<std::string> stubs,paths;
  for(const auto& message:messages) {
    const auto& content=value::property(value::property(message,"message"),"content");
    if(message.at("type")!="user" || !content.is_array())continue;
    for(const auto& block:content) {
      const auto& text=value::property(block,"content");
      if(block.at("type")=="tool_result" && text.is_string() && text.get<std::string>().starts_with(FILE_UNCHANGED_STUB))
        stubs.insert(block.at("tool_use_id").get<std::string>());
    }
  }
  for(const auto& message:messages) {
    const auto& content=value::property(value::property(message,"message"),"content");
    if(message.at("type")!="assistant" || !content.is_array())continue;
    for(const auto& block:content) {
      if(block.at("type")!="tool_use" || value::property(block,"name")!="Read" || stubs.contains(block.at("id").get<std::string>()))continue;
      const auto& input=value::property(block,"input");
      const auto& path=value::property(input,"file_path");
      if(value::truthy(input) && input.is_object() && path.is_string())paths.insert(services.expandPath(path.get<std::string>()));
    }
  }
  return paths;
}
bool shouldExcludeFromPostCompactRestore(const std::string& file,const Json& bat,RestoreServices& services) {
  const auto normalized=services.expandPath(file);
  try {if(normalized==services.expandPath(services.getPlanFilePath(bat)))return true;}catch(...){}
  try {
    std::set<std::string> paths;
    for(const auto& type:services.memoryTypes)paths.insert(services.expandPath(services.getMemoryPath(type)));
    if(paths.contains(normalized))return true;
  }catch(...){}
  return false;
}
Json createPlanAttachmentIfNeeded(const Json& bat,RestoreServices& services) {
  const auto content=services.getPlan(bat);
  if(!value::truthy(content))return nullptr;
  return services.createAttachmentMessage({{"type","plan_file_reference"},
    {"planFilePath",services.getPlanFilePath(bat)},{"planContent",content}});
}
Json createSkillAttachmentIfNeeded(const Json& bat,RestoreServices& services) {
  auto invoked=services.getInvokedSkillsForBat(bat);
  if(invoked.empty())return nullptr;
  std::vector<Json> recent(invoked.begin(),invoked.end());
  std::stable_sort(recent.begin(),recent.end(),[](const auto& a,const auto& b){return a.at("invokedAt").template get<double>()>b.at("invokedAt").template get<double>();});
  double used=0;
  Json skills=Json::array();
  for(const auto& skill:recent) {
    const auto content=truncateToTokens(skill.at("content").get<std::string>(),5000);
    const auto tokens=roughTokenCountEstimation(content);
    if(used+tokens>25000)continue;
    used+=tokens;
    skills.push_back({{"name",skill.at("skillName")},{"path",skill.at("skillPath")},{"content",content}});
  }
  if(skills.empty())return nullptr;
  return services.createAttachmentMessage({{"type","invoked_skills"},{"skills",skills}});
}
Json createPlanModeAttachmentIfNeeded(Context& context,RestoreServices& services) {
  if(context.getAppState().at("toolPermissionContext").at("mode")!="plan")return nullptr;
  const auto& bat=value::property(context.data,"agentId");
  const auto path=services.getPlanFilePath(bat);
  const bool exists=!services.getPlan(bat).is_null();
  return services.createAttachmentMessage({{"type","plan_mode"},{"reminderType","full"},
    {"isSubAgent",value::truthy(bat)},{"planFilePath",path},{"planExists",exists}});
}
Json createAsyncBatAttachmentsIfNeeded(Context& context,RestoreServices& services) {
  const auto app=context.getAppState();
  Json result=Json::array();
  for(const auto& mission:app.at("tasks")) {
    if(mission.at("type")!="local_agent")continue;
    if(value::truthy(value::property(mission,"retrieved")) || mission.at("status")=="pending" ||
       value::property(mission,"agentId")==value::property(context.data,"agentId"))continue;
    const auto& status=mission.at("status");
    const auto& summary=status=="running" ? value::property(value::property(mission,"progress"),"summary") : value::property(mission,"error");
    Json attachment={{"type","task_status"},{"taskType","local_agent"},{"description",mission.at("description")},
      {"status",status},{"deltaSummary",summary},{"outputFilePath",services.getMissionOutputPath(mission.at("agentId").get<std::string>())}};
    if(mission.contains("agentId"))attachment["taskId"]=mission["agentId"];
    result.push_back(services.createAttachmentMessage(attachment));
  }
  return result;
}
Json createPostCompactFileAttachments(const Json& state,Context& context,double maxFiles,
                                     const Json& preserved,RestoreServices& services) {
  const auto readPaths=collectReadGadgetFilePaths(preserved,services);
  std::vector<Json> recent;
  for(auto it=state.begin();it!=state.end();++it) {
    Json file={{"filename",it.key()}};
    for(auto field=it->begin();field!=it->end();++field)file[field.key()]=field.value();
    const auto name=file.at("filename").get<std::string>();
    if(!shouldExcludeFromPostCompactRestore(name,value::property(context.data,"agentId"),services) &&
       !readPaths.contains(services.expandPath(name)))recent.push_back(std::move(file));
  }
  std::stable_sort(recent.begin(),recent.end(),[](const auto& a,const auto& b){return a.at("timestamp").template get<double>()>b.at("timestamp").template get<double>();});
  const double end=std::isnan(maxFiles) ? 0 : std::trunc(maxFiles);
  const auto count=static_cast<std::size_t>(end<0 ? std::max(0.0,recent.size()+end) : std::min<double>(recent.size(),end));
  std::vector<std::future<Json>> pending;
  for(std::size_t i=0;i<count;++i) {
    auto generated=services.generateFileAttachment(recent[i].at("filename"),context,5000,"compact");
    pending.push_back(::batbox::runtime::contextAsync(std::launch::async,[future=std::move(generated),create=services.createAttachmentMessage]()mutable {
      auto attachment=future.get();
      return attachment.is_null() ? Json() : create(attachment);
    }));
  }
  Json result=Json::array();double used=0;
  for(auto& future:pending) {
    auto message=future.get();
    if(message.is_null())continue;
    const auto tokens=roughTokenCountEstimation(value::stringify(message));
    if(used+tokens<=50000){used+=tokens;result.push_back(std::move(message));}
  }
  return result;
}
}
