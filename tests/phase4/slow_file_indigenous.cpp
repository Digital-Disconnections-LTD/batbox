#include "diagnostics/SlowFileIndigenous.hpp"
#include "runtime/JsonValue.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>

int main(){
  const auto directory=std::filesystem::temp_directory_path()/
    ("batbox-slow-file-"+std::to_string(::getpid()));
  std::filesystem::create_directories(directory);
  const auto path=(directory/"settings.json").string();
  batbox::diagnostics::SlowOperationImports imports;
  imports.parseJson=[](const std::string& value){return batbox::value::parse(value);};
  batbox::diagnostics::bindIndigenousSlowFileOperations(imports);
  batbox::diagnostics::SlowOperations operations({},std::move(imports));
  operations.writeFileSyncDeprecated(path,std::string("alpha"),
    batbox::Json::object({{"flush",true},{"encoding","utf8"}}));
  operations.writeFileSyncDeprecated(path,
    std::vector<unsigned char>{'b','e','t','a'},
    batbox::Json::object({{"flag","a"}}));
  {
    std::ifstream file(path,std::ios::binary);
    const std::string content(std::istreambuf_iterator<char>(file),{});
    assert(content=="alphabeta");
  }
  operations.writeFileSyncDeprecated(path,std::string("new"),"utf8");
  {
    std::ifstream file(path,std::ios::binary);
    const std::string content(std::istreambuf_iterator<char>(file),{});
    assert(content=="new");
  }
  std::filesystem::remove_all(directory);
}
