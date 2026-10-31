add_library(batbox_cron src/cron/CronParse.cpp src/cron/CronCalendar.cpp)
target_sources(batbox_cron PRIVATE src/cron/CronRead.cpp src/cron/CronWrite.cpp
  src/cron/CronIndigenousFs.cpp src/cron/CronJitter.cpp)
target_link_libraries(batbox_cron PUBLIC batbox_core batbox_event_loop batbox_bootstrap_state)
target_compile_options(batbox_cron PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_cron PRIVATE src/cron/CronSchedulerCheck.cpp src/cron/CronSchedulerLifecycle.cpp
  src/cron/CronSchedulerIndigenous.cpp src/cron/CronMissed.cpp src/cron/CronLock.cpp
  src/cron/CronLockIndigenous.cpp src/cron/CronHuman.cpp src/cron/CronHumanIndigenous.cpp)
target_link_libraries(batbox_cron PUBLIC batbox_settings batbox_cleanup)
target_sources(batbox_cron PRIVATE
  src/cron/config/CronConfiguration.cpp
  src/cron/config/CronConfigurationIndigenous.cpp
)
