#include "gadgets/files/FileReadInternal.hpp"
#include "gadgets/files/ActiveTriggers.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace batbox::gadgets::files {
GadgetResult callRead(const Json& input,const GadgetCallContext& context,FileReadServices& services){
  requireReadBinding(services.readFileState!=nullptr,"readFileState");
  const auto& limits=context.data.is_object()&&context.data.contains("fileReadingLimits")
    ? context.data.at("fileReadingLimits") : Json();
  ReadArguments args;
  args.inputPath=input.at("file_path").get<std::string>();
  args.offset=input.value("offset",static_cast<std::size_t>(1));
  if(input.contains("limit")&&!input.at("limit").is_null())args.limit=input.at("limit").get<std::size_t>();
  if(input.contains("pages")&&!input.at("pages").is_null())args.pages=input.at("pages").get<std::string>();
  args.maxSizeBytes=limits.is_object()&&limits.contains("maxSizeBytes")&&!limits.at("maxSizeBytes").is_null()
    ? limits.at("maxSizeBytes").get<std::size_t>() : services.defaultLimits.maxSizeBytes;
  args.maxTokens=limits.is_object()&&limits.contains("maxTokens")&&!limits.at("maxTokens").is_null()
    ? limits.at("maxTokens").get<std::size_t>() : services.defaultLimits.maxTokens;
  args.extension=std::filesystem::path(args.inputPath).extension().string();
  if(args.extension.starts_with('.'))args.extension.erase(0,1);
  std::transform(args.extension.begin(),args.extension.end(),args.extension.begin(),
    [](unsigned char c){return static_cast<char>(std::tolower(c));});
  args.fullPath=filesystem::expandPath(args.inputPath,services.paths);
  args.resolvedPath=args.fullPath;
  requireReadBinding(static_cast<bool>(services.dedupKillswitch),"dedupKillswitch");
  if(!services.dedupKillswitch()){
    const auto found=services.readFileState->get(args.fullPath);
    if(found&&!found->isPartialView&&found->offset){
      if(found->offset==args.offset&&found->limit==args.limit){
        try{
          if(filesystem::getFileModificationTime(args.fullPath)==found->timestamp)
            return GadgetResult{Json{{"type","file_unchanged"},{"file",{{"filePath",args.inputPath}}}}};
        }catch(const std::exception&){}
      }
    }
  }
  requireReadBinding(static_cast<bool>(services.simpleMode),"simpleMode");
  if(!services.simpleMode()){
    requireReadBinding(static_cast<bool>(services.discoverSkillDirsForPaths),"discoverSkillDirsForPaths");
    requireReadBinding(static_cast<bool>(services.activateConditionalSkillsForPaths),"activateConditionalSkillsForPaths");
    const auto cwd=services.paths.getCwd ? services.paths.getCwd() : std::filesystem::current_path().string();
    const auto dirs=services.discoverSkillDirsForPaths(args.fullPath,cwd);
    if(!dirs.empty()){
      for(const auto& dir:dirs)recordDynamicSkillDir(services,dir);
      requireReadBinding(static_cast<bool>(services.addSkillDirectories),"addSkillDirectories");
      services.addSkillDirectories(dirs);
    }
    services.activateConditionalSkillsForPaths(args.fullPath,cwd);
  }
  try{return callReadInner(args,context,services);}
  catch(const filesystem::RangeError& e){if(e.code!=std::errc::no_such_file_or_directory)throw;}
  catch(const std::system_error& e){
    if(e.code()!=std::errc::no_such_file_or_directory)throw;
  }
  if(const auto alternate=getAlternateScreenshotPath(args.fullPath)){
    try{args.resolvedPath=*alternate;return callReadInner(args,context,services);}
    catch(const filesystem::RangeError& e){if(e.code!=std::errc::no_such_file_or_directory)throw;}
    catch(const std::system_error& e){if(e.code()!=std::errc::no_such_file_or_directory)throw;}
  }
  requireReadBinding(static_cast<bool>(services.findSimilarFile),"findSimilarFile");
  requireReadBinding(static_cast<bool>(services.suggestPathUnderCwd),"suggestPathUnderCwd");
  const auto similar=services.findSimilarFile(args.fullPath);
  const auto cwdSuggestion=services.suggestPathUnderCwd(args.fullPath);
  const auto cwd=services.paths.getCwd ? services.paths.getCwd() : std::filesystem::current_path().string();
  std::string message="File does not exist. "+services.fileNotFoundCwdNote+" "+cwd+".";
  if(cwdSuggestion)message+=" Did you mean "+*cwdSuggestion+"?";
  else if(!similar.empty())message+=" Did you mean "+similar+"?";
  throw std::runtime_error(message);
}
} // namespace batbox::gadgets::files
