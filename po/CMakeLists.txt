# Task 48: install pipeline for gettext .po -> .mo catalogs.
#
# ki18n_install() is defined by KF6I18nMacros.cmake, which KF6I18nConfig.cmake
# includes automatically -- the top-level CMakeLists.txt's
# find_package(KF6I18n REQUIRED) is enough, no extra include()/find_package()
# is needed here. It globs ${podir}/<lang>/*.po recursively (see
# KF6I18nMacros.cmake's KI18N_INSTALL -> build-pofiles.cmake), compiles each
# into share/locale/<lang>/LC_MESSAGES/<name>.mo via msgfmt, and installs that
# tree at ${CMAKE_INSTALL_PREFIX}/share/locale -- the exact path KI18n's
# KCatalog machinery searches at runtime (via QStandardPaths::
# GenericDataLocation + "locale/<lang>/LC_MESSAGES/<domain>.mo") once
# KLocalizedString::setApplicationDomain("kypost") has run (see
# app/main.cpp).
#
# Deliberately NOT ecm_create_qm_loader/ecm_install_po_files_as_qm
# (ECMPoQmTools.cmake): that ECM module compiles .po -> .ts -> .qm and
# generates a QTranslator-based C++ loader (see
# /usr/share/ECM/modules/ECMQmLoader.cpp.in -- it literally constructs a
# QTranslator and calls QCoreApplication::installTranslator()). That is
# exactly the parallel QTranslator/tr()-based scheme
# phase8-global-constraints.md item 4 forbids; this project uses KI18n's
# native gettext .mo catalogs throughout. No C++ loader shim is needed for
# those: once a .mo is installed at the standard path and the domain is set,
# KI18n's KCatalog finds and loads it automatically the first time i18n()/
# i18nc() is called for that domain.
#
# Global constraint 5: po/ ships zero .po files this phase (see .gitkeep).
# ki18n_install() handles an empty tree cleanly -- KI18N_INSTALL only checks
# that the podir itself exists (it does; this file lives in it), the
# subsequent recursive glob for *.po simply returns an empty list, the
# generated custom target's msgfmt loop body never executes, and it still
# installs an empty share/locale/ directory. Verified: `cmake --build`
# against the current empty po/ tree produces no warning or error.
ki18n_install(${CMAKE_CURRENT_SOURCE_DIR})
