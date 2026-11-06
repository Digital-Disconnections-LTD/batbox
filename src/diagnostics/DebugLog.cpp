#include "diagnostics/DebugLog.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::diagnostics {
DebugLog::DebugLog(DebugLogOptions options,std::filesystem::path defaultFile,
    std::function<std::string()> timestamp,
    std::function<void(const std::string&)> writeToStderr):
    options_(std::move(options)),filter_(parseDebugFilter(options_.filter)),
    path_(options_.file?*options_.file:std::move(defaultFile)),
    timestamp_(std::move(timestamp)),writeToStderr_(std::move(writeToStderr)){
  if(!options_.enabled)throw std::invalid_argument("Debug log options are not enabled");
  if(!timestamp_||!writeToStderr_)throw std::invalid_argument("Debug log imports are required");
}
void DebugLog::write(const std::string& message){
  if(!shouldShowDebugMessage(message,filter_))return;
  const auto line=timestamp_()+" [DEBUG] "+value::trim(message)+"\n";
  std::lock_guard lock(mutex_);
  if(options_.toStderr){writeToStderr_(line);return;}
  if(!opened_){
    opened_=true;
    std::error_code error;
    if(path_.has_parent_path())std::filesystem::create_directories(path_.parent_path(),error);
    file_.open(path_,std::ios::binary|std::ios::app);
    // The default file gets a "latest" link beside it; a link that cannot be
    // made (Windows without the privilege) is skipped.
    if(file_&&!options_.file){
      const auto latest=path_.parent_path()/"latest";
      std::filesystem::remove(latest,error);
      std::filesystem::create_symlink(path_.filename(),latest,error);
    }
  }
  if(!file_)return;
  file_<<line;
  file_.flush();
}
}
