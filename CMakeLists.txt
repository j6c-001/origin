cmake_minimum_required(VERSION 3.14)
project(CharacterSystem)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

# --- ImGui Installation ---
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.4
)
FetchContent_MakeAvailable(imgui)

# We need to manually define the imgui library because the repo doesn't provide a standard CMakeLists.txt
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
)

# --- nlohmann/json Installation ---
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# --- GLM Installation ---
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# --- Assimp ---
find_package(assimp QUIET)
if(NOT assimp_FOUND)
    FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG        v5.3.1
    )
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(assimp)
endif()

# --- GLEW Installation (Using Pre-generated Source) ---
if(NOT EMSCRIPTEN)
    FetchContent_Declare(
        glew
        URL https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip
    )
    FetchContent_GetProperties(glew)
    if(NOT glew_POPULATED)
        FetchContent_Populate(glew)
        add_library(glew STATIC ${glew_SOURCE_DIR}/src/glew.c)
        target_include_directories(glew PUBLIC ${glew_SOURCE_DIR}/include)
        target_link_libraries(glew PUBLIC OpenGL::GL)
        target_compile_definitions(glew PUBLIC GLEW_STATIC GLEW_NO_GLU)
    endif()
endif()

# --- System Dependencies ---
if(NOT EMSCRIPTEN)
    find_package(glfw3 REQUIRED)
    find_package(OpenGL REQUIRED)
endif()

set(COMMON_SRCS 
    FBXStateMachine.cpp 
    FBXStateMachine.h 
    AnimationMixer.h 
    CharacterPhysics.h 
    SkinnedRenderer.h 
    AssetBaking.h
)

# Main Executable (origin)
if(NOT EMSCRIPTEN)
    add_executable(origin main.cpp ${COMMON_SRCS})
    target_include_directories(origin PRIVATE ${assimp_SOURCE_DIR}/contrib/stb)
    target_link_libraries(origin 
        PRIVATE 
            imgui 
            assimp::assimp 
            glm::glm 
            nlohmann_json::nlohmann_json
            glfw
            glew
            OpenGL::GL
    )
endif()

# Runtime Executable
add_executable(runtime_player main_runtime.cpp ${COMMON_SRCS})
target_include_directories(runtime_player PRIVATE ${assimp_SOURCE_DIR}/contrib/stb)
if(EMSCRIPTEN)
    target_link_libraries(runtime_player 
        PRIVATE
            assimp::assimp 
            glm::glm 
            nlohmann_json::nlohmann_json
    )
    target_link_options(runtime_player PRIVATE
        "-sUSE_GLFW=3"
        "-sUSE_WEBGL2=1"
        "-sFULL_ES3=1"
        "-sWASM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXPORTED_FUNCTIONS=['_main','_setState','_shoot']"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "-sEXCEPTION_CATCHING_ALLOWED=['assimp']"
        "--preload-file" "${CMAKE_SOURCE_DIR}/assets@/assets"
    )
    # Assets preloading will be handled by the user or a separate script
    set_target_properties(runtime_player PROPERTIES SUFFIX ".html")
else()
    target_link_libraries(runtime_player 
        PRIVATE
            assimp::assimp 
            glm::glm 
            nlohmann_json::nlohmann_json
            glfw 
            glew
            OpenGL::GL
    )
endif()

# Editor Executable
if(NOT EMSCRIPTEN)
    add_executable(editor main_editor.cpp ${COMMON_SRCS})
    target_include_directories(editor PRIVATE ${assimp_SOURCE_DIR}/contrib/stb)
    target_link_libraries(editor 
        PRIVATE
            imgui 
            assimp::assimp 
            glm::glm 
            nlohmann_json::nlohmann_json
            glfw 
            glew
            OpenGL::GL
    )
endif()
