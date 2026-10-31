#include "glob/DiscoveryMatcherBinding.hpp"
#include "instructions/project/ProjectDiscovery.hpp"
#include <cassert>

int main(){
  batbox::instructions::project::DiscoveryServices services;
  batbox::glob::bindDiscoveryMatchers(services);
  assert(services.matchExclude("/project/.config/RULES.md",
    {"/project/**/RULES.md"}));
  assert(services.matchExclude("/project/.config/RULES.md",
    {"/project/**/{RULES,GUIDE}.md"}));
  assert(!services.matchExclude("/project/README.md",{"/project/**/RULES.md"}));
  assert(services.matchRuleIgnore("src/nested/file.ts",{"src/**"}));
  assert(services.matchRuleIgnore("src/nested/file.ts",{"src/**","!src/file.ts"}));
  assert(!services.matchRuleIgnore("src/file.ts",{"src/*.ts","!src/file.ts"}));
}
