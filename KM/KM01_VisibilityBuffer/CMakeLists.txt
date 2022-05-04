cmake_minimum_required (VERSION 3.6)

project(KM01_VisibilityBuffer CXX)

set(SOURCE
    src/KM01_VisibilityBuffer.cpp
    ../../Tutorials/Common/src/TexturedCube.cpp
)

set(INCLUDE
    src/KM01_VisibilityBuffer.hpp
    ../../Tutorials/Common/src/TexturedCube.hpp
)

file(GLOB SHADER_FILES assets/*.vsh assets/*.psh)

set(SHADERS
    ${SHADER_FILES}
)

set(ASSETS
    assets/DGLogo.png
)

add_sample_app("KM01_VisibilityBuffer" "KM" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(KM01_VisibilityBuffer PRIVATE Diligent-TextureLoader)