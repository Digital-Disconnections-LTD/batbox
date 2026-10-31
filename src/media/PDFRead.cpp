#include "media/Media.hpp"
#include "media/Base64.hpp"
#include "media/Process.hpp"
#include "media/PDFFile.hpp"
#include "filesystem/FileOps.hpp"
#include "formatting/Format.hpp"
#include <filesystem>
#include <cmath>
#include <limits>
#include <regex>

namespace batbox::media {
namespace {
constexpr std::size_t pdfTargetRawSize=20*1024*1024;
Json error(const std::string& reason,const std::string& message){
  return {{"success",false},{"error",{{"reason",reason},{"message",message}}}};
}
} // namespace
Json readPDF(const std::string& filePath){
  try{
    const auto size=indigenous::statSize(filePath);
    if(size==0)return error("empty","PDF file is empty: "+filePath);
    if(size>pdfTargetRawSize)return error("too_large",
      "PDF file exceeds maximum allowed size of "+formatting::formatFileSize(pdfTargetRawSize)+".");
    const auto bytes=filesystem::readFileBytes(filePath);
    if(bytes.rfind("%PDF-",0)!=0)return error("corrupted",
      "File is not a valid PDF (missing %PDF- header): "+filePath);
    return {{"success",true},{"data",{{"type","pdf"},{"file",{{"filePath",filePath},
      {"base64",indigenous::base64Encode(bytes)},{"originalSize",size}}}}}};
  }catch(const std::exception& failure){return error("unknown",failure.what());}
}

std::optional<int> getPDFPageCount(const std::string& filePath){
  const auto process=indigenous::runProcess("pdfinfo",{filePath},10000);
  if(process.code!=0)return std::nullopt;
  static const std::regex pages(R"((?:^|\n)Pages:\s+(\d+))");
  std::smatch match;
  if(!std::regex_search(process.stdoutText,match,pages))return std::nullopt;
  const auto number=value::parseInt10(match[1].str());
  if(!std::isfinite(number)||number<0||number>std::numeric_limits<int>::max())return std::nullopt;
  return static_cast<int>(number);
}
} // namespace batbox::media
