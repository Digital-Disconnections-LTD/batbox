#include "gadgets/files/FileRead.hpp"
#include "gadgets/files/Notebook.hpp"
#include "formatting/Format.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
using batbox::Json;
namespace files=batbox::gadgets::files;
static Json evaluate(const Json& t){
  const auto path=t.at("input").at("file_path").get<std::string>();
  batbox::file_state_stash::FileStateStash state(100);
  if(t.value("priorRead",false)&&std::filesystem::exists(path)){
    const auto content=batbox::filesystem::readFileBytes(path);
    const auto& input=t.at("input");
    state.set(path,files::ReadSnapshot{content,batbox::filesystem::getFileModificationTime(path),false,
      static_cast<double>(input.value("offset",1)),
      input.contains("limit")?std::optional<double>(input.at("limit").get<double>()):std::nullopt});
  }
  std::set<std::string> triggers,skillTriggers;
  std::vector<std::string> events;
  files::FileReadServices s;
  s.paths.getCwd=[&](){return t.at("cwd").get<std::string>();};
  s.paths.homeDirectory=[](){return std::string("/home/batbox");};
  s.paths.platform=[](){return std::string("linux");};
  s.paths.normalizeNFC=[](const std::string& p){return p;};
  s.readFileState=&state;s.nestedMemoryAttachmentTriggers=&triggers;
  s.dynamicSkillDirTriggers=&skillTriggers;
  batbox::messages::Services messages;
  messages.randomUUID=[](){return std::string("uuid-1");};
  messages.nowISO=[](){return std::string("now");};
  s.messageServices=&messages;
  s.formatFileSize=batbox::formatting::formatFileSize;
  s.range.formatFileSize=batbox::formatting::formatFileSize;
  s.dedupKillswitch=[&](){return t.value("dedupKillswitch",false);};
  s.simpleMode=[&](){return t.value("simple",true);};
  s.discoverSkillDirsForPaths=[&](const std::string&,const std::string&){
    std::vector<std::string> dirs;if(t.contains("skillDirs"))for(const auto& d:t.at("skillDirs"))dirs.push_back(d.get<std::string>());
    return dirs;};
  s.addSkillDirectories=[&](const std::vector<std::string>& dirs){
    std::string all;for(const auto& dir:dirs){if(!all.empty())all+=',';all+=dir;}events.push_back("addSkills:"+all);};
  s.activateConditionalSkillsForPaths=[&](const std::string&,const std::string&){events.push_back("activateSkills");};
  s.findSimilarFile=[&](const std::string&){return t.value("similar",std::string());};
  s.suggestPathUnderCwd=[&](const std::string&)->std::optional<std::string>{
    if(t.contains("suggestion"))return t.at("suggestion").get<std::string>();return std::nullopt;};
  s.roughTokenCountEstimationForFileType=[](const std::string& content,const std::string&){
    return batbox::value::utf16CodeUnits(content).size()/4.0;};
  s.countTokensWithAPI=[&](const std::string& content)->std::optional<double>{
    if(t.contains("tokenCount"))return t.at("tokenCount").get<double>();
    return batbox::value::utf16CodeUnits(content).size()/2.0;};
  files::notebook::Services ns;
  ns.paths=s.paths;ns.formatOutput=[](const std::string& text){return batbox::value::substring(text,0,40);};
  s.readNotebook=[&](const std::string& p){return files::notebook::readNotebook(p,ns);};
  s.mapNotebookCellsToGadgetResult=files::notebook::mapNotebookCellsToGadgetResult;
  s.image.detectImageFormatFromBuffer=[&](const std::string&){return t.value("imageMediaType",std::string("image/png"));};
  s.image.maybeResizeAndDownsampleImageBuffer=[](const std::string& b,std::size_t,const std::string& f){
    return files::ImageTransform{b,f,Json()};};
  s.image.compressImageBufferWithTokenLimit=[](const std::string&,std::size_t,const std::string&){
    return files::ImageCompressed{"YQ==","image/png"};};
  s.image.fallbackJpegResize=[](const std::string& b){return b;};
  s.image.createImageMetadataText=[](const Json&){return std::string();};
  s.getPDFPageCount=[&](const std::string&)->std::optional<int>{return t.value("pdfPages",1);};
  s.isPDFSupported=[&](){return t.value("pdfSupported",true);};
  s.extractPDFPages=[&](const std::string& p,const std::optional<files::PdfPageRange>&){
    return Json{{"success",true},{"data",{{"type","parts"},{"file",{{"filePath",p},
      {"count",0},{"originalSize",std::filesystem::file_size(p)},{"outputDir",t.value("pdfOutputDir",t.at("cwd").get<std::string>())}}}}}};};
  s.readPDF=[&](const std::string& p){
    return Json{{"success",true},{"data",{{"type","pdf"},{"file",{{"filePath",p},
      {"base64","JVBERi0="},{"originalSize",std::filesystem::file_size(p)}}}}}};};
  s.isAutoMemFile=[&](const std::string&){return t.value("autoMemory",false);};
  batbox::gadgets::GadgetCallContext actual{};
  actual.data=Json::object();
  if(t.contains("fileReadingLimits"))actual.data["fileReadingLimits"]=t.at("fileReadingLimits");
  const auto result=files::callRead(t.at("input"),actual,s);
  Json newContents;
  if(result.newMessages){newContents=Json::array();for(const auto& m:*result.newMessages)newContents.push_back(m.at("message").at("content"));}
  Json snapshot;
  if(auto found=state.get(path)){
    snapshot=Json{{"content",found->content},{"timestamp",found->timestamp},
      {"offset",found->offset.value_or(0)}};
    if(found->limit)snapshot["limit"]=*found->limit;
  }
  Json triggered=Json::array(),skills=Json::array();
  for(const auto& x:triggers)triggered.push_back(x);
  for(const auto& x:skillTriggers)skills.push_back(x);
  return Json{{"data",result.data},{"newContents",newContents},
    {"presentationMetadata",result.presentationMetadata?*result.presentationMetadata:Json()},
    {"events",events},{"triggered",triggered},{"skillTriggers",skills},{"state",snapshot}};
}
int main(){std::string input((std::istreambuf_iterator<char>(std::cin)),{});
  try{std::cout<<batbox::value::stringify(Json{{"result",evaluate(batbox::value::parse(input))}})<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}}
