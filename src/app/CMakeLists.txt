# SPDX-License-Identifier: GPL-2.0-or-later
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Multimedia Concurrent)

set(CMAKE_AUTOMOC ON)

qt_add_executable(akai-explorer
    main.cpp
    AppController.cpp AppController.h
    LibraryModel.cpp LibraryModel.h
    FileModel.cpp FileModel.h
    Player.cpp Player.h
    WaveformItem.cpp WaveformItem.h
    IconItem.cpp IconItem.h)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# RESOURCE_PREFIX is set explicitly so the QML lives at qrc:/AkaiExplorer/...
# in every Qt 6 version (the default prefix changed in Qt 6.5).
qt_add_qml_module(akai-explorer
    URI AkaiExplorer
    VERSION 1.0
    RESOURCE_PREFIX /
    NO_PLUGIN
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Sidebar.qml
        qml/VolumeHeader.qml
        qml/FileList.qml
        qml/PlayerBar.qml
        qml/EmptyState.qml
        qml/PillButton.qml
        qml/IconButton.qml
        qml/Chip.qml
        qml/Toast.qml)

target_link_libraries(akai-explorer PRIVATE
    akx_core
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia Qt6::Concurrent)
target_compile_options(akai-explorer PRIVATE -Wall -Wextra)
target_compile_definitions(akai-explorer PRIVATE AKX_VERSION="${PROJECT_VERSION}")
set_target_properties(akai-explorer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

install(TARGETS akai-explorer RUNTIME DESTINATION bin)
