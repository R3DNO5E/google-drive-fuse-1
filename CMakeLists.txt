cmake_minimum_required(VERSION 3.10)

project(gdrive-test)

include_directories(include)

add_library(api_request src/api_request.cpp)
target_link_libraries(api_request curl)

add_executable(oauth2 src/oauth2.cpp)
target_link_libraries(oauth2 PUBLIC ssl crypto microhttpd json-c curl)
#add_library(gdrive src/gdrive.cpp)
#add_executable(gdrive src/gdrive.cpp)
#target_link_libraries(gdrive PUBLIC json-c api_request)
add_executable(gdrive-fuse src/gdrive-fuse.cpp)
target_include_directories(gdrive-fuse PUBLIC  /usr/include/fuse3)
target_link_libraries(gdrive-fuse PUBLIC pthread fuse3 json-c api_request)
