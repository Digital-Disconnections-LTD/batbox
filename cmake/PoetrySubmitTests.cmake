foreach(section IN ITEMS submit_queue poetry_submit)
  add_executable(batbox-indigenous-${section}-probe tests/phase5/indigenous_${section}_probe.cpp)
  target_link_libraries(batbox-indigenous-${section}-probe PRIVATE batbox_poetry_submit)

endforeach()
batbox_indigenous(phase5-indigenous-submit-readiness tests/phase5/indigenous_submit_readiness.cpp
  batbox_poetry_submit)
batbox_indigenous(phase5-indigenous-poetry-submit-real tests/phase5/indigenous_poetry_submit_real.cpp
  batbox_poetry_submit)
