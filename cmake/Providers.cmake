# Bundled provider catalog (generated from models.dev by
# gadgets/providers_catalog.py), local model-server discovery and the shared
# steps that turn a catalog entry into a saved endpoint.
add_library(batbox_providers STATIC
  src/providers/Catalog.cpp src/providers/Catalog.generated.cpp
  src/providers/LocalDiscovery.cpp src/providers/Onboarding.cpp)
target_link_libraries(batbox_providers PUBLIC batbox_application_config batbox_mcp_runtime
  Threads::Threads)
target_compile_options(batbox_providers PRIVATE -Wall -Wextra -Wpedantic)
