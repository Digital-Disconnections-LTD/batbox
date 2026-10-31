#include "auth/storage/Storage.hpp"
#include <cerrno>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace batbox::auth::storage {
namespace {
std::string storagePath(const PlaintextServices& services){
  return (std::filesystem::path(services.configHome())/
    services.fileName).string();
}
bool missing(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const std::system_error& caught){
    return caught.code().value()==ENOENT;
  }catch(...){return false;}
}
}
Storage makePlaintextStorage(event_loop::Loop& loop,
    PlaintextServices services){
  if(!services.configHome||!services.readFileSync||
      !services.readFileAsync||!services.mkdirSync||
      !services.writeFileSync||!services.chmodSync||
      !services.unlinkSync||services.fileName.empty())
    throw std::invalid_argument("Missing plaintext storage binding");
  Storage result;
  result.name="plaintext";
  result.read=[services]()->std::optional<Json>{
    const auto path=storagePath(services);
    try{
      const auto parsed=value::parse(services.readFileSync(path));
      return parsed.is_null()?std::nullopt:std::optional<Json>(parsed);
    }
    catch(...){return std::nullopt;}
  };
  result.readAsync=[&loop,services](){
    event_loop::Deferred<std::optional<Json>> output(loop);
    const auto path=storagePath(services);
    try{services.readFileAsync(path).then([output](
        const std::string* content,const std::exception_ptr& error){
      if(error){output.resolve(std::nullopt);return;}
      try{
        const auto parsed=value::parse(*content);
        output.resolve(parsed.is_null()?std::nullopt:
          std::optional<Json>(parsed));
      }
      catch(...){output.resolve(std::nullopt);}
    });}
    catch(...){output.resolve(std::nullopt);}
    return output;
  };
  result.update=[services](const Json& data){
    try{
      const auto directory=services.configHome();
      const auto path=(std::filesystem::path(directory)/
        services.fileName).string();
      try{services.mkdirSync(directory);}
      catch(const std::system_error& error){
        if(error.code().value()!=EEXIST)throw;
      }
      services.writeFileSync(path,value::stringify(data));
      services.chmodSync(path,0600);
      return Json{{"success",true},
        {"warning","Warning: Storing credentials in plaintext."}};
    }catch(...){return Json{{"success",false}};}
  };
  result.remove=[services](){
    const auto path=storagePath(services);
    try{services.unlinkSync(path);return true;}
    catch(...){return missing(std::current_exception());}
  };
  return result;
}
}
