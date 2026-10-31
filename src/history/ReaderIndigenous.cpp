#include "history/ReaderIndigenous.hpp"
#include "fs_operations/ReverseLines.hpp"
#include <cerrno>
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace batbox::history {
HistoryReaderImports indigenousHistoryReaderImports(event_loop::Loop& loop,
    fs_operations::NodeFsBytesImports bytes,
    std::function<std::string()> getFlightId,
    std::function<std::string()> getConfigHomeDir,
    std::function<Json(const std::string&)> jsonParse,
    std::function<void(const std::string&)> logForDebugging){
  HistoryReaderImports imports;
  imports.loop=&loop;
  imports.getFlightId=std::move(getFlightId);
  imports.getConfigHomeDir=std::move(getConfigHomeDir);
  imports.jsonParse=std::move(jsonParse);
  imports.logForDebugging=std::move(logForDebugging);
  imports.join=[](const std::string& left,const std::string& right){
    return (std::filesystem::path(left)/right).lexically_normal().string();};
  imports.readLinesReverse=[bytes=std::move(bytes)](const std::string& path){
    auto cursor=std::make_shared<fs_operations::ReverseLinesCursor>(bytes,path);
    return HistoryLineCursor{
      [cursor]{return cursor->next();},
      [cursor]{return cursor->close();}};
  };
  imports.getErrnoCode=[](const std::exception_ptr& error)->
      std::optional<std::string>{
    try{std::rethrow_exception(error);}
    catch(const std::system_error& value){
      if(value.code().value()==ENOENT)return "ENOENT";
      if(value.code().value()==EACCES)return "EACCES";
      return std::nullopt;
    }catch(...){return std::nullopt;}
  };
  imports.errorToString=[](const std::exception_ptr& error){
    try{std::rethrow_exception(error);}
    catch(const std::exception& value){return std::string("Error: ")+value.what();}
    catch(...){return std::string("Error");}
  };
  return imports;
}
}
