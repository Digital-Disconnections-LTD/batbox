#include "user_input/InputInternal.hpp"
#include <stdexcept>

namespace batbox::user_input::internal {
namespace {
std::optional<std::string> pathFor(const Json& value,const Json& stored){
  const auto& source=value::property(value,"sourcePath");
  if(source.is_string())return source.get<std::string>();
  const auto& id=value::property(value,"id");
  const std::string key=id.is_string()?id.get<std::string>():value::stringify(id);
  const auto& path=value::property(stored,key);
  if(path.is_string())return path.get<std::string>();
  return std::nullopt;
}
void completePasted(const std::shared_ptr<InputState>& state,
  const std::shared_ptr<std::vector<Json>>& results){
  try{
    for(std::size_t i=0;i<results->size();++i){
      const auto& pasted=state->imageContents.at(i);
      const auto& resized=results->at(i);
      const auto path=pathFor(pasted,state->storedPaths);
      auto dims=value::property(resized,"dimensions");
      if(!value::truthy(dims))dims=value::property(pasted,"dimensions");
      if(value::truthy(dims)){
        if(!state->services.createImageMetadataText)
          throw std::invalid_argument("Missing user input binding: createImageMetadataText");
        const auto metadata=state->services.createImageMetadataText(dims,path);
        if(metadata&& !metadata->empty())state->imageMetadata.push_back(*metadata);
      }else if(path)state->imageMetadata.push_back("[Image source: "+*path+"]");
      state->imageBlocks.push_back(resized.at("block"));
    }
    checkpoint(*state,"query_pasted_image_processing_end");
    loadAttachmentsAndRoute(state);
  }catch(...){fail(state,std::current_exception());}
}
void resizePasted(const std::shared_ptr<InputState>& state){
  try{
    checkpoint(*state,"query_pasted_image_processing_start");
    if(state->imageContents.empty()){
      auto empty=std::make_shared<std::vector<Json>>();
      completePasted(state,empty);return;
    }
    if(!state->services.resizeImageBlock)
      throw std::invalid_argument("Missing user input binding: resizeImageBlock");
    auto results=std::make_shared<std::vector<Json>>(state->imageContents.size());
    auto remaining=std::make_shared<std::size_t>(state->imageContents.size());
    auto failed=std::make_shared<bool>(false);
    for(std::size_t i=0;i<state->imageContents.size();++i){
      const auto& pasted=state->imageContents.at(i);
      const auto& rawMedia=value::property(pasted,"mediaType");
      Json block={{"type","image"},{"source",Json{{"type","base64"},
        {"media_type",value::truthy(rawMedia)?rawMedia:Json("image/png")},
        {"data",value::property(pasted,"content")}}}};
      auto resized=state->services.resizeImageBlock(block);
      resized.then([state,results,remaining,failed,i]
        (const Json* value,const std::exception_ptr& error){
        if(*failed)return;
        if(error){*failed=true;fail(state,error);return;}
        (*results)[i]=*value;
        if(--*remaining==0)completePasted(state,results);
      });
    }
  }catch(...){fail(state,std::current_exception());}
}
}
void processPasted(const std::shared_ptr<InputState>& state){
  try{
    if(!state->inputString&&state->request.mode!="prompt")
      throw std::runtime_error("Mode: "+state->request.mode+" requires a string input.");
    const auto& pasted=state->request.pastedContents;
    if(pasted.is_object()){
      for(const auto& key:value::objectKeys(pasted)){
        const auto& content=pasted.at(key);
        if(value::property(content,"type")=="image"&&
           value::property(content,"content").is_string()&&
           !text(value::property(content,"content")).empty()){
          state->imageContents.push_back(content);
          state->imagePasteIds.push_back(value::property(content,"id"));
        }
      }
      if(!state->services.storeImages)
        throw std::invalid_argument("Missing user input binding: storeImages");
      auto stored=state->services.storeImages(pasted);
      stored.then([state](const Json* value,const std::exception_ptr& error){
        if(error){fail(state,error);return;}
        state->storedPaths=*value;
        resizePasted(state);
      });
    }else resizePasted(state);
  }catch(...){fail(state,std::current_exception());}
}
} // namespace batbox::user_input::internal
