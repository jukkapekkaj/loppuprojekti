cmake_minimum_required(VERSION 3.25)
project(loppuprojekti)

set(CMAKE_CXX_STANDARD 11)
include_directories(/usr/local/include/)

add_executable(loppuprojekti main.cpp serialib.cpp serialib.h)

target_link_directories(loppuprojekti PRIVATE /usr/local/lib64)

target_link_libraries(loppuprojekti paho-mqttpp3)
target_link_libraries(loppuprojekti paho-mqtt3c)

target_link_libraries(loppuprojekti pthread)

add_compile_options(-Wall -Wextra -Wpedantic -g)




add_custom_command(
    TARGET loppuprojekti 
    POST_BUILD
    COMMAND doxygen ../Doxyfile)




