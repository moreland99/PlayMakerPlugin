add_executable(DspSmokeTest DspSmokeTest.cpp ../Source/Params.cpp)

target_link_libraries(DspSmokeTest
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_processors
        juce::juce_recommended_config_flags
)

target_compile_definitions(DspSmokeTest
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
)
