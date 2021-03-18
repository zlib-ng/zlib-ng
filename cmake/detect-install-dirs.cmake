# detect-install-dirs.cmake -- Detect install directory parameters
# Copyright (C) 2021 Hans Kristian Rosbach
# Licensed under the Zlib license, see LICENSE.md for details

# Determine installation directory for executables
if (DEFINED BIN_INSTALL_DIR)
    set(BIN_INSTALL_DIR "${BIN_INSTALL_DIR}" CACHE PATH "Installation directory for executables" FORCE)
elseif (DEFINED INSTALL_BIN_DIR)
    set(BIN_INSTALL_DIR "${INSTALL_BIN_DIR}" CACHE PATH "Installation directory for executables" FORCE)
elseif (DEFINED CMAKE_INSTALL_FULL_BINDIR)
    set(BIN_INSTALL_DIR "${CMAKE_INSTALL_FULL_BINDIR}" CACHE PATH "Installation directory for executables" FORCE)
elseif (DEFINED CMAKE_INSTALL_BINDIR)
    set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}" CACHE PATH "Installation directory for executables" FORCE)
else()
    set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Installation directory for executables")
endif()

# Determine installation directory for libraries
if (DEFINED LIB_INSTALL_DIR)
    set(LIB_INSTALL_DIR "${LIB_INSTALL_DIR}" CACHE PATH "Installation directory for libraries" FORCE)
elseif (DEFINED INSTALL_LIB_DIR)
    set(LIB_INSTALL_DIR "${INSTALL_LIB_DIR}" CACHE PATH "Installation directory for libraries" FORCE)
elseif (DEFINED CMAKE_INSTALL_FULL_LIBDIR)
    set(LIB_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBDIR}" CACHE PATH "Installation directory for libraries" FORCE)
elseif (DEFINED CMAKE_INSTALL_LIBDIR)
    set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}" CACHE PATH "Installation directory for libraries" FORCE)
else()
    set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Installation directory for libraries")
endif()

# Determine installation directory for include files
if (DEFINED INC_INSTALL_DIR)
    set(INC_INSTALL_DIR "${INC_INSTALL_DIR}" CACHE PATH "Installation directory for headers" FORCE)
elseif (DEFINED INSTALL_INC_DIR)
    set(INC_INSTALL_DIR "${INSTALL_INC_DIR}" CACHE PATH "Installation directory for headers" FORCE)
elseif (DEFINED CMAKE_INSTALL_FULL_INCLUDEDIR)
    set(INC_INSTALL_DIR "${CMAKE_INSTALL_FULL_INCLUDEDIR}" CACHE PATH "Installation directory for headers" FORCE)
elseif (DEFINED CMAKE_INSTALL_INCLUDEDIR)
    set(INC_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}" CACHE PATH "Installation directory for headers" FORCE)
else()
    set(INC_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Installation directory for headers")
endif()

# Determine installation directory for pkgconfig files
if (DEFINED PKGCONFIG_INSTALL_DIR)
    set(PKGCONFIG_INSTALL_DIR "${PKGCONFIG_INSTALL_DIR}" CACHE PATH "Installation directory for pkgconfig (.pc) files" FORCE)
elseif (DEFINED INSTALL_PKGCONFIG_DIR)
    set(PKGCONFIG_INSTALL_DIR "${INSTALL_PKGCONFIG_DIR}" CACHE PATH "Installation directory for pkgconfig (.pc) files" FORCE)
elseif (DEFINED CMAKE_INSTALL_FULL_PKGCONFIGDIR)
    set(PKGCONFIG_INSTALL_DIR "${CMAKE_INSTALL_FULL_PKGCONFIGDIR}" CACHE PATH "Installation directory for pkgconfig (.pc) files" FORCE)
elseif (DEFINED CMAKE_INSTALL_PKGCONFIGDIR)
    set(PKGCONFIG_INSTALL_DIR "${LIB_INSTALL_DIR}/${CMAKE_INSTALL_PKGCONFIGDIR}" CACHE PATH "Installation directory for pkgconfig (.pc) files" FORCE)
else()
    set(PKGCONFIG_INSTALL_DIR "${LIB_INSTALL_DIR}/pkgconfig" CACHE PATH "Installation directory for pkgconfig (.pc) files")
endif()
