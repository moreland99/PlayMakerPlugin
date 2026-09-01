juce_add_plugin(PlaymakersEQ
    COMPANY_NAME "PLAYMAKERS"
    PLUGIN_MANUFACTURER_CODE Plmk
    PLUGIN_CODE Peq1
    FORMATS AU VST3
    PRODUCT_NAME "PLAYMAKERS EQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES "Fx" "EQ"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
)

juce_add_binary_data(PlaymakersEQAssets
    SOURCES
        ${CMAKE_SOURCE_DIR}/Assets/Fonts/PPMonumentWide-Black.otf
        ${CMAKE_SOURCE_DIR}/Assets/Fonts/PPMonumentWide-Regular.otf
        ${CMAKE_SOURCE_DIR}/Assets/Fonts/PPMonumentExtended-Black.otf
        ${CMAKE_SOURCE_DIR}/Assets/Fonts/Helvetica.ttf
        ${CMAKE_SOURCE_DIR}/Assets/Fonts/Helvetica-Bold.ttf
        ${CMAKE_SOURCE_DIR}/Assets/Logos/PMlogo1.svg
        ${CMAKE_SOURCE_DIR}/Assets/Logos/PMlogo1-mark.svg
        ${CMAKE_SOURCE_DIR}/Assets/Logos/mark-black.png
        ${CMAKE_SOURCE_DIR}/Assets/Logos/mark-white.png
        ${CMAKE_SOURCE_DIR}/Assets/Logos/monogram-m.png
        ${CMAKE_SOURCE_DIR}/Assets/Logos/hand-mark.png
        ${CMAKE_SOURCE_DIR}/Assets/Logos/mark-on-orange.png
)

target_sources(PlaymakersEQ
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
        Params.cpp
        SpectrumAnalyzer.cpp
        BandList.cpp
        PresetState.cpp
        PresetManager.cpp
        PresetBrowser.cpp
)

target_compile_definitions(PlaymakersEQ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(PlaymakersEQ
    PRIVATE
        PlaymakersEQAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
