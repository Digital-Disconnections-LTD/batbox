#include "interactive/navigation/ResubmitText.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::interactive::navigation;
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  const std::vector<Json> contents={
    "hello","<bash-input>ls -la</bash-input>",
    "<command-name>/review</command-name><command-args>file</command-args>",
    "<command-name>/clear</command-name>",
    "<ide_selection>hidden</ide_selection> visible <code>x</code>",
    Json::array({{{"type","text"},{"text","  a  "}},
      {{"type","image"}},{{"type","text"},{"text","b  "}}}),
    Json::array({{{"type","image"}}}),
    "<bash-input></bash-input>plain"
  };
  for(const auto& content:contents){
    auto message=std::make_shared<Json>(Json{{"type","user"},
      {"message",{{"content",content}}}});
    const auto result=textForResubmit(message);
    std::cout<<(result?Json{{"text",result->text},{"mode",result->mode}}:
      Json()).dump()<<'\n';
  }
}
