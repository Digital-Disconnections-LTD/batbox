#include "compaction/Restore.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
using namespace batbox;
using namespace batbox::compaction;
int main() {
  std::promise<Json> slow,fast;
  fast.set_value(Json{{"filename","fast"}});
  std::promise<void> convertedFast;
  auto converted=convertedFast.get_future();
  std::atomic<int> sequence=0;
  RestoreServices services;
  services.expandPath=[](const std::string& value){return value;};
  services.getPlanFilePath=[](const Json&){return "plan";};
  services.generateFileAttachment=[&](const std::string& path,Context&,double,const std::string&){
    return path=="slow" ? slow.get_future() : fast.get_future();
  };
  services.createAttachmentMessage=[&](const Json& value){
    Json result=value;result["sequence"]=++sequence;
    if(value.at("filename")=="fast")convertedFast.set_value();
    return result;
  };
  Context context;context.data=Json::object();
  const Json state={{"slow",{{"timestamp",2},{"content",""}}},{"fast",{{"timestamp",1},{"content",""}}}};
  auto restored=std::async(std::launch::async,[&]{return createPostCompactFileAttachments(state,context,2,Json::array(),services);});
  const bool independent=converted.wait_for(std::chrono::seconds(5))==std::future_status::ready;
  slow.set_value(Json{{"filename","slow"}});
  const auto result=restored.get();
  if(!independent || result.size()!=2 || result[0].at("sequence")!=2 || result[1].at("sequence")!=1) {
    std::cerr<<"Attachment continuations did not preserve completion order and result input order\n";
    return 1;
  }
  std::cout<<"Concurrent file restore completes independent continuations and preserves Promise.all result order\n";
}
