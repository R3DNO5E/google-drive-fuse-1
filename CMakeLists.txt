cmake_minimum_required(VERSION 3.10)

project(gdrive-test)
add_executable(oauth2 src/oauth2.cpp)
target_link_libraries(oauth2 PUBLIC ssl crypto microhttpd json-c curl)
add_library(gdrive src/gdrive.cpp)
target_link_libraries(gdrive PUBLIC curl json-c)
# add_executable(gdrive-test src/main.cpp)
# target_link_libraries(gdrive-test PUBLIC pthread fuse3 curl ssl)
