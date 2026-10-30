cmake_minimum_required(VERSION 3.21)
project(pistachio-fm VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Qml Quick Test)
qt_standard_project_setup(REQUIRES 6.9)

# The backend (C++) and the UI (QML) as one static QML module, linked by the app, the tests and
# the screenshot tool. The QML files sit at the module's root (aliases), next to its qmldir, so
# they see each other and the singletons without imports.
set(PFM_QML
    Main.qml Theme.qml Icons.qml Icon.qml Shadow.qml
    IconButton.qml PathBar.qml SearchField.qml
    Sidebar.qml PlaceButton.qml SectionLabel.qml
    FileView.qml FileTile.qml FileRow.qml KindBadge.qml ThinScrollBar.qml
    StatusBar.qml FileMenu.qml FolderDrop.qml MenuEntry.qml
    ConfirmDialog.qml NameDialog.qml PropertiesDialog.qml DialogButton.qml
)
list(TRANSFORM PFM_QML PREPEND qml/ OUTPUT_VARIABLE PFM_QML_FILES)
foreach(file IN LISTS PFM_QML)
    set_source_files_properties(qml/${file} PROPERTIES QT_RESOURCE_ALIAS ${file})
endforeach()
set_source_files_properties(qml/Theme.qml qml/Icons.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(pfm STATIC)
qt_add_qml_module(pfm
    URI Pistachio.Files
    VERSION 1.0
    SOURCES
        src/Format.h src/Format.cpp
        src/FolderModel.h src/FolderModel.cpp
        src/FileOps.h src/FileOps.cpp
        src/Places.h src/Places.cpp
        src/Transfer.h src/Transfer.cpp
    QML_FILES ${PFM_QML_FILES}
)
target_include_directories(pfm PUBLIC src)
target_link_libraries(pfm PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)

qt_add_executable(pistachio-fm src/main.cpp)
target_compile_definitions(pistachio-fm PRIVATE PFM_VERSION="${PROJECT_VERSION}")
target_link_libraries(pistachio-fm PRIVATE pfm pfmplugin)

install(TARGETS pistachio-fm RUNTIME DESTINATION bin)
# The desktop entry runs the app by its installed path, worked out at install time so that
# `cmake --install --prefix` counts: launchers don't search ~/.local/bin (the shell's PATH lacks it).
install(CODE "set(pfm_entry_in \"${CMAKE_CURRENT_SOURCE_DIR}/data/pistachio-fm.desktop\")
set(pfm_entry_out \"${CMAKE_CURRENT_BINARY_DIR}/pistachio-fm.desktop\")")
install(CODE [[
    file(READ "${pfm_entry_in}" entry)
    string(REGEX REPLACE "\n(Try)?Exec=pistachio-fm" "\n\\1Exec=${CMAKE_INSTALL_PREFIX}/bin/pistachio-fm" entry "${entry}")
    file(WRITE "${pfm_entry_out}" "${entry}")
]])
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pistachio-fm.desktop DESTINATION share/applications)

# tests: the backend without a window, and the screenshot tool (pfm-shots OUTDIR)
enable_testing()

qt_add_executable(tst_backend tests/tst_backend.cpp)
target_link_libraries(tst_backend PRIVATE pfm Qt6::Test)
add_test(NAME backend COMMAND tst_backend)
set_tests_properties(backend PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(pfm-shots tests/shots.cpp)
target_link_libraries(pfm-shots PRIVATE pfm pfmplugin Qt6::Test)
