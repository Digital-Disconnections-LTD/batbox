#include "compaction/Restore.hpp"
#include <filesystem>
#include <stdexcept>
using namespace batbox;
using namespace batbox::compaction;
Json runCompactionRestoreFixture(const Json& row,Json& trace) {
  RestoreServices services;
  services.expandPath=[](const std::string& path){return std::filesystem::path(path.starts_with('/')?path:"/workspace/"+path).lexically_normal().string();};
  services.memoryTypes={"User","Project","Local","Managed","AutoMem"};
  services.getMemoryPath=[&](const std::string& type){if(row.value("memoryThrows",false))throw std::runtime_error("no memory");return "/memory/"+type;};
  services.getPlanFilePath=[&](const Json&){if(row.value("planThrows",false))throw std::runtime_error("no plan");return "/workspace/plan.md";};
  services.getPlan=[&](const Json&){return row.value("plan",Json());};
  services.createAttachmentMessage=[](const Json& att){return Json{{"type","attachment"},{"attachment",att}};};
  services.getInvokedSkillsForBat=[&](const Json&){return row.value("skills",Json::array());};
  services.getMissionOutputPath=[](const std::string& mission){return "/tmp/"+mission+".output";};
  services.generateFileAttachment=[&](const std::string& name,Context&,double maximum,const std::string& source){
    trace.push_back(Json::array({"generate",name,maximum,source}));
    auto results=row.value("fileResults",Json::object());
    Json value=results.contains(name)?results[name]:Json{{"type","file"},{"filename",name},{"content","read"}};
    std::promise<Json> promise;promise.set_value(std::move(value));return promise.get_future();
  };
  Context context;context.data=Json::object();if(row.contains("agent"))context.data["agentId"]=row["agent"];
  context.getAppState=[&]{return Json{{"toolPermissionContext",{{"mode",row.value("mode","default")}}},
    {"tasks",row.value("tasks",Json::object())}};};
  const auto op=row.at("op").get<std::string>();
  const auto bat=value::property(context.data,"agentId");
  if(op=="restoreTruncate")return truncateToTokens(row.at("text"),row.at("maximum"));
  if(op=="restoreReadPaths")return collectReadGadgetFilePaths(row.at("messages"),services);
  if(op=="restoreExclude")return shouldExcludeFromPostCompactRestore(row.at("filename"),bat,services);
  if(op=="restorePlan")return createPlanAttachmentIfNeeded(bat,services);
  if(op=="restoreSkills")return createSkillAttachmentIfNeeded(bat,services);
  if(op=="restorePlanMode")return createPlanModeAttachmentIfNeeded(context,services);
  if(op=="restoreAgents")return createAsyncBatAttachmentsIfNeeded(context,services);
  if(op=="restoreFiles")return createPostCompactFileAttachments(row.at("reads"),context,row.value("maximum",5.0),row.value("preserved",Json::array()),services);
  throw std::runtime_error("Unknown restore fixture operation");
}
