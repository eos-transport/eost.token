cmake_minimum_required(VERSION 3.9)
project(eost_token)

find_package(eosio.cdt)

add_executable(eost_token
        include/eost.token/eost.token.hpp
        src/eost.token.cpp)
