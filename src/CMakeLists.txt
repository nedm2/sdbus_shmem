find_package(PkgConfig REQUIRED)
pkg_check_modules(Systemd REQUIRED IMPORTED_TARGET libsystemd)

add_executable(SdbusService
    bus_service.c
)

target_link_libraries(SdbusService
    PkgConfig::Systemd
)

add_executable(SdbusClient
    bus_client.c
)

target_link_libraries(SdbusClient
    PkgConfig::Systemd
)
