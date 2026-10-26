qt_add_library(nhill_console STATIC)
file(GLOB admin_pages RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS pages/*.qml)
set_source_files_properties(ConsoleNav.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(nhill_console
    URI Nhill.Console
    VERSION 1.0
    QML_FILES ConsoleMain.qml ConsoleNav.qml ConsoleLogin.qml ConsoleShell.qml ConsoleSetup.qml ${admin_pages}
)
target_link_libraries(nhill_console PRIVATE Qt6::Quick Qt6::QuickControls2)
