#include "mcp/Collapse.hpp"
#include "mcp/CollapseData.hpp"
#include <unicode/ustring.h>
#include <stdexcept>
#include <unordered_set>

namespace batbox::mcp {
namespace {
std::unordered_set<std::string_view> names(const std::vector<std::string_view>& list){
  return {list.begin(),list.end()};
}
std::unordered_set<std::string_view> readNames(){
  auto result=names(collapseRead1Names());
  for(const auto* part:{&collapseRead2Names(),&collapseRead3Names()})
    result.insert(part->begin(),part->end());
  return result;
}
}
std::string normalizeCollapseGadgetName(const std::string& name){
  const auto input=value::utf16CodeUnits(name);
  std::u16string separated;
  separated.reserve(input.size());
  for(std::size_t i=0;i<input.size();++i){
    const auto unit=input[i];
    separated+=unit==u'-'?u'_':unit;
    if(i+1<input.size()&&unit>=u'a'&&unit<=u'z'&&
        input[i+1]>=u'A'&&input[i+1]<=u'Z')separated+=u'_';
  }
  UErrorCode error=U_ZERO_ERROR;
  const int32_t needed=u_strToLower(nullptr,0,
    reinterpret_cast<const UChar*>(separated.data()),
    static_cast<int32_t>(separated.size()),"",&error);
  if(error!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(error))
    throw std::runtime_error("MCP tool-name lowercase failed");
  std::u16string lower(static_cast<std::size_t>(needed),u'\0');
  error=U_ZERO_ERROR;
  const int32_t produced=u_strToLower(reinterpret_cast<UChar*>(lower.data()),needed,
    reinterpret_cast<const UChar*>(separated.data()),
    static_cast<int32_t>(separated.size()),"",&error);
  if(U_FAILURE(error))throw std::runtime_error("MCP tool-name lowercase failed");
  lower.resize(static_cast<std::size_t>(produced));
  return value::fromUtf16(lower);
}
Json classifyMcpGadgetForCollapse(const std::string&,const std::string& gadgetName){
  static const auto search=names(collapseSearchNames());
  static const auto read=readNames();
  const auto normalized=normalizeCollapseGadgetName(gadgetName);
  return {{"isSearch",search.contains(normalized)},{"isRead",read.contains(normalized)}};
}
} // namespace batbox::mcp
