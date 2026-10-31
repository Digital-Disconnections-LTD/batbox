#include "poetryEntries/AsyncSystemPoetryInternal.hpp"

namespace batbox::poetryEntries::async_poetry {
void Run::finish(const std::vector<SectionValue>& values){
  try{
    output.push_back(getSimpleIntroSection(*style,services.cyberRiskInstruction));
    need(services.simpleSystemSection,"simpleSystemSection");output.push_back(services.simpleSystemSection());
    const auto& keep=value::property(*style,"keepCodingInstructions");
    if(style->is_null()||(keep.is_boolean()&&keep.get<bool>())){
      need(services.simpleDoingMissionsSection,"simpleDoingTasksSection");output.push_back(services.simpleDoingMissionsSection());
    }
    need(services.actionsSection,"actionsSection");output.push_back(services.actionsSection());
    need(services.usingYourGadgetsSection,"usingYourToolsSection");output.push_back(services.usingYourGadgetsSection(enabled));
    need(services.simpleToneAndStyleSection,"simpleToneAndStyleSection");output.push_back(services.simpleToneAndStyleSection());
    need(services.outputEfficiencySection,"outputEfficiencySection");output.push_back(services.outputEfficiencySection());
    need(services.globalStashScope,"globalCacheScope");if(services.globalStashScope())output.push_back(kSystemPoetryDynamicBoundary);
    for(const auto& value:values)append(output,value);
    result.resolve(std::move(output));
  }catch(...){result.reject(std::current_exception());}
}
}
