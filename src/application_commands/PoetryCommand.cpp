#include "application_commands/PoetryCommand.hpp"
#include "utils/envUtils.hpp"
#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace batbox::application_commands {
namespace {
constexpr std::size_t previewLines=12,previewWidth=100;
}
std::string poetryCommandText(const poetryEntries::SystemPoetrySource& source,
    bool simpleMode){
  std::ostringstream text;
  text<<"System poetry (system prompt): "<<poetryEntries::describeSystemPoetry(source);
  for(const auto& warning:poetryEntries::systemPoetryWarnings(source))
    text<<"\nWarning: "<<warning;
  if(simpleMode)text<<"\nBATBOX_SIMPLE is set, so this Flight uses the short built-in prompt instead.";
  if(source.fromFile)text<<"\nEdits apply from the next request.";
  else if(!source.path.empty())text<<"\nCustomise it: run batbox poetry export, then edit "
    <<poetryEntries::displayPath(source.path)<<". Edits apply from the next request.";
  std::vector<std::string> lines;
  std::istringstream input(source.head);
  for(std::string line;std::getline(input,line);)
    if(!lines.empty()||!line.empty())lines.push_back(std::move(line));
  text<<"\n\nPreview";
  if(lines.size()>previewLines)text<<" (first "<<previewLines<<" of "<<lines.size()<<" lines)";
  text<<":";
  for(std::size_t i=0;i<lines.size()&&i<previewLines;++i){
    auto line=lines[i];
    if(line.size()>previewWidth){
      std::size_t cut=previewWidth;
      while(cut>0&&(static_cast<unsigned char>(line[cut])&0xC0)==0x80)--cut;
      line=line.substr(0,cut)+"…";
    }
    text<<"\n  "<<line;
  }
  text<<"\n\nFull text: batbox poetry";
  return text.str();
}
user_input::CommandEntry makePoetryCommand(event_loop::Loop& owner,
    std::shared_ptr<application_poetry::Provider> poetry){
  if(!poetry)throw std::invalid_argument("Poetry command needs the prompt provider");
  user_input::CommandEntry entry;
  entry.descriptor={{"type","local"},{"name","poetry"},
    {"description","Show the system prompt (poetry) source and a preview"},
    {"supportsNonInteractive",true}};
  entry.loadLocal=[&owner,poetry]{
    event_loop::Deferred<user_input::LocalCall> loaded(owner);
    loaded.resolve([&owner,poetry](const std::string&){
      event_loop::Deferred<Json> output(owner);
      try{
        const char* simpleValue=std::getenv("BATBOX_SIMPLE");
        const bool simple=simpleValue&&isEnvTruthy(Json(simpleValue));
        output.resolve(Json{{"type","text"},
          {"value",poetryCommandText(poetry->systemPoetrySource(),simple)}});
      }catch(...){output.reject(std::current_exception());}
      return output;
    });
    return loaded;
  };
  return entry;
}
} // namespace batbox::application_commands
