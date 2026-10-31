#include "fs_operations/PathChecks.hpp"
#include "fs_operations/IndigenousPathChecks.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_set>

using namespace batbox;
int main(){
  const std::string raw(std::istreambuf_iterator<char>(std::cin),{});
  const auto input=value::parse(raw);
  Json calls=Json::array(),output=Json::array();
  std::unordered_set<std::string> loaded;
  std::vector<std::string> loadedOrder;
  for(const auto& value:input.value("loaded",Json::array())){
    auto text=value.get<std::string>();loaded.insert(text);loadedOrder.push_back(text);
  }
  const auto& nodes=input.value("nodes",Json::object());
  auto find=[&nodes](const std::string& path)->Json{
    const auto item=nodes.find(path);return item==nodes.end()?Json(nullptr):*item;};
  fs_operations::FileView view;
  view.existsSync=[&](const std::string& path){
    calls.push_back(Json::array({"exists",path}));
    const auto node=find(path);
    return !node.is_null()&&node.value("exists",true);
  };
  view.lstatSync=[&](const std::string& path){
    calls.push_back(Json::array({"lstat",path}));
    const auto node=find(path);
    if(node.is_null()||node.value("lstatError",false))
      throw std::runtime_error("lstat failed");
    const auto type=node.value("type",std::string());
    return fs_operations::Stats{type=="fifo",type=="socket",type=="char",
      type=="block",type=="symlink"};
  };
  view.realpathSync=[&](const std::string& path){
    calls.push_back(Json::array({"realpath",path}));
    const auto node=find(path);
    if(node.is_null()||node.value("realpathError",false))
      throw std::runtime_error("realpath failed");
    return node.value("realpath",path);
  };
  view.readlinkSync=[&](const std::string& path){
    calls.push_back(Json::array({"readlink",path}));
    const auto node=find(path);
    if(node.is_null()||node.value("readlinkError",false)||!node.contains("linkTarget"))
      throw std::runtime_error("readlink failed");
    return node.at("linkTarget").get<std::string>();
  };
  auto path=fs_operations::indigenousPathImports([]{return std::string("/work");},
    [&]{return input.value("home",std::string("/home/user"));});
  for(const auto& action:input.at("actions")){
    Json result=nullptr;
    const auto op=action.at("op").get<std::string>();
    const auto name=action.at("path").get<std::string>();
    try{
      if(op=="safe"){
        const auto resolved=filesystem::safeResolvePath(view,name);
        result=Json::object({{"resolvedPath",resolved.resolvedPath},
          {"isSymlink",resolved.isSymlink},{"isCanonical",resolved.isCanonical}});
      }
      if(op=="duplicate"){
        const auto before=loaded;
        const auto duplicate=fs_operations::isDuplicatePath(view,name,loaded);
        if(!duplicate)for(const auto& item:loaded)
          if(!before.contains(item))loadedOrder.push_back(item);
        result=duplicate;
      }
      if(op=="deepest"){
        const auto resolved=fs_operations::resolveDeepestExistingAncestorSync(view,name,path);
        if(resolved)result=*resolved;
      }
      if(op=="permission")
        result=fs_operations::getPathsForPermissionCheck(name,view,path);
    }catch(const std::exception& error){result=Json::object({{"error",error.what()}});}
    output.push_back(Json::object({{"result",result},{"loaded",loadedOrder},
      {"calls",calls}}));
  }
  std::cout<<value::stringify(output);
}
