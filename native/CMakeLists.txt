cmake_minimum_required(VERSION 3.21)

project(fcitx5-meld VERSION 0.5.1 LANGUAGES CXX)

include(GNUInstallDirs)
find_package(Fcitx5Core 5.1.13 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Rime REQUIRED IMPORTED_TARGET "rime>=1.7.0")

include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

set(RIME_DATA_DIR "/usr/share/rime-data" CACHE PATH "Rime shared data directory")
add_compile_definitions(
    MELD_VERSION="${PROJECT_VERSION}"
    RIME_DATA_DIR="${RIME_DATA_DIR}"
)

add_subdirectory(addon)

install(
    FILES
        data/meld_bopomofo.schema.yaml
        data/meld_bopomofo_relaxed.schema.yaml
    DESTINATION "${RIME_DATA_DIR}"
)
install(
    FILES ../rime/smart_english.tsv
    DESTINATION "${CMAKE_INSTALL_DATADIR}/meld-input"
)
install(
    FILES data/fcitx-meld.svg
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
)
