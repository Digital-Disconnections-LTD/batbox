#include "poetryEntries/AsyncSystemPoetryInternal.hpp"
#include "poetryEntries/SystemHead.hpp"

namespace batbox::poetryEntries::async_poetry {
void Run::finish(const std::vector<SectionValue>& values){
  try{
    if(values.size()!=sectionNames.size())
      throw std::logic_error("System prompt sections and names differ");
    need(services.systemHead,"systemHead");const auto head=services.systemHead();
    need(services.usingYourGadgetsSection,"usingYourToolsSection");
    std::vector<HeadSection> sections{{kUsingYourToolsSection,
      services.usingYourGadgetsSection(enabled),false}};
    for(std::size_t i=0;i<values.size();++i)sections.push_back({sectionNames[i],values[i]});
    need(services.globalStashScope,"globalCacheScope");
    output=renderSystemHead(head,sections,services.globalStashScope());
    result.resolve(std::move(output));
  }catch(...){result.reject(std::current_exception());}
}
}
