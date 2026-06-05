kcoreaddons_add_plugin(latte_packagestructure_indicator SOURCES indicatorpackage.cpp INSTALL_NAMESPACE kpackage/packagestructure)
set_target_properties(latte_packagestructure_indicator PROPERTIES OUTPUT_NAME latte_indicator)

target_link_libraries(latte_packagestructure_indicator
   KF6::ConfigQml  # KF5::Declarative -> KF6::ConfigQml; Phase 3 for QML engine setup
   KF6::I18n
   KF6::Package
)

# KDE_INSTALL_KSERVICETYPES5DIR is removed in Plasma 6; plugin registration is
# handled by kcoreaddons_add_plugin(INSTALL_NAMESPACE) above. TODO Phase 3: verify
# latte-indicator.desktop is not needed, or migrate to JSON metadata.
