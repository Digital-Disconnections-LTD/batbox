#include "gadgets/files/Grep.hpp"
#include <filesystem>
#include <stdexcept>

namespace batbox::gadgets::files {
ValidationResult validateGrepInput(const Json& input,const GrepServices& services){
  if(!input.contains("path")||input.at("path").is_null()||input.at("path")=="")
    return {true,std::nullopt,std::nullopt};
  const auto path=input.at("path").get<std::string>();
  const auto absolute=filesystem::expandPath(path,services.paths);
  if(absolute.starts_with("\\\\")||absolute.starts_with("//"))return {true,std::nullopt,std::nullopt};
  std::error_code ec;
  (void)std::filesystem::status(absolute,ec);
  if(ec){
    if(ec==std::errc::no_such_file_or_directory){
      if(!services.suggestPathUnderCwd)throw std::invalid_argument("Missing Grep binding: suggestPathUnderCwd");
      const auto suggestion=services.suggestPathUnderCwd(absolute);
      const auto cwd=services.paths.getCwd ? services.paths.getCwd() : std::filesystem::current_path().string();
      std::string message="Path does not exist: "+path+". "+services.fileNotFoundCwdNote+" "+cwd+".";
      if(suggestion)message+=" Did you mean "+*suggestion+"?";
      return {false,message,1};
    }
    throw std::filesystem::filesystem_error("stat",absolute,ec);
  }
  return {true,std::nullopt,std::nullopt};
}
} // namespace batbox::gadgets::files
