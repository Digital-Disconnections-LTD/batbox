#include "runtime/AsyncLaunch.hpp"
#include "gadgets/files/FileGadgetsInternal.hpp"
#include "gadgets/files/ActiveStash.hpp"
#include "filesystem/FileOps.hpp"
#include <limits>
#include <memory>

namespace batbox::gadgets::files {
namespace {
std::string parentUUID(const Json& parent){return stringValue(parent,"uuid");}
std::future<std::string> readyString(std::string value){
  return ::batbox::runtime::contextAsync(std::launch::deferred,[value=std::move(value)]{return value;});
}
std::future<ValidationResult> readyValidation(ValidationResult result){
  return ::batbox::runtime::contextAsync(std::launch::deferred,[result=std::move(result)]{return result;});
}
std::future<GadgetResult> readyResult(GadgetResult result){
  return ::batbox::runtime::contextAsync(std::launch::deferred,[result=std::move(result)]{return result;});
}
} // namespace

Gadget makeReadGadget(FileReadServices services,FileGadgetBindings bindings){
  auto state=std::make_shared<FileReadServices>(std::move(services));
  auto gadget=makeFileGadgetBase("Read","read files, images, PDFs, notebooks",
    std::numeric_limits<double>::infinity(),true,std::move(bindings));
  gadget.description=[](const Json&,const Json&){return readyString("Read a file from the local filesystem.");};
  gadget.poetry=[state](const PoetryContext&){return readyString(readPoetry(*state));};
  gadget.isConcurrencySafe=[](const Json&){return true;};
  gadget.isReadOnly=[](const Json&){return true;};
  gadget.toAutoClassifierInput=[](const Json& input)->Json{return input.value("file_path",Json());};
  gadget.isSearchOrReadCommand=[](const Json&)->Json{return {{"isSearch",false},{"isRead",true}};};
  gadget.getPath=[state](const Json& input){
    const auto path=stringValue(input,"file_path");
    return path.empty()?state->paths.getCwd():path;
  };
  bindFilePathBackfill(gadget,state->paths);
  gadget.extractSearchText=[](const Json&){return std::string();};
  gadget.validateInput=[state](const Json& input,const Json&){return readyValidation(validateReadInput(input,*state));};
  gadget.validateInputWithContext=[state](const Json& input,const GadgetCallContext& context){
    auto active=withActiveStash(*state,context);
    return readyValidation(validateReadInput(input,active));
  };
  gadget.call=[state](const Json& input,const GadgetCallContext& context,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const GadgetProgress&)>&){
    auto active=withActiveStash(*state,context);
    return readyResult(callRead(input,context,active));
  };
  gadget.mapGadgetResultToGadgetResultBlockParam=[state](const Json& data,const std::string& id){
    return mapReadResult(data,id,*state);
  };
  gadget.mapGadgetResultWithPresentationMetadata=[state](const Json& data,const std::string& id,
    const std::optional<Json>& metadata){
    std::optional<double> mtime;
    if(metadata&&metadata->is_object()&&metadata->contains("memoryMtimeMs")&&
       (*metadata)["memoryMtimeMs"].is_number())mtime=(*metadata)["memoryMtimeMs"].get<double>();
    return mapReadResult(data,id,*state,mtime);
  };
  return buildGadget(std::move(gadget));
}

Gadget makeWriteGadget(FileWriteServices services,FileGadgetBindings bindings){
  auto state=std::make_shared<FileWriteServices>(std::move(services));
  auto gadget=makeFileGadgetBase("Write","create or overwrite files",100000,true,std::move(bindings));
  gadget.description=[](const Json&,const Json&){return readyString("Write a file to the local filesystem.");};
  gadget.poetry=[](const PoetryContext&){return readyString(writePoetry());};
  gadget.toAutoClassifierInput=[](const Json& input)->Json{
    return stringValue(input,"file_path")+": "+stringValue(input,"content");
  };
  gadget.getPath=[](const Json& input){return stringValue(input,"file_path");};
  bindFilePathBackfill(gadget,state->paths);
  gadget.extractSearchText=[](const Json&){return std::string();};
  gadget.validateInput=[state](const Json& input,const Json&){return readyValidation(validateWriteInput(input,*state));};
  gadget.validateInputWithContext=[state](const Json& input,const GadgetCallContext& context){
    auto active=withActiveStash(*state,context);
    return readyValidation(validateWriteInput(input,active));
  };
  gadget.call=[state](const Json& input,const GadgetCallContext& context,
    const std::function<std::future<Json>(const Json&)>&,const Json& parent,
    const std::function<void(const GadgetProgress&)>&){
    auto active=withActiveStash(*state,context);
    return readyResult(callWrite(input,parentUUID(parent),context,active));
  };
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& data,const std::string& id){
    return mapWriteResult(data,id);
  };
  return buildGadget(std::move(gadget));
}
} // namespace batbox::gadgets::files
