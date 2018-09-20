cmake_minimum_required(VERSION 3.12)
project(MindMap CXX)

find_package(Qt5 5.11 REQUIRED COMPONENTS Core Widgets Xml)

add_executable(MindMap main.cpp)
target_link_libraries(MindMap PRIVATE Qt5::Core Qt5::Widgets Qt5::Xml)
target_compile_features(MindMap PRIVATE cxx_std_17)
