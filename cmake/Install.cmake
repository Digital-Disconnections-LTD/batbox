# Installed layout for package managers: the executable in bin/ and the
# project, dependency and documentation records in share/doc/batbox. The
# executable reads no data files from its own prefix, so nothing else is needed
# at runtime. The notice selection matches gadgets/package_notices.py.
include(GNUInstallDirs)
install(TARGETS batbox RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime)
install(FILES LICENSE THIRD_PARTY_NOTICES.md README.md third_party/DEPENDENCIES.md
  DESTINATION "${CMAKE_INSTALL_DOCDIR}" COMPONENT Runtime)
file(GLOB batbox_install_docs "${PROJECT_SOURCE_DIR}/docs/*.md")
install(FILES ${batbox_install_docs}
  DESTINATION "${CMAKE_INSTALL_DOCDIR}/docs" COMPONENT Runtime)
foreach(batbox_notice_root IN ITEMS src third_party)
  file(GLOB_RECURSE batbox_notice_candidates LIST_DIRECTORIES false
    RELATIVE "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/${batbox_notice_root}/*")
  foreach(batbox_notice IN LISTS batbox_notice_candidates)
    get_filename_component(batbox_notice_name "${batbox_notice}" NAME)
    string(TOLOWER "${batbox_notice_name}" batbox_notice_name)
    if(batbox_notice_name MATCHES "^(license|notice|copying|unicode-license)"
        OR batbox_notice MATCHES "(^|/)(licenses|notices)/")
      get_filename_component(batbox_notice_dir "${batbox_notice}" DIRECTORY)
      install(FILES "${batbox_notice}"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}/${batbox_notice_dir}" COMPONENT Runtime)
    endif()
  endforeach()
endforeach()
