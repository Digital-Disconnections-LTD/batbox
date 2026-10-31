#include "bootstrap/BootstrapState.hpp"
#include "runtime/Platform.hpp"

namespace batbox::bootstrap {
BootstrapInitialServices makeIndigenousBootstrapInitialServices(
  bool developerUser){
  return {cwd::indigenousInitialServices(),[]{
    return static_cast<double>(platform::nowMillis());
  },[]{return platform::randomUUID();},developerUser};
}
}
