#include "attachments/files/FileAttachments.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace batbox::attachments::files {
namespace {
using Result=std::optional<Json>;
Json readInput(const std::string& filename,const ReadOptions& options){
  Json input={{"file_path",filename}};
  if(options.offset)input["offset"]=*options.offset;
  if(options.limit)input["limit"]=*options.limit;
  return input;
}
std::size_t newlineCount(const std::string& text){
  return static_cast<std::size_t>(std::count(text.begin(),text.end(),'\n'));
}
bool readLimitError(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const gadgets::files::MaxFileReadTokenExceededError&){return true;}
  catch(const filesystem::FileTooLargeError&){return true;}
  catch(...){return false;}
}
struct Run:std::enable_shared_from_this<Run> {
  std::string filename;
  execution::Context& context;
  Mode mode;
  ReadOptions options;
  Services services;
  event_loop::Deferred<Result> output;
  file_state_stash::FileStateStash::Value prior;
  Run(std::string path,execution::Context& turn,Mode kind,ReadOptions range,
      const Services& imports):filename(std::move(path)),context(turn),
    mode(kind),options(range),services(imports),output(imports.loop){}
  void reject(std::exception_ptr error){output.reject(error);}
  void empty(){output.resolve(std::nullopt);}
  bool denied(){
    if(!services.getAppState||!services.isFileReadDenied)
      throw std::invalid_argument("File attachment permission imports required");
    return services.isFileReadDenied(filename,services.getAppState());
  }
  std::string display(){
    if(!services.displayPath)throw std::invalid_argument(
      "File attachment display path import required");
    return services.displayPath(filename);
  }
  void start(){
    try{
      if(denied()){empty();return;}
      if(mode==Mode::AtMention){
        if(!services.withinReadSizeLimit)
          throw std::invalid_argument("File size-limit import required");
        if(!services.withinReadSizeLimit(filename,services.maxReadSizeBytes)&&
           !gadgets::files::isPDFExtension(
             std::filesystem::path(filename).extension().string())){
          if(!services.stat)throw std::invalid_argument("File stat import required");
          services.stat(filename).then([self=shared_from_this()](
              const Stat* value,const std::exception_ptr& error){
            if(!error&&value){self->empty();return;}
            self->pdf();
          });
          return;
        }
      }
      pdf();
    }catch(...){reject(std::current_exception());}
  }
  void pdf(){
    try{
      if(mode==Mode::Compact){alreadyRead();return;}
      tryGetPDFReference(filename,services).then([self=shared_from_this()](
          const Result* value,const std::exception_ptr& error){
        if(error){self->reject(error);return;}
        if(value&&*value){self->output.resolve(**value);return;}
        self->alreadyRead();
      });
    }catch(...){reject(std::current_exception());}
  }
  void alreadyRead(){
    try{
      if(context.readFileState)
        prior=context.readFileState->get(filename);
      if(!prior||mode!=Mode::AtMention){validate();return;}
      if(!services.modificationTime)
        throw std::invalid_argument("File modification-time import required");
      services.modificationTime(filename).then([self=shared_from_this()](
          const double* mtime,const std::exception_ptr& error){
        if(!error&&mtime&&self->prior&&
           self->prior->timestamp<=*mtime&&*mtime==self->prior->timestamp){
          try{
            const auto lines=newlineCount(self->prior->content)+1;
            self->output.resolve(Json{{"type","already_read_file"},
              {"filename",self->filename},{"displayPath",self->display()},
              {"content",{{"type","text"},{"file",{
                {"filePath",self->filename},{"content",self->prior->content},
                {"numLines",lines},
                {"startLine",self->options.offset.value_or(1)},
                {"totalLines",lines}}}}}});
          }catch(...){self->reject(std::current_exception());}
          return;
        }
        self->validate();
      });
    }catch(...){empty();}
  }
  void validate(){
    try{
      if(!services.validateRead)
        throw std::invalid_argument("Read validation import required");
      services.validateRead(readInput(filename,options),context).then(
        [self=shared_from_this()](const gadgets::ValidationResult* value,
            const std::exception_ptr& error){
          if(error||!value){self->empty();return;}
          if(!value->result){self->empty();return;}
          self->read();
        });
    }catch(...){empty();}
  }
  void read(){
    try{
      if(!services.callRead)throw std::invalid_argument("Read call import required");
      services.callRead(readInput(filename,options),context).then(
        [self=shared_from_this()](const gadgets::GadgetResult* result,
            const std::exception_ptr& error){
          if(error){
            if(readLimitError(error))self->truncated();else self->empty();
            return;
          }
          if(!result){self->empty();return;}
          try{self->output.resolve(Json{{"type","file"},
            {"filename",self->filename},{"content",result->data},
            {"displayPath",self->display()}});}
          catch(...){self->empty();}
        });
    }catch(...){empty();}
  }
  void truncated(){
    try{
      if(mode==Mode::Compact){
        output.resolve(Json{{"type","compact_file_reference"},
          {"filename",filename},{"displayPath",display()}});
        return;
      }
      if(denied()){empty();return;}
      if(!services.callRead)throw std::invalid_argument("Read call import required");
      ReadOptions range{options.offset.value_or(1),
        static_cast<double>(services.maxLinesToRead)};
      services.callRead(readInput(filename,range),context).then(
        [self=shared_from_this()](const gadgets::GadgetResult* result,
            const std::exception_ptr& error){
          if(error||!result){self->empty();return;}
          try{self->output.resolve(Json{{"type","file"},
            {"filename",self->filename},{"content",result->data},
            {"truncated",true},{"displayPath",self->display()}});}
          catch(...){self->empty();}
        });
    }catch(...){empty();}
  }
};
}
event_loop::Deferred<Result> generateFileAttachment(
    std::string filename,execution::Context& context,Mode mode,
    ReadOptions options,const Services& services){
  if(!services.loop.isOwnerThread())throw std::logic_error(
    "File attachment generation requires owner loop");
  if(!context.readFileState||!services.getAppState||!services.isFileReadDenied||
     !services.displayPath||!services.withinReadSizeLimit||!services.stat||
     !services.pdfPageCount||!services.modificationTime||
     !services.validateRead||!services.callRead)
    throw std::invalid_argument("File attachment cache and service imports required");
  auto run=std::make_shared<Run>(std::move(filename),context,mode,options,services);
  run->start();return run->output;
}
} // namespace batbox::attachments::files
