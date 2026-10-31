#include "runtime/AsyncLaunch.hpp"
#include "gadgets/files/FileGadgetsInternal.hpp"
#include "gadgets/files/ActiveStash.hpp"
#include "filesystem/FileOps.hpp"
#include <memory>

namespace batbox::gadgets::files {
namespace {
std::future<std::string> readyString(std::string value){
  return ::batbox::runtime::contextAsync(std::launch::deferred,[value=std::move(value)]{return value;});
}
std::future<ValidationResult> readyValidation(ValidationResult value){
  return ::batbox::runtime::contextAsync(std::launch::deferred,[value=std::move(value)]{return value;});
}
std::future<GadgetResult> readyResult(GadgetResult value){
  return ::batbox::runtime::contextAsync(std::launch::deferred,[value=std::move(value)]{return value;});
}
std::string cwdOr(const Json& input,const char* key,const filesystem::PathServices& paths,bool expand){
  const auto path=stringValue(input,key);
  return path.empty()?paths.getCwd():expand?filesystem::expandPath(path,paths):path;
}
Json searchFlag(){return {{"isSearch",true},{"isRead",false}};}
Json asEditInput(const Json& input){
  return {{"file_path",input.value("file_path",Json())},
    {"edits",Json::array({{{"old_string",input.value("old_string",Json())},
      {"new_string",input.value("new_string",Json())},
      {"replace_all",input.value("replace_all",Json(false))}}})}};
}
} // namespace

Gadget makeEditGadget(FileEditServices services,FileGadgetBindings bindings){
  auto state=std::make_shared<FileEditServices>(std::move(services));
  auto gadget=makeFileGadgetBase("Edit","modify file contents in place",100000,true,std::move(bindings));
  gadget.description=[](const Json&,const Json&){return readyString("A tool for editing files");};
  gadget.poetry=[state](const PoetryContext&){
    if(!state->compactLinePrefixEnabled||!state->conciseEditHint)
      throw std::invalid_argument("Missing Edit prompt bindings");
    return readyString(editPoetry(state->compactLinePrefixEnabled(),state->conciseEditHint()));
  };
  gadget.toAutoClassifierInput=[](const Json& input)->Json{
    return stringValue(input,"file_path")+": "+stringValue(input,"new_string");
  };
  gadget.getPath=[](const Json& input){return stringValue(input,"file_path");};
  bindFilePathBackfill(gadget,state->paths);
  gadget.validateInput=[state](const Json& input,const Json&){return readyValidation(validateEditInput(input,*state));};
  gadget.validateInputWithContext=[state](const Json& input,const GadgetCallContext& context){
    auto active=withActiveStash(*state,context);
    return readyValidation(validateEditInput(input,active));
  };
  gadget.inputsEquivalent=[state](const Json& a,const Json& b){
    return edit::areFileEditsInputsEquivalent(asEditInput(a),asEditInput(b),state->editUtils);
  };
  gadget.call=[state](const Json& input,const GadgetCallContext& context,
    const std::function<std::future<Json>(const Json&)>&,const Json& parent,
    const std::function<void(const GadgetProgress&)>&){
    auto active=withActiveStash(*state,context);
    return readyResult(callEdit(input,stringValue(parent,"uuid"),context,active));
  };
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& data,const std::string& id){
    return mapEditResult(data,id);
  };
  return buildGadget(std::move(gadget));
}

Gadget makeGlobGadget(GlobServices services,FileGadgetBindings bindings){
  auto state=std::make_shared<GlobServices>(std::move(services));
  auto gadget=makeFileGadgetBase("Glob","find files by name pattern or wildcard",100000,false,std::move(bindings));
  gadget.description=[](const Json&,const Json&){return readyString(globPoetry());};
  gadget.poetry=[](const PoetryContext&){return readyString(globPoetry());};
  gadget.isConcurrencySafe=[](const Json&){return true;};
  gadget.isReadOnly=[](const Json&){return true;};
  gadget.toAutoClassifierInput=[](const Json& input)->Json{return input.value("pattern",Json());};
  gadget.isSearchOrReadCommand=[](const Json&)->Json{return searchFlag();};
  gadget.getPath=[state](const Json& input){return cwdOr(input,"path",state->paths,true);};
  gadget.validateInput=[state](const Json& input,const Json&){return readyValidation(validateGlobInput(input,*state));};
  gadget.extractSearchText=[](const Json& result){
    std::string value;
    if(result.is_object()&&result.value("filenames",Json()).is_array())
      for(const auto& file:result.at("filenames")){if(!value.empty())value+='\n';value+=file.get<std::string>();}
    return value;
  };
  gadget.call=[state](const Json& input,const GadgetCallContext& context,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const GadgetProgress&)>&){return readyResult(callGlob(input,context,*state));};
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& data,const std::string& id){return mapGlobResult(data,id);};
  return buildGadget(std::move(gadget));
}

Gadget makeGrepGadget(GrepServices services,FileGadgetBindings bindings){
  auto state=std::make_shared<GrepServices>(std::move(services));
  const auto bat=bindings.batGadgetName,bash=bindings.bashGadgetName;
  auto gadget=makeFileGadgetBase("Grep","search file contents with regex (ripgrep)",20000,true,std::move(bindings));
  gadget.userFacingName=[](const Json&){return std::string("Search");};
  gadget.description=[bat,bash](const Json&,const Json&){return readyString(grepPoetry(bat,bash));};
  gadget.poetry=[bat,bash](const PoetryContext&){return readyString(grepPoetry(bat,bash));};
  gadget.isConcurrencySafe=[](const Json&){return true;};
  gadget.isReadOnly=[](const Json&){return true;};
  gadget.toAutoClassifierInput=[](const Json& input)->Json{
    const auto path=stringValue(input,"path"),pattern=stringValue(input,"pattern");
    return path.empty()?pattern:pattern+" in "+path;
  };
  gadget.isSearchOrReadCommand=[](const Json&)->Json{return searchFlag();};
  gadget.getPath=[state](const Json& input){return cwdOr(input,"path",state->paths,false);};
  gadget.validateInput=[state](const Json& input,const Json&){return readyValidation(validateGrepInput(input,*state));};
  gadget.extractSearchText=[](const Json& result){
    if(result.is_object()&&result.value("mode",Json())=="content"&&
       value::truthy(result.value("content",Json())))return stringValue(result,"content");
    std::string value;
    if(result.is_object()&&result.value("filenames",Json()).is_array())
      for(const auto& file:result.at("filenames")){if(!value.empty())value+='\n';value+=file.get<std::string>();}
    return value;
  };
  gadget.call=[state](const Json& input,const GadgetCallContext& context,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const GadgetProgress&)>&){return readyResult(callGrep(input,context,*state));};
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& data,const std::string& id){return mapGrepResult(data,id);};
  return buildGadget(std::move(gadget));
}
} // namespace batbox::gadgets::files
