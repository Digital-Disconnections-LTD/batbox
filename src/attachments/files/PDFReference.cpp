#include "attachments/files/FileAttachments.hpp"
#include <cmath>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace batbox::attachments::files {
namespace {
using Result=std::optional<Json>;
struct Run {
  event_loop::Deferred<Result> output;
  std::string filename;
  std::function<std::string(const std::string&)> displayPath;
  double size=0;
  std::optional<int> pageCount;
  bool gotStat=false,gotPages=false,failed=false;
  int threshold;
  Run(event_loop::Loop& loop,std::string path,
      std::function<std::string(const std::string&)> display,int limit):
    output(loop),filename(std::move(path)),displayPath(std::move(display)),
    threshold(limit){}
  void finish(){
    if(failed||!gotStat||!gotPages)return;
    const double effective=pageCount?static_cast<double>(*pageCount):
      std::ceil(size/(100.0*1024.0));
    if(effective<=threshold){output.resolve(std::nullopt);return;}
    try{output.resolve(Json{{"type","pdf_reference"},{"filename",filename},
      {"pageCount",effective},{"fileSize",size},
      {"displayPath",displayPath(filename)}});}
    catch(...){error();}
  }
  void error(){if(!failed){failed=true;output.resolve(std::nullopt);}}
};
}
event_loop::Deferred<Result> tryGetPDFReference(
    std::string filename,const Services& services){
  event_loop::Deferred<Result> empty(services.loop);
  if(!gadgets::files::isPDFExtension(
      std::filesystem::path(filename).extension().string())){
    empty.resolve(std::nullopt);return empty;
  }
  if(!services.stat||!services.pdfPageCount||!services.displayPath)
    throw std::invalid_argument("PDF reference imports required");
  try{
    auto run=std::make_shared<Run>(services.loop,filename,
      services.displayPath,services.pdfInlineThreshold);
    auto stat=services.stat(filename);
    auto pages=services.pdfPageCount(filename);
    stat.then([run](const Stat* value,const std::exception_ptr& error){
      if(error||!value){run->error();return;}
      run->size=value->size;run->gotStat=true;run->finish();
    });
    pages.then([run](const std::optional<int>* value,
        const std::exception_ptr& error){
      if(error||!value){run->error();return;}
      run->pageCount=*value;run->gotPages=true;run->finish();
    });
    return run->output;
  }catch(...){empty.resolve(std::nullopt);return empty;}
}
} // namespace batbox::attachments::files
