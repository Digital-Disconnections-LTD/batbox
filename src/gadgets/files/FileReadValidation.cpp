#include "gadgets/files/FileRead.hpp"
#include <filesystem>
#include <stdexcept>

namespace batbox::gadgets::files {
ValidationResult validateReadInput(const Json& input, const FileReadServices& services) {
  const std::string path=input.at("file_path").get<std::string>();
  if (input.contains("pages") && !input.at("pages").is_null()) {
    const std::string pages=input.at("pages").get<std::string>();
    const auto parsed=parsePDFPageRange(pages);
    if (!parsed) return {false,
      "Invalid pages parameter: \""+pages+"\". Use formats like \"1-5\", \"3\", or \"10-20\". Pages are 1-indexed.",7};
    const double rangeSize=std::isinf(parsed->lastPage) ? services.pdfMaxPagesPerRead+1 : parsed->lastPage-parsed->firstPage+1;
    if (rangeSize > services.pdfMaxPagesPerRead) return {false,
      "Page range \""+pages+"\" exceeds maximum of "+std::to_string(services.pdfMaxPagesPerRead)+" pages per request. Please use a smaller range.",8};
  }
  const auto fullPath=filesystem::expandPath(path,services.paths);
  if (!services.matchingReadDenyRule) throw std::invalid_argument("Missing FileRead binding: matchingReadDenyRule");
  if (services.matchingReadDenyRule(fullPath)) return {false,
    "File is in a directory that is denied by your permission settings.",1};
  if (fullPath.starts_with("\\\\") || fullPath.starts_with("//")) return {true,std::nullopt,std::nullopt};
  auto ext=std::filesystem::path(fullPath).extension().string();
  std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  const std::string bare=ext.empty() ? "" : ext.substr(1);
  if (hasBinaryExtension(fullPath) && !isPDFExtension(ext) &&
      bare!="png" && bare!="jpg" && bare!="jpeg" && bare!="gif" && bare!="webp")
    return {false,"This tool cannot read binary files. The file appears to be a binary "+ext+" file. Please use appropriate tools for binary file analysis.",4};
  if (isBlockedDevicePath(fullPath)) return {false,
    "Cannot read '"+path+"': this device file would block or produce infinite output.",9};
  return {true,std::nullopt,std::nullopt};
}
} // namespace batbox::gadgets::files
