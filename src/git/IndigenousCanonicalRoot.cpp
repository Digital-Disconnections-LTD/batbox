#include "git/CanonicalRoot.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace batbox::git {
CanonicalRootImports indigenousCanonicalRootImports(){
  CanonicalRootImports imports;
  imports.readFileSync=[](const std::string& path){
    std::ifstream stream(runtime::pathFromUtf8(path),std::ios::binary);
    if(!stream)throw std::system_error(errno,std::generic_category(),"readFileSync");
    std::string data(std::istreambuf_iterator<char>{stream},{});
    if(stream.bad())throw std::runtime_error("readFileSync failed");
    return value::decodeUtf8(data,true);
  };
  imports.realpathSync=[](const std::string& path){
#ifdef _WIN32
    return runtime::pathToUtf8(std::filesystem::canonical(runtime::pathFromUtf8(path)));
#else
    const auto encoded=value::encodeUtf8(path);
    char* raw=::realpath(encoded.c_str(),nullptr);
    if(!raw)throw std::system_error(errno,std::generic_category(),"realpathSync");
    std::string result(raw);std::free(raw);return result;
#endif
  };
  return imports;
}
} // namespace batbox::git
