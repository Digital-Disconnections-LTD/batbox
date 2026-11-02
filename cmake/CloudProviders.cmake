# Amazon Bedrock and Google Vertex AI adapters for Anthropic models.
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
add_library(batbox_cloud_providers
  src/providers/cloud/Clock.cpp src/providers/cloud/Command.cpp src/providers/cloud/Crypto.cpp
  src/providers/cloud/ErrorShape.cpp src/providers/cloud/HttpExchange.cpp
  src/providers/aws/SigV4.cpp src/providers/aws/Credentials.cpp src/providers/aws/EventStream.cpp
  src/providers/aws/BedrockClient.cpp
  src/providers/gcp/GoogleAuth.cpp src/providers/gcp/VertexClient.cpp)
target_include_directories(batbox_cloud_providers PUBLIC src)
target_link_libraries(batbox_cloud_providers PUBLIC batbox_transport batbox_core CURL::libcurl OpenSSL::Crypto)
target_compile_options(batbox_cloud_providers PRIVATE -Wall -Wextra -Wpedantic)
