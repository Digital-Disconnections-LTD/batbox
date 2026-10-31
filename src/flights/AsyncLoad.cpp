#include "flights/AsyncLoad.hpp"
#include "flights/LoadParsing.hpp"

namespace batbox::flights {
namespace {
struct ReadState:std::enable_shared_from_this<ReadState> {
  event_loop::Loop& loop;
  std::string path;
  bool keepAllLeaves,preserved=false;
  AsyncLoadServices services;
  event_loop::Deferred<LoadedTranscript> result;
  LoadedTranscript output;
  std::optional<std::string> buffer;
  std::vector<std::string> metadata;
  ReadState(event_loop::Loop& loop,std::string path,bool keep,AsyncLoadServices services):
    loop(loop),path(std::move(path)),keepAllLeaves(keep),services(std::move(services)),result(loop){}
  void finish(){
    try{finishLoadedTranscript(output,services.pruneLeaves);result.resolve(std::move(output));}
    catch(...){result.reject(std::current_exception());}
  }
  void parse(){
    try{
      if(!keepAllLeaves&&!preserved&&!services.disablePrecompactSkip&&buffer->size()>services.skipThreshold)
        buffer=services.walkChainBeforeParse(*buffer);
      parseLoadedTranscriptBuffers(*buffer,metadata,output);
    }catch(const std::exception&){} // Retain map entries populated before the failure.
    finish();
  }
  void read(){
    if(buffer){parse();return;}
    const auto self=shared_from_this();
    try{services.readFile(path).then([self](const auto* bytes,const auto& error){
      if(error){self->finish();return;}self->buffer=*bytes;self->parse();
    });}catch(const std::exception&){finish();}
  }
  void scan(std::size_t size){
    const auto self=shared_from_this();
    try{services.readTranscriptForLoad(path,size).then([self](const ScanResult* scan,const auto& error){
      if(error){self->finish();return;}
      self->buffer=scan->postBoundaryBuf;self->preserved=scan->hasPreservedSegment;
      if(!scan->boundaryStartOffset){self->read();return;}
      try{self->services.scanPreBoundaryMetadata(self->path,scan->boundaryStartOffset).then(
        [self](const auto* metadata,const auto& failure){
          if(failure){self->finish();return;}self->metadata=*metadata;self->read();
        });}catch(const std::exception&){self->finish();}
    });}catch(const std::exception&){finish();}
  }
  void start(){
    if(services.disablePrecompactSkip){read();return;}
    const auto self=shared_from_this();
    try{services.stat(path).then([self](const std::uintmax_t* size,const auto& error){
      if(error){self->finish();return;}
      if(*size>self->services.skipThreshold)self->scan(*size);else self->read();
    });}catch(const std::exception&){finish();}
  }
};
}
event_loop::Deferred<LoadedTranscript> loadTranscriptFileAsync(event_loop::Loop& loop,
  const std::string& path,bool keepAllLeaves,const AsyncLoadServices& services){
  const auto state=std::make_shared<ReadState>(loop,path,keepAllLeaves,services);
  state->start();return state->result;
}
} // namespace batbox::flights
