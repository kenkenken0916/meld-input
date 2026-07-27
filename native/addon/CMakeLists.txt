add_library(meld SHARED meld.cpp smart_engine.cpp composition_model.cpp)
target_compile_features(meld PRIVATE cxx_std_20)
target_link_libraries(meld PRIVATE Fcitx5::Core PkgConfig::Rime)
set_target_properties(meld PROPERTIES PREFIX "")

install(TARGETS meld DESTINATION "${FCITX_INSTALL_LIBDIR}/fcitx5")

configure_file(meld-addon.conf.in meld-addon.conf @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/meld-addon.conf"
    RENAME meld.conf
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon"
)
install(
    FILES meld.conf
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod"
)
