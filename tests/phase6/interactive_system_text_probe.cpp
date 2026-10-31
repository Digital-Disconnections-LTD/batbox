#include "interactive/cards_system/SystemTextRouter.hpp"
#include <fstream>
#include <iostream>

using namespace batbox::interactive::cards_system;
using batbox::Json;

int main(int argc,char** argv) {
  if (argc!=3 || std::string(argv[1])!="--cases") return 2;
  std::ifstream stream(argv[2]);
  Json result=Json::array();
  for (const auto& item:Json::parse(stream)) {
    auto message=item;
    message["type"]="system";
    const auto card=classifySystemText(message,item.value("margin",false),
      item.value("verbose",false),item.value("transcript",false));
    result.push_back({{"name",item.at("name")},
      {"kind",systemTextKindName(card.kind)},
      {"content",card.content},{"margin",card.addMargin},
      {"dot",card.dot},{"dim",card.dim},
      {"color",card.color?Json(*card.color):Json(nullptr)},
      {"transcript",card.kind==SystemTextKind::StopHookSummary&&
                     card.transcriptMode}});
  }
  std::cout<<result.dump()<<'\n';
}
