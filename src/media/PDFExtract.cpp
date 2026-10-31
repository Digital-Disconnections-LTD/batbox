#include "media/Media.hpp"
#include "media/Process.hpp"
#include "media/PDFFile.hpp"
#include "formatting/Format.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stdexcept>

namespace batbox::media {
namespace {
constexpr std::size_t pdfMaxExtractSize=100*1024*1024;
Json error(const std::string& reason,const std::string& message){
  return {{"success",false},{"error",{{"reason",reason},{"message",message}}}};
}
bool containsNoCase(std::string text,const std::string& needle){
  std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return std::tolower(c);});
  return text.find(needle)!=std::string::npos;
}
std::mutex availabilityMutex;
std::optional<bool> availabilityStash;
}
bool isPdftoppmAvailable(){
  std::lock_guard lock(availabilityMutex);
  if(availabilityStash)return *availabilityStash;
  const auto probe=indigenous::runProcess("pdftoppm",{"-v"},5000);
  // Missing executable has no child stderr, matching execFileNoThrow's failed spawn.
  availabilityStash=probe.code==0||(probe.code!=127&&!probe.stderrText.empty());
  return *availabilityStash;
}
void resetPdftoppmStash(){
  std::lock_guard lock(availabilityMutex);availabilityStash.reset();
}
Json extractPDFPages(const std::string& filePath,
  const std::optional<gadgets::files::PdfPageRange>& pages,const Config& config){
  try{
    const auto size=indigenous::statSize(filePath);
    if(size==0)return error("empty","PDF file is empty: "+filePath);
    if(size>pdfMaxExtractSize)return error("too_large",
      "PDF file exceeds maximum allowed size for text extraction ("+
      formatting::formatFileSize(pdfMaxExtractSize)+").");
    if(!isPdftoppmAvailable())return error("unavailable",
      "pdftoppm is not installed. Install poppler-utils (e.g. `brew install poppler` or `apt-get install poppler-utils`) to enable PDF page rendering.");
    if(!config.getGadgetResultsDir||!config.randomUUID)
      throw std::invalid_argument("Missing PDF storage directory or UUID binding");
    const auto outputDir=std::filesystem::path(config.getGadgetResultsDir())/("pdf-"+config.randomUUID());
    std::filesystem::create_directories(outputDir);
    std::vector<std::string> args={"-jpeg","-r","100"};
    if(pages&&pages->firstPage!=0)args.insert(args.end(),{"-f",value::numberToString(pages->firstPage)});
    if(pages&&pages->lastPage!=0&&std::isfinite(pages->lastPage))
      args.insert(args.end(),{"-l",value::numberToString(pages->lastPage)});
    args.push_back(filePath);args.push_back((outputDir/"page").string());
    const auto process=indigenous::runProcess("pdftoppm",args,120000);
    if(process.code!=0){
      if(containsNoCase(process.stderrText,"password"))return error("password_protected",
        "PDF is password-protected. Please provide an unprotected version.");
      if(containsNoCase(process.stderrText,"damaged")||containsNoCase(process.stderrText,"corrupt")||
         containsNoCase(process.stderrText,"invalid"))return error("corrupted","PDF file is corrupted or invalid.");
      return error("unknown","pdftoppm failed: "+process.stderrText);
    }
    std::size_t count=0;
    for(const auto& item:std::filesystem::directory_iterator(outputDir))
      if(item.path().filename().string().ends_with(".jpg"))++count;
    if(count==0)return error("corrupted","pdftoppm produced no output pages. The PDF may be invalid.");
    return {{"success",true},{"data",{{"type","parts"},{"file",{{"filePath",filePath},
      {"originalSize",size},{"outputDir",outputDir.string()},{"count",count}}}}}};
  }catch(const std::exception& failure){return error("unknown",failure.what());}
}
} // namespace batbox::media
