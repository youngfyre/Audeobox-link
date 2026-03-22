#pragma once

#include "PluginProcessor.h"

class AueoboxAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Button::Listener,
                                         private juce::Timer
{
public:
    explicit AueoboxAudioProcessorEditor(AueoboxAudioProcessor&);
    ~AueoboxAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void buttonClicked(juce::Button*) override;
    void timerCallback() override;
    void refreshArtwork();

    AueoboxAudioProcessor& audioProcessor;
    juce::TextButton helpButton { "Help" };
    juce::TextButton reconnectButton { "Reconnect" };
    juce::Image artworkImage;
    juce::String artworkPath;
    float pulsePhase = 0.0f;
};
