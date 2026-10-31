target_sources(batbox_cron PRIVATE
  src/cron/locale/NotificationDate.cpp
  src/cron/use_scheduled/FireTime.cpp
  src/cron/use_scheduled/Flight.cpp
  src/cron/use_scheduled/FlightIndigenous.cpp
  src/cron/print/HeadlessCron.cpp
)
target_link_libraries(batbox_cron PUBLIC ICU::i18n)
add_library(batbox_cron_gadgets
  src/gadgets/cron/CronBindings.cpp
  src/gadgets/cron/CronCreate.cpp
  src/gadgets/cron/CronDelete.cpp
  src/gadgets/cron/CronList.cpp
  src/gadgets/cron/CronPoetry.cpp
  src/gadgets/cron/CronGadgetSchema.cpp
  src/gadgets/cron/CronGadgetsIndigenous.cpp
)
target_link_libraries(batbox_cron_gadgets PUBLIC batbox_cron batbox_core batbox_truncate batbox_bat_context)
target_compile_options(batbox_cron_gadgets PRIVATE -Wall -Wextra -Wpedantic)
