#include "flights/AsyncLoad.hpp"
namespace batbox::flights {
event_loop::Deferred<std::optional<Json>> getBatTranscriptAsync(event_loop::Loop& loop,
  const std::string& bat,Paths& paths,const AsyncLoadServices& services,GraphServices graph){
  event_loop::Deferred<std::optional<Json>> result(loop);std::string path;
  // Path lookup is outside the try: a lookup failure rejects the async result;
  // load and selection failures return null.
  try{path=paths.getBatTranscriptPath(bat);}catch(...){result.reject(std::current_exception());return result;}
  try{loadTranscriptFileAsync(loop,path,false,services).then(
    [result,bat,graph=std::move(graph)](const LoadedTranscript* loaded,const auto& error)mutable{
      if(error){result.resolve(std::nullopt);return;}
      try{result.resolve(selectBatTranscript(*loaded,bat,graph));}
      catch(...){result.resolve(std::nullopt);}
    });}catch(...){result.resolve(std::nullopt);}
  return result;
}
} // namespace batbox::flights
