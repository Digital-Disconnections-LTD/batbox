#pragma once
#include "interactive/MessageGrouping.hpp"
#include "interactive/MessageLookups.hpp"
#include "gadgets/Gadget.hpp"
#include <map>

namespace batbox::interactive::message_list_view {
std::string gadgetUseSearchText(const Json& input);
std::string gadgetResultSearchText(const Json& result);
std::string computeSearchText(const Json& message);
std::string lowercase(const std::string& text);

// Weak ownership preserves message identity without retaining transcripts.
class SearchTextStash {
public:
  std::string renderable(const MessageRef& message);
  std::string extract(const MessageRef& message,const MessageLookups& lookups,
    const gadgets::Gadgets& gadgets);
  void clear();
private:
  using Stash=std::map<std::weak_ptr<Json>,std::string,
    std::owner_less<std::weak_ptr<Json>>>;
  Stash raw_,extracted_;
  std::optional<std::weak_ptr<Json>> rawSweep_,extractedSweep_;
  static void collect(Stash& stash,std::optional<std::weak_ptr<Json>>& cursor);
};
} // namespace batbox::interactive::message_list_view
