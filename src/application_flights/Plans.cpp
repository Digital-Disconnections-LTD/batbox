#include "application_flights/Plans.hpp"
#include "application_flights/PlanWords.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Platform.hpp"
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <stdexcept>

namespace batbox::application_flights {
namespace {
std::optional<std::string> nonemptyString(const Json& value){
  if(value.is_string()&&!value.get_ref<const std::string&>().empty())
    return value.get<std::string>();
  return {};
}
bool missing(const std::filesystem::filesystem_error& error){
  return error.code()==std::errc::no_such_file_or_directory;
}
std::string read(const std::filesystem::path& path){
  std::ifstream input(path,std::ios::binary);
  if(!input)throw std::filesystem::filesystem_error("read plan",path,
    std::error_code(errno,std::generic_category()));
  std::string value((std::istreambuf_iterator<char>(input)),{});
  if(input.bad())throw std::runtime_error("Could not read plan file");
  return value::decodeUtf8(value,false);
}
void validComponent(const std::string& value){
  if(value.empty()||value=="."||value==".."||
      value.find_first_of("/\\")!=std::string::npos||value.find('\0')!=std::string::npos)
    throw std::invalid_argument("Invalid saved plan identity");
}
std::filesystem::path slugPath(const std::filesystem::path& directory,
    const std::string& slug){
  validComponent(slug);return directory/runtime::pathFromUtf8(slug+".md");
}
template<std::size_t N>std::string pick(const std::array<std::string_view,N>& words){
  // Use the UUID's first four bytes of cryptographic entropy as an unsigned
  // integer, then reduce modulo the table length.
  const auto random=platform::randomUUID();
  const auto value=static_cast<std::uint32_t>(std::stoul(random.substr(0,8),nullptr,16));
  return std::string(words[value%N]);
}
}
std::string generatePlanSlug(){
  const auto adjective=pick(plan_words::ADJECTIVES);
  const auto verb=pick(plan_words::VERBS);
  const auto noun=pick(plan_words::NOUNS);
  return adjective+"-"+verb+"-"+noun;
}
std::optional<std::string> planSlugFromLog(const Json& log){
  const auto& messages=value::property(log,"messages");
  if(messages.is_array())for(const auto& message:messages)
    if(auto value=nonemptyString(value::property(message,"slug")))return value;
  return {};
}
std::optional<std::string> recoverPlanFromMessages(const Json& log){
  const auto& messages=value::property(log,"messages");
  if(!messages.is_array())return {};
  for(auto it=messages.rbegin();it!=messages.rend();++it){
    const auto& type=value::property(*it,"type");
    if(type=="assistant"){
      const auto& blocks=value::property(value::property(*it,"message"),"content");
      if(blocks.is_array())for(const auto& block:blocks)
        if(value::property(block,"type")=="tool_use"&&
            value::property(block,"name")=="ExitPlanMode")
          if(auto plan=nonemptyString(value::property(value::property(block,"input"),"plan")))
            return plan;
    }
    if(type=="user")
      if(auto plan=nonemptyString(value::property(*it,"planContent")))return plan;
    if(type=="attachment"){
      const auto& attachment=value::property(*it,"attachment");
      if(value::property(attachment,"type")=="plan_file_reference")
        if(auto plan=nonemptyString(value::property(attachment,"planContent")))return plan;
    }
  }
  return {};
}
std::optional<Json> findFileSnapshotEntry(const Json& messages,const std::string& key){
  if(!messages.is_array())return {};
  for(auto it=messages.rbegin();it!=messages.rend();++it)
    if(value::property(*it,"type")=="system"&&
        value::property(*it,"subtype")=="file_snapshot"&&it->contains("snapshotFiles")){
      const auto& files=it->at("snapshotFiles");
      if(files.is_array())for(const auto& file:files)
        if(value::property(file,"key")==key)return file;
      return {}; // Only the most recent snapshot is authoritative.
    }
  return {};
}
PlanFiles::PlanFiles(std::function<std::filesystem::path()> directory,
    Logger logError,std::function<std::string()> generateSlug):
    directory_(std::move(directory)),logError_(std::move(logError)),
    generateSlug_(std::move(generateSlug)){
  if(!directory_||!generateSlug_)throw std::invalid_argument("Plan file providers required");
}
std::string PlanFiles::slug(const std::string& flight){
  validComponent(flight);
  if(const auto at=slugs_.find(flight);at!=slugs_.end())return at->second;
  const auto directory=directory_();
  std::string generated;
  for(int attempt=0;attempt<10;++attempt){
    generated=generateSlug_();
    if(!std::filesystem::exists(slugPath(directory,generated)))break;
  }
  slugs_[flight]=generated;return generated;
}
std::filesystem::path PlanFiles::path(const std::string& flight,
    const std::optional<std::string>& bat){
  auto name=slug(flight);
  if(bat&&!bat->empty()){validComponent(*bat);name+="-agent-"+*bat;}
  return slugPath(directory_(),name);
}
std::optional<std::string> PlanFiles::content(const std::string& flight,
    const std::optional<std::string>& bat){
  try{return read(path(flight,bat));}
  catch(const std::filesystem::filesystem_error& error){
    if(!missing(error)&&logError_)logError_(std::current_exception());
  }catch(...){if(logError_)logError_(std::current_exception());}
  return {};
}
bool PlanFiles::resume(const Json& log,const std::string& flight,bool remote){
  try{
    auto saved=planSlugFromLog(log);
    if(!saved){
      // Existing indigenous transcripts predate persisted slugs. Reuse only the
      // explicitly resumed flight's file; never scan other project plans.
      if(!std::filesystem::exists(slugPath(directory_(),flight)))return false;
      saved=flight;
    }
    validComponent(*saved);slugs_[flight]=*saved;
    const auto file=slugPath(directory_(),*saved);
    try{read(file);return true;}
    catch(const std::filesystem::filesystem_error& error){if(!missing(error))throw;}
    if(!remote)return false;
    const auto snapshot=findFileSnapshotEntry(value::property(log,"messages"),"plan");
    auto recovered=snapshot?nonemptyString(value::property(*snapshot,"content")):
      std::optional<std::string>{};
    if(!recovered)recovered=recoverPlanFromMessages(log);
    if(!recovered)return false;
    std::ofstream output(file,std::ios::binary);
    output<<*recovered;output.close();
    if(!output)throw std::runtime_error("Could not restore plan file");
    return true;
  }catch(...){if(logError_)logError_(std::current_exception());return false;}
}
bool PlanFiles::fork(const Json& log,const std::string& targetFlight){
  try{
    auto source=planSlugFromLog(log);
    if(!source){
      // Compatibility with the indigenous flight-id plan filenames.
      source=nonemptyString(value::property(log,"sessionId"));
      if(!source||!std::filesystem::exists(slugPath(directory_(),*source)))return false;
    }
    const auto original=slugPath(directory_(),*source);
    const auto target=path(targetFlight);
    if(original==target)throw std::invalid_argument("Forked plan must have a distinct identity");
    std::filesystem::copy_file(original,target,std::filesystem::copy_options::overwrite_existing);
    return true;
  }catch(const std::filesystem::filesystem_error& error){
    if(!missing(error)&&logError_)logError_(std::current_exception());
  }catch(...){if(logError_)logError_(std::current_exception());}
  return false;
}
} // namespace batbox::application_flights
