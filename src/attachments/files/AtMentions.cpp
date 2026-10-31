#include "attachments/files/FileAttachments.hpp"
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace batbox::attachments::files {
namespace {
struct Run:std::enable_shared_from_this<Run> {
  execution::Context& context;
  Services services;
  app_state::AppStateRef appState;
  event_loop::Deferred<Json> output;
  std::vector<std::string> mentions;
  std::vector<std::optional<Json>> results;
  std::size_t remaining;
  Run(execution::Context& turn,const Services& imports,
      std::vector<std::string> files):context(turn),services(imports),
    output(imports.loop),mentions(std::move(files)),results(mentions.size()),
    remaining(mentions.size()){}
  void done(std::size_t index,std::optional<Json> value){
    results[index]=std::move(value);
    if(--remaining)return;
    Json attachments=Json::array();
    for(const auto& item:results)if(item)attachments.push_back(*item);
    output.resolve(std::move(attachments));
  }
  void file(std::size_t index,const std::string& filename,
      const FileMention& parsed){
    ReadOptions options;
    options.offset=parsed.lineStart;
    if(parsed.lineStart&&parsed.lineEnd&&*parsed.lineStart!=0&&
       *parsed.lineEnd!=0)
      options.limit=*parsed.lineEnd-*parsed.lineStart+1;
    try{
      generateFileAttachment(filename,context,Mode::AtMention,options,services)
        .then([self=shared_from_this(),index](const std::optional<Json>* value,
            const std::exception_ptr& error){
          self->done(index,error||!value?std::nullopt:*value);
        });
    }catch(...){done(index,std::nullopt);}
  }
  void startOne(std::size_t index){
    try{
      const auto parsed=parseAtMentionedFileLines(mentions[index]);
      const auto filename=services.expandPath(parsed.filename);
      if(services.isFileReadDenied(filename,appState)){
        done(index,std::nullopt);return;
      }
      try{
        services.stat(filename).then([self=shared_from_this(),index,filename,
            parsed](const Stat* stats,const std::exception_ptr& error){
          if(error||!stats){self->file(index,filename,parsed);return;}
          if(!stats->isDirectory){self->file(index,filename,parsed);return;}
          try{
            self->services.readDirectory(filename).then(
              [self,index,filename](const std::vector<std::string>* entries,
                  const std::exception_ptr& directoryError){
                if(directoryError||!entries){self->done(index,std::nullopt);return;}
                try{
                  const auto count=std::min<std::size_t>(entries->size(),1000);
                  std::string content;
                  for(std::size_t i=0;i<count;++i){
                    if(i)content+='\n';
                    content+=(*entries)[i];
                  }
                  if(entries->size()>count){
                    if(count)content+='\n';
                    content+="\xE2\x80\xA6 and "+
                      std::to_string(entries->size()-count)+" more entries";
                  }
                  self->done(index,Json{{"type","directory"},{"path",filename},
                    {"content",content},
                    {"displayPath",self->services.displayPath(filename)}});
                }catch(...){self->done(index,std::nullopt);}
              });
          }catch(...){self->done(index,std::nullopt);}
        });
      }catch(...){file(index,filename,parsed);}
    }catch(...){done(index,std::nullopt);}
  }
  void start(){
    try{
      appState=services.getAppState();
      for(std::size_t i=0;i<mentions.size();++i)startOne(i);
    }catch(...){output.reject(std::current_exception());}
  }
};
}
event_loop::Deferred<Json> processAtMentionedFiles(
    const std::string& input,execution::Context& context,
    const Services& services){
  if(!services.loop.isOwnerThread())throw std::logic_error(
    "File mentions require owner loop");
  if(!services.getAppState||!services.expandPath||!services.displayPath||
     !services.isFileReadDenied||!services.stat||!services.readDirectory)
    throw std::invalid_argument("File mention service imports required");
  auto files=extractAtMentionedFiles(input);
  if(files.empty()){
    event_loop::Deferred<Json> result(services.loop);
    result.resolve(Json::array());return result;
  }
  auto run=std::make_shared<Run>(context,services,std::move(files));
  run->start();return run->output;
}
} // namespace batbox::attachments::files
