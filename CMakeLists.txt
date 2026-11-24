cmake_minimum_required(VERSION 3.22)
project(PatchSamplerPrototype VERSION 1.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(PatchSamplerPrototype
    COMPANY_NAME "Noise Mu6ix Studio"
    PRODUCT_NAME "PatchSampler Prototype"
    VERSION "1.0.1"
)

target_sources(PatchSamplerPrototype
    PRIVATE
        Main.cpp
        MainComponent.cpp
)

target_compile_definitions(PatchSamplerPrototype
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PatchSamplerPrototype,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PatchSamplerPrototype,JUCE_VERSION>"
)

target_link_libraries(PatchSamplerPrototype
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
