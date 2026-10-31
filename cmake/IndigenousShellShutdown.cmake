# Include after batbox_application_shell exists and before IndigenousHost.
target_sources(batbox_application_shell PRIVATE
  src/application_shell/Shutdown.cpp)
