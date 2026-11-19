# Application logic: state machine, input, services, UI. Portable (firmware + host).
add_library(app STATIC app.cpp)
target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(app PUBLIC game devices hal PRIVATE cf_warnings)
