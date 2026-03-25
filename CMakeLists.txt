cmake_minimum_required(VERSION 3.21)
project(DecenzaPocket VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VERSION_CODE_FILE "${CMAKE_SOURCE_DIR}/versioncode.txt")
set(VERSION_HEADER "${CMAKE_BINARY_DIR}/version.h")

file(READ "${VERSION_CODE_FILE}" CURRENT_VERSION_CODE)
string(STRIP "${CURRENT_VERSION_CODE}" CURRENT_VERSION_CODE)

configure_file(
    "${CMAKE_SOURCE_DIR}/src/version.h.in"
    "${VERSION_HEADER}"
    @ONLY
)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core Gui Qml Quick QuickControls2 Network WebSockets Multimedia
)

qt_standard_project_setup(REQUIRES 6.8)

set(SOURCES
    src/main.cpp
    src/core/settings.h
    src/core/settings.cpp
    src/network/discovery.h
    src/network/discovery.cpp
    src/network/decenzaclient.h
    src/network/decenzaclient.cpp
    src/core/notificationmanager.h
    src/core/notificationmanager.cpp
    src/network/relayclient.h
    src/network/relayclient.cpp
    src/network/connectionmanager.h
    src/network/connectionmanager.cpp
)

qt_add_executable(DecenzaPocket ${SOURCES})

qt_add_resources(DecenzaPocket "sounds"
    PREFIX "/"
    FILES
        resources/sounds/speech.wav
)

qt_add_resources(DecenzaPocket "icons"
    PREFIX "/"
    FILES
        resources/icons/back.svg
        resources/icons/settings.svg
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(DecenzaPocket
    URI DecenzaPocket
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/StatusCard.qml
        qml/PowerButton.qml
        qml/ConnectionIndicator.qml
        qml/PairingPage.qml
        qml/SettingsPage.qml
        qml/BottomBar.qml
)

target_include_directories(DecenzaPocket PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(DecenzaPocket PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
    Qt6::Network Qt6::WebSockets Qt6::Multimedia
)

set_target_properties(DecenzaPocket PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
    QT_ANDROID_APP_LIB_NAME "DecenzaPocket"
)

if(ANDROID)
    set(ANDROID_VERSION_NAME "${CMAKE_PROJECT_VERSION}")
    set(ANDROID_VERSION_CODE "${CURRENT_VERSION_CODE}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/android/AndroidManifest.xml.in"
        "${CMAKE_SOURCE_DIR}/android/AndroidManifest.xml"
        @ONLY
    )
endif()

if(IOS)
    set_target_properties(DecenzaPocket PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.kulitorum.decenzapocket"
        MACOSX_BUNDLE_BUNDLE_VERSION "${CURRENT_VERSION_CODE}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${CMAKE_PROJECT_VERSION}"
    )
endif()
