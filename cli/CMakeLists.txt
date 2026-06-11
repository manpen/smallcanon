add_executable(devtool devtool.cpp)

target_link_libraries(devtool
        PRIVATE
        smallcanon::smallcanon
)
