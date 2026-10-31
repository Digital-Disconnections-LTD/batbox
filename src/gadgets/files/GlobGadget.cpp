#include "gadgets/files/Glob.hpp"
#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace batbox::gadgets::files {
namespace fs=std::filesystem;
ValidationResult validateGlobInput(const Json& input,const GlobServices& services){
  if(!input.contains("path")||input.at("path").is_null()||input.at("path")=="")
    return {true,std::nullopt,std::nullopt};
  const auto path=input.at("path").get<std::string>();
  const auto absolute=filesystem::expandPath(path,services.paths);
  if(absolute.starts_with("\\\\")||absolute.starts_with("//"))return {true,std::nullopt,std::nullopt};
  std::error_code ec;
  const auto status=fs::status(absolute,ec);
  if(ec){
    if(ec==std::errc::no_such_file_or_directory){
      if(!services.suggestPathUnderCwd)throw std::invalid_argument("Missing Glob binding: suggestPathUnderCwd");
      const auto suggestion=services.suggestPathUnderCwd(absolute);
      const auto cwd=services.paths.getCwd ? services.paths.getCwd() : fs::current_path().string();
      std::string message="Directory does not exist: "+path+". "+services.fileNotFoundCwdNote+" "+cwd+".";
      if(suggestion)message+=" Did you mean "+*suggestion+"?";
      return {false,message,1};
    }
    throw fs::filesystem_error("stat",absolute,ec);
  }
  if(!fs::is_directory(status))return {false,"Path is not a directory: "+path,2};
  return {true,std::nullopt,std::nullopt};
}

GadgetResult callGlob(const Json& input,const GadgetCallContext& context,const GlobServices& services){
  const auto now=[&](){
    if(services.nowMs)return services.nowMs();
    return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
  };
  const double start=now();
  const auto cwd=services.paths.getCwd ? services.paths.getCwd() : fs::current_path().string();
  const auto& limits=context.data.is_object()&&context.data.contains("globLimits")
    ? context.data.at("globLimits") : Json();
  const std::size_t limit=limits.is_object()&&limits.contains("maxResults")&&
    !limits.at("maxResults").is_null()?limits.at("maxResults").get<std::size_t>():100;
  const auto searchPath=input.contains("path")&&!input.at("path").is_null()&&
    input.at("path")!=""?filesystem::expandPath(input.at("path").get<std::string>(),services.paths):cwd;
  const auto permission=context.data.is_object()&&context.data.contains("toolPermissionContext")
    ? context.data.at("toolPermissionContext") : Json::object();
  const auto result=glob(input.at("pattern").get<std::string>(),searchPath,limit,0,
    permission,context.stopToken,services);
  Json filenames=Json::array();
  for(const auto& file:result.files)filenames.push_back(toRelativePath(file,cwd));
  GadgetResult gadget;
  gadget.data=Json{{"filenames",filenames},{"durationMs",now()-start},
    {"numFiles",filenames.size()},{"truncated",result.truncated}};
  return gadget;
}

Json mapGlobResult(const Json& output,const std::string& id){
  std::string content;
  for(const auto& file:output.at("filenames")){
    if(!content.empty())content+='\n';
    content+=file.get<std::string>();
  }
  if(content.empty())content="No files found";
  else if(output.at("truncated").get<bool>())
    content+="\n(Results are truncated. Consider using a more specific path or pattern.)";
  return Json{{"tool_use_id",id},{"type","tool_result"},{"content",content}};
}
} // namespace batbox::gadgets::files
