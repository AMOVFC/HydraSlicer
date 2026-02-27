add_library(hydra STATIC
    HydraSettings.cpp
    HydraMoonrakerClient.cpp
    HydraLocalStore.cpp
    HydraPresetBundle.cpp
    HydraSyncManager.cpp
    HydraPlatePlan.cpp
    HydraGitSync.cpp
)

target_include_directories(hydra PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hydra PUBLIC libcurl OpenSSL::Crypto)
