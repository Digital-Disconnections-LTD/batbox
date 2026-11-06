# One factory from an endpoint's provider kind to its transport::WireClient,
# shared by requests, /config discovery and the provider picker's connection test.
add_library(batbox_wire_clients STATIC src/providers/WireClients.cpp)
target_link_libraries(batbox_wire_clients PUBLIC batbox_application_config
  batbox_cloud_providers batbox_transport)
target_compile_options(batbox_wire_clients PRIVATE -Wall -Wextra -Wpedantic)
# Bundled provider catalog (generated from models.dev by
# gadgets/providers_catalog.py), local model-server discovery and the shared
# steps that turn a catalog entry into a saved endpoint.
add_library(batbox_providers STATIC
  src/providers/Catalog.cpp src/providers/Catalog.generated.cpp
  src/providers/LocalDiscovery.cpp src/providers/Onboarding.cpp
  src/providers/RequestSettings.cpp)
target_link_libraries(batbox_providers PUBLIC batbox_application_config batbox_mcp_runtime
  batbox_wire_clients Threads::Threads)
target_compile_options(batbox_providers PRIVATE -Wall -Wextra -Wpedantic)
