# Plugin Host: JUCE AU + VST3 test bench (ADR 0004).
juce_add_plugin(ResilioVersio
    COMPANY_NAME "Resilio"
    PLUGIN_MANUFACTURER_CODE Rslo
    PLUGIN_CODE RsVs
    PRODUCT_NAME "Resilio Versio"
    FORMATS AU VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE   # MIDI notes trigger Kick (ADR 0005)
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(ResilioVersio PRIVATE PluginProcessor.cpp)

target_compile_definitions(ResilioVersio PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ResilioVersio
    PRIVATE rv_core juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
