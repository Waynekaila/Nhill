qt_add_library(nhill_ui STATIC)
set_source_files_properties(Theme.qml Labels.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(nhill_ui
    URI Nhill.Ui
    VERSION 1.0
    QML_FILES
        Theme.qml Labels.qml Icon.qml LogoMark.qml
        FbButton.qml FbIconButton.qml FbTextField.qml FbTextArea.qml FbComboBox.qml FbSwitch.qml
        Field.qml FbCard.qml FbChip.qml FbDialog.qml FbConfirm.qml Notifier.qml EmptyState.qml Banner.qml
        SectionTitle.qml KpiTile.qml SearchField.qml MoneyField.qml DateField.qml DataTable.qml FbPage.qml
        ConnectionBadge.qml TabStrip.qml SupportCenter.qml
)
target_link_libraries(nhill_ui PRIVATE Qt6::Quick Qt6::QuickControls2)
