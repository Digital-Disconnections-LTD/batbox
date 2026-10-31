#include "glob/DiscoveryMatcherBinding.hpp"
#include "glob/Ignore.hpp"
#include "glob/Picomatch.hpp"
#include "instructions/project/ProjectDiscovery.hpp"

namespace batbox::glob {
void bindDiscoveryMatchers(instructions::project::DiscoveryServices& services){
  services.matchExclude=[](const std::string& path,
    const std::vector<std::string>& patterns){
    return isMatch(path,patterns);
  };
  services.matchRuleIgnore=[](const std::string& relativePath,
    const std::vector<std::string>& patterns){
    return ignores(relativePath,patterns);
  };
}
} // namespace batbox::glob
