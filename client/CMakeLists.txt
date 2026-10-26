find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 Svg LinguistTools)
find_package(Qt6Keychain CONFIG QUIET)
qt_standard_project_setup()

set(NHILL_SERVER_URL "http://127.0.0.1:8080" CACHE STRING "Default API address compiled into the client (HTTPS in production)")
set(QML_IMPORT_PATH ${CMAKE_CURRENT_BINARY_DIR}/qml CACHE STRING "" FORCE)

# ---------------------------------------------------------------- C++ core (shared by app, admin console, tests)
qt_add_library(nhill_client_core STATIC
    src/core/apiclient.cpp src/core/apiclient.h
    src/core/session.cpp src/core/session.h
    src/core/formatter.cpp src/core/formatter.h
    src/core/appcontext.cpp src/core/appcontext.h
)
target_include_directories(nhill_client_core PUBLIC src/core)
target_link_libraries(nhill_client_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick nhill_common)
target_compile_definitions(nhill_client_core PRIVATE NHILL_DEFAULT_SERVER="${NHILL_SERVER_URL}")
if(Qt6Keychain_FOUND)
    target_link_libraries(nhill_client_core PUBLIC Qt6Keychain::Qt6Keychain)
    target_compile_definitions(nhill_client_core PUBLIC NHILL_HAVE_KEYCHAIN)
    message(STATUS "Nhill client: QtKeychain found, refresh tokens stored in the OS keychain")
else()
    message(WARNING "QtKeychain not found: sessions will not be remembered between launches (no clear-text fallback)")
endif()

# Brand assets and fonts embedded in the application
set(brand_files)
foreach(n nhill-logo nhill-logo-reverse nhill-logo-business nhill-logo-business-reverse nhill-logo-console
          nhill-logo-console-reverse nhill-logo-pos nhill-logo-pos-reverse nhill-mark
          nhill-icon-business nhill-icon-console nhill-icon-pos)
    list(APPEND brand_files ${PROJECT_SOURCE_DIR}/assets/brand/${n}.svg)
endforeach()
set(font_files
    ${PROJECT_SOURCE_DIR}/assets/fonts/IBMPlexSans-Regular.ttf
    ${PROJECT_SOURCE_DIR}/assets/fonts/IBMPlexSans-Medium.ttf
    ${PROJECT_SOURCE_DIR}/assets/fonts/IBMPlexSans-SemiBold.ttf
    ${PROJECT_SOURCE_DIR}/assets/fonts/IBMPlexSans-Bold.ttf
    ${PROJECT_SOURCE_DIR}/assets/fonts/IBMPlexMono-Regular.ttf
    ${PROJECT_SOURCE_DIR}/assets/fonts/IBMPlexMono-Medium.ttf)
foreach(f ${brand_files})
    get_filename_component(n ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS brand/${n})
endforeach()
foreach(f ${font_files})
    get_filename_component(n ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS fonts/${n})
endforeach()
qt_add_resources(nhill_client_core "nhill_assets" PREFIX "/" FILES ${brand_files} ${font_files})

# Translations (sources: English; complete French translation)
qt_add_lrelease(nhill_client_core TS_FILES i18n/nhill_fr.ts i18n/nhill_en.ts QM_FILES_OUTPUT_VARIABLE qm_files)
qt_add_resources(nhill_client_core "nhill_translations" PREFIX "/i18n" BASE ${CMAKE_CURRENT_BINARY_DIR} FILES ${qm_files})

# ---------------------------------------------------------------- QML modules
add_subdirectory(qml/Nhill/Ui)
add_subdirectory(qml/Nhill/Business)
add_subdirectory(qml/Nhill/Console)
if(NOT ANDROID)
    add_subdirectory(qml/Nhill/Pos)
endif()

# ---------------------------------------------------------------- executables
# Nhill Business — establishments and entrepreneurs (Windows, Linux, Android)
qt_add_executable(nhill-business WIN32 MACOSX_BUNDLE src/business/main.cpp ${PROJECT_SOURCE_DIR}/client/resources/nhill-business.rc)
target_link_libraries(nhill-business PRIVATE nhill_client_core nhill_uiplugin nhill_businessplugin Qt6::QuickControls2 Qt6::Svg)
set_target_properties(nhill-business PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
    QT_ANDROID_TARGET_SDK_VERSION 34
    QT_ANDROID_MIN_SDK_VERSION 24
    QT_ANDROID_VERSION_NAME ${PROJECT_VERSION}
    QT_ANDROID_VERSION_CODE 1)

# Nhill Console — creators only (Windows, Linux; never built for Android)
if(NOT ANDROID)
    qt_add_executable(nhill-console WIN32 src/console/main.cpp ${PROJECT_SOURCE_DIR}/client/resources/nhill-console.rc)
    target_link_libraries(nhill-console PRIVATE nhill_client_core nhill_uiplugin nhill_consoleplugin Qt6::QuickControls2 Qt6::Svg)
endif()

# Nhill POS — point of sale of an establishment (Windows, Linux)
if(NOT ANDROID)
    qt_add_executable(nhill-pos WIN32 src/pos/main.cpp ${PROJECT_SOURCE_DIR}/client/resources/nhill-pos.rc)
    target_link_libraries(nhill-pos PRIVATE nhill_client_core nhill_uiplugin nhill_posplugin Qt6::QuickControls2 Qt6::Svg)
endif()

include(GNUInstallDirs)
install(TARGETS nhill-business RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT business BUNDLE DESTINATION . COMPONENT business)
if(TARGET nhill-console)
    install(TARGETS nhill-console RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT console)
endif()
if(TARGET nhill-pos)
    install(TARGETS nhill-pos RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT pos)
endif()
if(UNIX AND NOT APPLE AND NOT ANDROID)
    foreach(app business console pos)
        install(FILES resources/nhill-${app}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT ${app})
        install(FILES ${PROJECT_SOURCE_DIR}/assets/icons/nhill-${app}-256.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
                RENAME nhill-${app}.png COMPONENT ${app})
        install(FILES ${PROJECT_SOURCE_DIR}/assets/brand/nhill-icon-${app}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
                RENAME nhill-${app}.svg COMPONENT ${app})
    endforeach()
endif()
