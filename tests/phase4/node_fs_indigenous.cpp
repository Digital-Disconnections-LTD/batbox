#include "fs_operations/NodeFsIndigenous.hpp"
#include "runtime/JsonValue.hpp"
#include <cassert>
#include <filesystem>
#include <memory>
#include <string>
#include <unistd.h>

int main(){
  namespace fs=std::filesystem;
  const auto root=fs::canonical(fs::temp_directory_path())/
    ("batbox-node-fs-"+std::to_string(::getpid()));
  batbox::diagnostics::SlowOperationImports slowImports;
  slowImports.parseJson=[](const std::string& text){return batbox::value::parse(text);};
  auto slow=std::make_shared<batbox::diagnostics::SlowOperations>(
    batbox::diagnostics::SlowOperationConfig{},std::move(slowImports));
  batbox::fs_operations::NodeFsSync node(
    batbox::fs_operations::indigenousNodeFsSyncImports(slow));
  const auto directory=(root/"nested").string();
  node.mkdirSync(directory,batbox::Json::object({{"mode",0700}}));
  assert(node.existsSync(directory));
  const auto file=(root/"nested"/"log.txt").string();
  node.appendFileSync(file,"hello",batbox::Json::object({{"mode",0600}}));
  node.appendFileSync(file," world",batbox::Json::object({{"mode",0600}}));
  assert(node.readFileSync(file,"utf8")=="hello world");
  const auto first=node.readSync(file,5);
  assert(first.bytesRead==5&&std::string(first.buffer.begin(),first.buffer.end())=="hello");
  const auto copied=(root/"nested"/"copy.txt").string();
  node.copyFileSync(file,copied);
  assert(node.statSync(copied)["size"]==11);
  const auto link=(root/"nested"/"link.txt").string();
  node.symlinkSync(copied,link,"file");
  assert(node.readlinkSync(link)==copied);
  assert(node.realpathSync(link)==copied);
  assert(node.lstatSync(link)["isSymbolicLink"]==true);
  assert(node.readdirSync(directory).size()==3);
  assert(!node.isDirEmptySync(directory));
  node.unlinkSync(link);
  node.unlinkSync(copied);
  node.unlinkSync(file);
  assert(node.isDirEmptySync(directory));
  node.rmdirSync(directory);
  node.rmdirSync(root.string());
  assert(!fs::exists(root));
}
