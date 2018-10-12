find_package(PkgConfig REQUIRED)
pkg_check_modules(Systemd REQUIRED IMPORTED_TARGET libsystemd)

find_package(Boost REQUIRED)

add_executable(SdbusService
    bus_service.cpp
)

target_link_libraries(SdbusService
    PkgConfig::Systemd
    Boost::disable_autolinking
    Boost::boost
    rt
)

add_executable(SdbusClient
    bus_client.cpp
)

target_link_libraries(SdbusClient
    PkgConfig::Systemd
    Boost::disable_autolinking
    Boost::boost
    rt
)
