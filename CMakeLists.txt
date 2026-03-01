cmake_minimum_required(VERSION 3.20)
project(HFT1 VERSION 0.1.0 LANGUAGES C CXX)

add_executable(HFT1 
    src/book.cpp
    src/ingest.cpp
    src/main.cpp
)

set_property(TARGET HFT1 PROPERTY CXX_STANDARD 23)
