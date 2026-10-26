qt_add_library(nhill_pos STATIC)
set_source_files_properties(PosNav.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
file(GLOB pos_pages RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS pages/*.qml)
qt_add_qml_module(nhill_pos
    URI Nhill.Pos
    VERSION 1.0
    QML_FILES PosMain.qml PosNav.qml ${pos_pages}
)
target_link_libraries(nhill_pos PRIVATE Qt6::Quick Qt6::QuickControls2)
