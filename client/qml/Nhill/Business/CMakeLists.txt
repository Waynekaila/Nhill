qt_add_library(nhill_business STATIC)
set_source_files_properties(Nav.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
file(GLOB app_pages RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS pages/*.qml dialogs/*.qml onboarding/*.qml)
qt_add_qml_module(nhill_business
    URI Nhill.Business
    VERSION 1.0
    QML_FILES Main.qml Nav.qml Shell.qml NotificationsButton.qml GlobalSearch.qml Tour.qml ${app_pages}
)
target_link_libraries(nhill_business PRIVATE Qt6::Quick Qt6::QuickControls2)
