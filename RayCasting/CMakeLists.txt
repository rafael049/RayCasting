# CMakeList.txt : CMake project for RayCasting, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (RayCasting 
src/camera.hpp
src/ds.hpp
src/line.hpp
src/media.hpp
src/rayCasting.cpp
src/rendering.hpp
src/sdl.hpp
src/wall.hpp)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET RayCasting PROPERTY CXX_STANDARD 23)
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
endif()

find_package(SDL2 REQUIRED CONFIG)
target_link_libraries(RayCasting
        PRIVATE
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    )

find_package(glm REQUIRED)
target_link_libraries(RayCasting PRIVATE glm::glm)

find_package(nlohmann_json REQUIRED)
target_link_libraries(RayCasting PRIVATE nlohmann_json::nlohmann_json)

# find_package(tinyobjloader REQUIRED)
# target_link_libraries(RayCasting PRIVATE tinyobjloader::tinyobjloader)

