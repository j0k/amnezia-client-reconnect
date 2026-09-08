# amnezia-direct-proxy: a tiny standalone local proxy whose traffic is excluded
# from the VPN by the parent app. Windows-only for now (plain Winsock, no Qt).
# Console subsystem so it runs headless with args; the parent app launches it
# with a no-window flag to avoid a console popup.
add_executable(amnezia-direct-proxy main.cpp)

# OpenSSL provides the optional TLS ("HTTPS proxy") client channel. The targets are
# imported at the top level via find_package(OpenSSL) (conan) and visible here.
target_link_libraries(amnezia-direct-proxy PRIVATE ws2_32 crypt32 OpenSSL::SSL OpenSSL::Crypto)

# Build as a GUI-subsystem app (no console window ever) while keeping main() as the
# entry point. This guarantees no stray console windows when the client launches it.
if(MSVC)
    target_link_options(amnezia-direct-proxy PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

# Place the binary next to the client executable in the build tree.
set_target_properties(amnezia-direct-proxy PROPERTIES
    OUTPUT_NAME "amnezia-direct-proxy"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/client"
)

# Ship it next to AmneziaVPN.exe in the installer (same component).
include(GNUInstallDirs)
install(TARGETS amnezia-direct-proxy
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT AmneziaVPN
)
