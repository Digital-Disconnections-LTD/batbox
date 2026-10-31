#include "fs_operations/FsRoster.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <memory>

struct StubWritable final:batbox::fs_operations::WritableStream {
  bool write(std::string_view)override{return true;}
  void end()override{}
  void abort()override{}
  void onFinish(std::function<void()>)override{}
  void onError(std::function<void(const std::exception_ptr&)>)override{}
  void onDrain(std::function<void()>)override{}
  void onClose(std::function<void()>)override{}
};
int main(){
  using namespace batbox;
  using namespace batbox::fs_operations;
  Json input;std::cin>>input;
  Json calls=Json::array(),results=Json::array();
  auto original=std::make_shared<FsImplementation>();
  auto other=std::make_shared<FsImplementation>();
  auto stream=std::make_shared<StubWritable>();
  original->createWriteStream=[&](const std::string& path){
    calls.push_back(Json::array({"createWriteStream",path}));return stream;};
  FsRoster roster(original);
  for(const auto& action:input.at("actions")){
    const auto text=action.get<std::string>();
    if(text=="set")roster.setFsImplementation(other);
    else if(text=="null")roster.setFsImplementation(nullptr);
    else if(text=="reset")roster.setOriginalFsImplementation();
    else if(text=="get"){
      const auto active=roster.getFsImplementation();
      results.push_back(!active?Json(nullptr):active==original?Json("node"):Json("other"));
    }else if(text=="stream")results.push_back(
      createWriteStream(*original,"fixture")==stream);
  }
  std::cout<<value::stringify(Json::object({{"results",results},{"calls",calls}}))<<'\n';
}
