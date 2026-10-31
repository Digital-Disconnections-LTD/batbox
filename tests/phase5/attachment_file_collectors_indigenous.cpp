#include "attachments/files/FileAttachments.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include "runtime/Unicode.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unistd.h>

using namespace batbox;
namespace {
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr error;
  pending.then([&](const T* result,const std::exception_ptr& failure){
    error=failure;if(result)value=*result;
  });
  for(int i=0;i<5000&&!value&&!error;++i){
    loop.runReady();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if(error)std::rethrow_exception(error);
  if(!value)throw std::runtime_error("Native attachment did not settle");
  return *value;
}
void require(bool condition,const char* message){
  if(!condition)throw std::runtime_error(message);
}
}
int main(){
  namespace fs=std::filesystem;
  const auto dir=fs::temp_directory_path()/
    ("batbox-attachment-files-"+std::to_string(::getpid()));
  fs::create_directories(dir/"folder");
  struct Cleanup{fs::path path;~Cleanup(){std::error_code error;
    fs::remove_all(path,error);}} cleanup{dir};
  const auto textFile=dir/"sample.txt";
  {std::ofstream file(textFile);file<<"one\ntwo\nthree\n";}
  {std::ofstream file(dir/"folder"/"entry.txt");file<<"entry";}
  const auto largePdf=dir/"large.pdf";
  {std::ofstream file(largePdf,std::ios::binary);
    file<<"%PDF-1.4\n"<<std::string(12*102400,'x');}
  event_loop::Loop loop;
  gadgets::files::FileReadServices read;
  read.paths.getCwd=[dir]{return dir.string();};
  read.paths.homeDirectory=[dir]{return dir.string();};
  read.paths.platform=[](){return std::string("linux");};
  read.paths.normalizeNFC=[](const std::string& value){
    return value::normalizeNFC(value);
  };
  read.matchingReadDenyRule=[](const std::string&){return false;};
  read.dedupKillswitch=[](){return true;};
  read.simpleMode=[](){return true;};
  read.roughTokenCountEstimationForFileType=[](const std::string&,
    const std::string&){return 0.0;};
  read.isAutoMemFile=[](const std::string&){return false;};
  read.range.formatFileSize=[](double size){
    return std::to_string(static_cast<std::size_t>(size))+" bytes";
  };
  auto app=std::make_shared<app_state::AppState>();
  auto services=attachments::files::makeIndigenousFileServices(loop,read,
    [app]{return app;},[](const std::string&,
      const app_state::AppStateRef&){return false;});
  execution::Context context;
  context.readFileState=file_state_stash::createFileStateStashWithSizeLimit(100);
  context.nestedMemoryAttachmentTriggers=
    std::make_shared<runtime::OrderedStringSet>();
  const auto mentioned=await(loop,attachments::files::processAtMentionedFiles(
    "Read @sample.txt#L2-2",context,services));
  require(mentioned.size()==1&&mentioned[0]["type"]=="file",
    "Range mention did not use native Read");
  const auto content=mentioned[0]["content"]["file"]["content"].get<std::string>();
  require(content.find("two")!=std::string::npos&&
    content.find("one")==std::string::npos,"Range Read returned wrong lines");
  require(context.readFileState->has(textFile.string()),
    "Native Read did not update typed cache");
  require(context.nestedMemoryAttachmentTriggers->contains(textFile.string()),
    "Native Read did not publish typed nested-memory trigger");
  const auto again=await(loop,attachments::files::generateFileAttachment(
    textFile.string(),context,attachments::files::Mode::AtMention,
    attachments::files::ReadOptions{2,1},services));
  require(again&&(*again)["type"]=="already_read_file",
    "Unchanged file did not use typed cache attachment");
  const auto directory=await(loop,attachments::files::processAtMentionedFiles(
    "Inspect @folder",context,services));
  require(directory.size()==1&&directory[0]["type"]=="directory"&&
    directory[0]["content"]=="entry.txt","Real directory mention failed");
  const auto pdf=await(loop,attachments::files::tryGetPDFReference(
    largePdf.string(),services));
  require(pdf&&(*pdf)["type"]=="pdf_reference"&&
    (*pdf)["pageCount"]>=11,"Real PDF size fallback failed");
  const auto limited=dir/"limited.txt";
  {std::ofstream file(limited);file<<"alpha\nbeta\ngamma\n";}
  context.data["fileReadingLimits"]={{"maxSizeBytes",5}};
  const auto truncated=await(loop,attachments::files::generateFileAttachment(
    limited.string(),context,attachments::files::Mode::AtMention,{},services));
  require(truncated&&(*truncated)["type"]=="file"&&
    (*truncated)["truncated"]==true,
    "Typed byte-limit error did not trigger native truncated Read");
  const auto compact=await(loop,attachments::files::generateFileAttachment(
    limited.string(),context,attachments::files::Mode::Compact,{},services));
  require(compact&&(*compact)["type"]=="compact_file_reference",
    "Typed byte-limit error did not trigger compact reference");
  context.data["fileReadingLimits"]={{"maxSizeBytes",262144},{"maxTokens",5}};
  read.roughTokenCountEstimationForFileType=[](const std::string&,
    const std::string&){return 100.0;};
  read.countTokensWithAPI=[](const std::string&){
    return std::optional<double>(100.0);
  };
  const auto tokenLimited=await(loop,attachments::files::generateFileAttachment(
    limited.string(),context,attachments::files::Mode::Compact,{},services));
  require(tokenLimited&&(*tokenLimited)["type"]=="compact_file_reference",
    "Typed token-limit error did not trigger compact reference");
  std::cout<<"native file attachments passed\n";
}
