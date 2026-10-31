#include "fs_operations/NodeFsSync.hpp"
#include "runtime/Unicode.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::fs_operations {
namespace {
template<class T>void require(const T& callback){
  if(!callback)throw std::invalid_argument("Node filesystem import is required");
}
std::optional<std::string> code(const NodeFsSyncImports& imports,
    const std::exception_ptr& error){
  require(imports.errnoCode);
  return imports.errnoCode(error);
}
}

std::string NodeFsSync::cwd()const{require(imports_.cwd);return imports_.cwd();}

bool NodeFsSync::existsSync(const std::string& path)const{
  require(imports_.existsSync);
  return diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.existsSync(",")"},{Json(path)},
    [&]{return imports_.existsSync(path);});
}
Json NodeFsSync::statSync(const std::string& path)const{
  require(imports_.statSync);
  return diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.statSync(",")"},{Json(path)},
    [&]{return imports_.statSync(path);});
}
Json NodeFsSync::lstatSync(const std::string& path)const{
  require(imports_.lstatSync);
  return diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.lstatSync(",")"},{Json(path)},
    [&]{return imports_.lstatSync(path);});
}
std::string NodeFsSync::readFileSync(const std::string& path,
    const std::string& encoding)const{
  require(imports_.readFileSync);
  return diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.readFileSync(",")"},{Json(path)},
    [&]{return imports_.readFileSync(path,encoding);});
}
std::vector<unsigned char> NodeFsSync::readFileBytesSync(
    const std::string& path)const{
  require(imports_.readFileBytesSync);
  return diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.readFileBytesSync(",")"},{Json(path)},
    [&]{return imports_.readFileBytesSync(path);});
}
ReadSyncResult NodeFsSync::readSync(const std::string& path,std::size_t length)const{
  require(imports_.openSync);require(imports_.readSync);require(imports_.closeSync);
  return diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.readSync(",", "," bytes)"},{Json(path),Json(length)},[&]{
      std::optional<int> fd;
      ReadSyncResult result;
      try{
        fd=imports_.openSync(path,"r",std::nullopt);
        result.buffer.resize(length);
        result.bytesRead=imports_.readSync(*fd,result.buffer,0,length,0);
      }catch(...){
        const auto original=std::current_exception();
        if(fd&&*fd)imports_.closeSync(*fd);
        std::rethrow_exception(original);
      }
      if(fd&&*fd)imports_.closeSync(*fd);
      return result;
    });
}

void NodeFsSync::appendFileSync(const std::string& path,
    const std::string& data,std::optional<Json> options)const{
  require(imports_.appendPathSync);
  diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.appendFileSync(",", "," chars)"},
    {Json(path),Json(value::stringLength(data))},[&]{
      const bool explicitMode=options&&options->is_object()&&options->contains("mode");
      if(explicitMode){
        require(imports_.openSync);require(imports_.appendFdSync);
        require(imports_.closeSync);
        try{
          const int fd=imports_.openSync(path,"ax",(*options)["mode"]);
          try{imports_.appendFdSync(fd,data);}
          catch(...){const auto original=std::current_exception();
            imports_.closeSync(fd);std::rethrow_exception(original);}
          imports_.closeSync(fd);
          return;
        }catch(...){
          if(code(imports_,std::current_exception())!="EEXIST")throw;
        }
      }
      imports_.appendPathSync(path,data);
    });
}

void NodeFsSync::copyFileSync(const std::string& source,
    const std::string& destination)const{
  require(imports_.copyFileSync);
  diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.copyFileSync("," → ",")"},{Json(source),Json(destination)},
    [&]{imports_.copyFileSync(source,destination);});
}
void NodeFsSync::unlinkSync(const std::string& path)const{
  require(imports_.unlinkSync);
  diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.unlinkSync(",")"},{Json(path)},[&]{imports_.unlinkSync(path);});
}
void NodeFsSync::renameSync(const std::string& source,
    const std::string& destination)const{
  require(imports_.renameSync);
  diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.renameSync("," → ",")"},{Json(source),Json(destination)},
    [&]{imports_.renameSync(source,destination);});
}
void NodeFsSync::linkSync(const std::string& target,
    const std::string& path)const{
  require(imports_.linkSync);
  diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.linkSync("," → ",")"},{Json(target),Json(path)},
    [&]{imports_.linkSync(target,path);});
}
void NodeFsSync::symlinkSync(const std::string& target,
    const std::string& path,std::optional<std::string> type)const{
  require(imports_.symlinkSync);
  diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.symlinkSync("," → ",")"},{Json(target),Json(path)},
    [&]{imports_.symlinkSync(target,path,type);});
}
std::string NodeFsSync::readlinkSync(const std::string& path)const{
  require(imports_.readlinkSync);
  return diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.readlinkSync(",")"},{Json(path)},
    [&]{return imports_.readlinkSync(path);});
}
std::string NodeFsSync::realpathSync(const std::string& path)const{
  require(imports_.realpathSync);
  return diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.realpathSync(",")"},{Json(path)},
    [&]{return value::normalizeNFC(imports_.realpathSync(path));});
}
void NodeFsSync::mkdirSync(const std::string& path,
    std::optional<Json> options)const{
  require(imports_.mkdirSync);
  diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.mkdirSync(",")"},{Json(path)},[&]{
      Json mkdirOptions=Json::object({{"recursive",true}});
      if(options&&options->is_object()&&options->contains("mode"))
        mkdirOptions["mode"]=(*options)["mode"];
      try{imports_.mkdirSync(path,mkdirOptions);}
      catch(...){if(code(imports_,std::current_exception())!="EEXIST")throw;}
    });
}
Json NodeFsSync::readdirSync(const std::string& path)const{
  require(imports_.readdirSync);
  return diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.readdirSync(",")"},{Json(path)},
    [&]{return imports_.readdirSync(path,true);});
}
std::vector<std::string> NodeFsSync::readdirStringSync(
    const std::string& path)const{
  require(imports_.readdirStringSync);
  return diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.readdirStringSync(",")"},{Json(path)},
    [&]{return imports_.readdirStringSync(path);});
}
bool NodeFsSync::isDirEmptySync(const std::string& path)const{
  return diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.isDirEmptySync(",")"},{Json(path)},
    [&]{return readdirSync(path).empty();});
}
void NodeFsSync::rmdirSync(const std::string& path)const{
  require(imports_.rmdirSync);
  diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.rmdirSync(",")"},{Json(path)},[&]{imports_.rmdirSync(path);});
}
void NodeFsSync::rmSync(const std::string& path,std::optional<Json> options)const{
  require(imports_.rmSync);
  diagnostics::withSlowScope(imports_.beginSlowLogging,
    {"fs.rmSync(",")"},{Json(path)},[&]{imports_.rmSync(path,options);});
}
}
