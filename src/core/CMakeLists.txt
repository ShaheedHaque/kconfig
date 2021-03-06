
find_package(Qt5Core ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)

set(libkconfigcore_SRCS
   kconfig.cpp
   kconfigbase.cpp
   kconfigdata.cpp
   kconfiggroup.cpp
   kconfigbackend.cpp
   kconfigini.cpp
   kdesktopfile.cpp
   ksharedconfig.cpp
   kcoreconfigskeleton.cpp
   kauthorized.cpp
   kemailsettings.cpp
)

add_library(KF5ConfigCore ${libkconfigcore_SRCS})
generate_export_header(KF5ConfigCore BASE_NAME KConfigCore)
add_library(KF5::ConfigCore ALIAS KF5ConfigCore)

target_compile_definitions(KF5ConfigCore
    PRIVATE
        KCONF_UPDATE_INSTALL_LOCATION="${KDE_INSTALL_FULL_LIBEXECDIR_KF5}/$<TARGET_FILE_NAME:KF5::kconf_update>"
)

target_include_directories(KF5ConfigCore INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF5}/KConfigCore>")

target_link_libraries(KF5ConfigCore PUBLIC Qt5::Core)
if(WIN32)
    target_link_libraries(KF5ConfigCore PRIVATE ${KDEWIN_LIBRARIES})
endif()

set_target_properties(KF5ConfigCore PROPERTIES VERSION   ${KCONFIG_VERSION_STRING}
                                               SOVERSION ${KCONFIG_SOVERSION}
                                               EXPORT_NAME ConfigCore
)

ecm_generate_headers(KConfigCore_HEADERS
  HEADER_NAMES
  KAuthorized
  KConfig
  KConfigBackend
  KConfigBase
  KConfigGroup
  KDesktopFile
  KSharedConfig
  KCoreConfigSkeleton
  KEMailSettings
  ConversionCheck

  REQUIRED_HEADERS KConfigCore_HEADERS
)

find_package(PythonModuleGeneration)

if (PythonModuleGeneration_FOUND)
  ecm_generate_python_binding(
    TARGET KF5::ConfigCore
    PYTHONNAMESPACE PyKF5
    MODULENAME KConfigCore
    RULES_FILE "${CMAKE_SOURCE_DIR}/cmake/rules_PyKF5.py"
    SIP_DEPENDS
      QtCore/QtCoremod.sip
    HEADERS
      ${KConfigCore_HEADERS}
  )
endif()

install(TARGETS KF5ConfigCore EXPORT KF5ConfigTargets ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/kconfigcore_export.h
  ${KConfigCore_HEADERS}
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/KConfigCore COMPONENT Devel
)

include(ECMGeneratePriFile)
ecm_generate_pri_file(BASE_NAME KConfigCore LIB_NAME KF5ConfigCore DEPS "core" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KDE_INSTALL_INCLUDEDIR_KF5}/KConfigCore)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
