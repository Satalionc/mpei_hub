cmake_minimum_required(VERSION 3.16)
project(mpei_student_app VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)

include(FetchContent)

# ---------- nlohmann/json ----------
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

# ---------- standalone Asio ----------
# Crow 1.2 still uses asio::io_service / address::from_string.
# Those were removed in Asio >= 1.29, so pin to 1.28.x.
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-28-2
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(asio)
if(NOT asio_POPULATED)
    FetchContent_Populate(asio)
endif()
set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include")

# ---------- Crow (header-only, skip its CMake) ----------
FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.2.1
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(crow)
if(NOT crow_POPULATED)
    FetchContent_Populate(crow)
endif()
set(CROW_INCLUDE_DIR "${crow_SOURCE_DIR}/include")

# ---------- sources ----------
set(BACKEND_SOURCES
    backend/main.cpp
    backend/services/ruz_client.cpp
    backend/services/bars_client.cpp
    backend/services/schedule_cache.cpp
    backend/db/database.cpp
    backend/controllers/schedule_controller.cpp
    backend/controllers/auth_controller.cpp
    backend/controllers/bars_controller.cpp
)

add_executable(mpei_app ${BACKEND_SOURCES})

target_include_directories(mpei_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
    ${ASIO_INCLUDE_DIR}
    ${CROW_INCLUDE_DIR}
)

target_compile_definitions(mpei_app PRIVATE
    ASIO_STANDALONE
)

target_link_libraries(mpei_app PRIVATE
    nlohmann_json::nlohmann_json
    CURL::libcurl
    SQLite::SQLite3
    Threads::Threads
)
if(WIN32)
  target_link_libraries(mpei_app PRIVATE ws2_32 mswsock)
  # Crow / Asio on Windows
  target_compile_definitions(mpei_app PRIVATE _WIN32_WINNT=0x0A00)
endif()

# Copy frontend next to binary
add_custom_command(TARGET mpei_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/frontend
        $<TARGET_FILE_DIR:mpei_app>/frontend
    COMMENT "Copying frontend assets"
)

install(TARGETS mpei_app RUNTIME DESTINATION bin)
install(DIRECTORY frontend/ DESTINATION share/mpei_app/frontend)

message(STATUS "Asio:  ${ASIO_INCLUDE_DIR}")
message(STATUS "Crow:  ${CROW_INCLUDE_DIR}")
message(STATUS "Build: cmake --build .")
