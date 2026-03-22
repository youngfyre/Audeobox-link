#include "PluginEditor.h"

namespace
{
void drawPlaceholderCover(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& label)
{
    juce::ColourGradient gradient(juce::Colour::fromRGB(195, 74, 64),
                                  bounds.getTopLeft(),
                                  juce::Colour::fromRGB(38, 151, 153),
                                  bounds.getBottomRight(),
                                  false);
    gradient.addColour(0.45, juce::Colour::fromRGB(26, 27, 31));
    gradient.addColour(0.72, juce::Colour::fromRGB(245, 235, 216));

    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, 8.0f);

    g.setColour(juce::Colour::fromRGBA(12, 12, 14, 150));
    g.fillRoundedRectangle(bounds.removeFromBottom(bounds.getHeight() * 0.24f), 8.0f);

    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawFittedText(label, bounds.toNearestInt(), juce::Justification::centred, 1);
}

void drawLevelMeter(juce::Graphics& g, juce::Rectangle<float> bounds, float level)
{
    const auto meterBounds = bounds.reduced(18.0f, 14.0f);
    const auto normalized = juce::jlimit(0.0f, 1.0f, std::sqrt(level));
    constexpr int barCount = 3;
    const auto gap = 6.0f;
    const auto barWidth = (meterBounds.getWidth() - gap * (barCount - 1)) / static_cast<float>(barCount);

    for (int i = 0; i < barCount; ++i)
    {
        const auto barX = meterBounds.getX() + i * (barWidth + gap);
        auto bar = juce::Rectangle<float>(barX, meterBounds.getY(), barWidth, meterBounds.getHeight());
        const auto threshold = (i + 1.0f) / static_cast<float>(barCount);
        const auto active = normalized >= threshold * 0.58f;

        g.setColour(active ? juce::Colour::fromRGB(43, 210, 78)
                           : juce::Colour::fromRGBA(255, 255, 255, 42));
        const auto fillHeight = juce::jmax(bar.getHeight() * 0.22f, bar.getHeight() * juce::jlimit(0.22f, 1.0f, normalized));
        bar.removeFromTop(bar.getHeight() - fillHeight);
        g.fillRect(bar);
    }
}
}

AueoboxAudioProcessorEditor::AueoboxAudioProcessorEditor(AueoboxAudioProcessor& processor)
    : AudioProcessorEditor(&processor), audioProcessor(processor)
{
    helpButton.addListener(this);
    helpButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    helpButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    helpButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(220, 220, 220));
    helpButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible(helpButton);

    reconnectButton.addListener(this);
    reconnectButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    reconnectButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    reconnectButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(220, 220, 220));
    reconnectButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible(reconnectButton);

    setSize(680, 420);
    refreshArtwork();
    startTimerHz(30);
}

void AueoboxAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop(102);
    auto footer = bounds.removeFromBottom(108);
    auto content = bounds.reduced(24, 28);

    g.fillAll(juce::Colour::fromRGB(9, 10, 14));

    g.setColour(juce::Colour::fromRGB(29, 31, 38));
    g.fillRect(header);
    g.fillRect(footer);

    g.setColour(juce::Colour::fromRGB(48, 51, 61));
    g.fillRect(0, header.getBottom() - 1, getWidth(), 1);
    g.fillRect(0, footer.getY(), getWidth(), 1);

    auto logoArea = juce::Rectangle<float>(34.0f, 34.0f, 26.0f, 34.0f);
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(logoArea.removeFromTop(14.0f).withTrimmedRight(8.0f), 7.0f);
    g.fillRoundedRectangle(logoArea.withTrimmedLeft(8.0f), 7.0f);

    g.setColour(juce::Colour::fromRGB(160, 164, 174));
    g.setFont(juce::Font(18.0f));
    g.drawText("Version 0.0.1", 84, 34, 160, 34, juce::Justification::centredLeft, false);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(39.0f, juce::Font::bold));
    g.drawFittedText("Audeobox Sounds", 52, 124, getWidth() - 104, 54, juce::Justification::centred, 1);

    g.setColour(juce::Colour::fromRGB(57, 214, 123));
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawFittedText("CODEX TEST BUILD 08:19", 52, 164, getWidth() - 104, 22, juce::Justification::centred, 1);

    g.setColour(juce::Colour::fromRGB(230, 231, 235));
    g.setFont(juce::Font(21.0f));
    g.drawFittedText("Bridge your Audeobox sounds into the DAW.",
                     58, 194, getWidth() - 116, 34, juce::Justification::centred, 1);
    g.drawFittedText(audioProcessor.getBridgeStatusText(),
                     58, 226, getWidth() - 116, 34, juce::Justification::centred, 1);

    const auto level = audioProcessor.getOutputLevel();
    const auto soundActive = audioProcessor.isSoundActive();
    const auto nowPlayingTitle = audioProcessor.getNowPlayingTitle();
    auto nowPlayingCard = content.withTrimmedTop(130).withHeight(118).toFloat();

    g.setColour(juce::Colour::fromRGBA(255, 255, 255, soundActive ? 34 : 18));
    g.fillRoundedRectangle(nowPlayingCard, 18.0f);

    auto coverArea = nowPlayingCard.reduced(18.0f, 16.0f).removeFromLeft(86.0f);
    auto coverBounds = coverArea.withSizeKeepingCentre(70.0f, 70.0f);
    if (artworkImage.isValid())
    {
        const auto coverRect = coverBounds.toNearestInt();
        g.drawImageWithin(artworkImage, coverRect.getX(), coverRect.getY(), coverRect.getWidth(), coverRect.getHeight(),
                          juce::RectanglePlacement::stretchToFit);
    }
    else
        drawPlaceholderCover(g, coverBounds, soundActive ? "LIVE" : "IDLE");

    auto meterColumn = nowPlayingCard.removeFromRight(92.0f);
    g.setColour(juce::Colour::fromRGBA(0, 0, 0, 110));
    g.fillRect(meterColumn.removeFromLeft(1.0f));
    drawLevelMeter(g, meterColumn, level);

    auto textArea = nowPlayingCard.toNearestInt().reduced(120, 20);
    g.setColour(juce::Colours::white.withAlpha(soundActive ? 0.96f : 0.68f));
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawFittedText(soundActive && nowPlayingTitle.isNotEmpty() ? nowPlayingTitle : (soundActive ? "Active Sound Output" : "Waiting For Sound Output"),
                     textArea.removeFromTop(30), juce::Justification::centredLeft, 1);

    g.setColour(juce::Colour::fromRGB(190, 193, 200).withAlpha(soundActive ? 0.95f : 0.65f));
    g.setFont(juce::Font(15.0f));
    g.drawFittedText(soundActive ? "Cover and meter follow the active desktop sound in real time."
                                 : "Play a sound in the desktop app to light up this card.",
                     textArea.removeFromTop(42), juce::Justification::centredLeft, 2);

    const auto glow = 0.45f + 0.55f * std::sin(pulsePhase);
    const auto connected = audioProcessor.isBridgeReady();
    const auto statusColour = connected
        ? juce::Colour::fromFloatRGBA(0.19f, 0.84f, 0.39f, glow)
        : juce::Colour::fromFloatRGBA(0.84f, 0.43f, 0.25f, 0.85f);

    g.setColour(statusColour);
    g.fillEllipse(38.0f, static_cast<float>(footer.getY() + 39), 20.0f, 20.0f);

    g.setColour(juce::Colour::fromRGB(219, 223, 228));
    g.setFont(juce::Font(20.0f));
    const auto label = connected
        ? (audioProcessor.isBridgeSyncEnabled() ? "Connected" : "Sync Off")
        : (audioProcessor.isBridgeManuallyDisconnected() ? "Disconnected" : "Waiting");
    g.drawText(label, 78, footer.getY() + 28, 180, 42, juce::Justification::centredLeft, false);

    const auto debugSessionId = audioProcessor.getBridgeSessionId();
    const auto debugBpm = juce::String(audioProcessor.getHostBpm(), 2);
    const auto debugPlaying = audioProcessor.isHostPlaying() ? "yes" : "no";
    const auto debugSync = audioProcessor.isBridgeSyncEnabled() ? "on" : "off";
    const auto debugReady = audioProcessor.isBridgeReady() ? "yes" : "no";
    const auto debugEvent = audioProcessor.getBridgeLastEvent();
    const auto debugError = audioProcessor.getBridgeLastError();
    const auto debugBuild = audioProcessor.getBridgeBuildMarker();

    g.setColour(juce::Colour::fromRGB(126, 133, 145));
    g.setFont(juce::Font(12.0f));
    g.drawText("Debug", getWidth() - 282, footer.getY() + 10, 240, 16, juce::Justification::centredRight, false);
    g.drawText(juce::String("ready: ") + debugReady, getWidth() - 282, footer.getY() + 26, 240, 14, juce::Justification::centredRight, false);
    g.drawText(juce::String("sync: ") + debugSync, getWidth() - 282, footer.getY() + 40, 240, 14, juce::Justification::centredRight, false);
    g.drawText(juce::String("playing: ") + debugPlaying, getWidth() - 282, footer.getY() + 54, 240, 14, juce::Justification::centredRight, false);
    g.drawText(juce::String("bpm: ") + debugBpm, getWidth() - 282, footer.getY() + 68, 240, 14, juce::Justification::centredRight, false);
    g.drawText(juce::String("level: ") + juce::String(level, 3), getWidth() - 282, footer.getY() + 82, 240, 14, juce::Justification::centredRight, false);
    g.drawText(juce::String("event: ") + (debugEvent.isNotEmpty() ? debugEvent : "--"), 34, footer.getY() + 46, getWidth() - 68, 14, juce::Justification::centredLeft, false);
    g.drawText(juce::String("error: ") + (debugError.isNotEmpty() ? debugError : "--"), 34, footer.getY() + 62, getWidth() - 68, 14, juce::Justification::centredLeft, false);
    g.drawText(juce::String("session: ") + (debugSessionId.isNotEmpty() ? debugSessionId : "--"), 34, footer.getY() + 78, getWidth() - 68, 14, juce::Justification::centredLeft, false);
    g.drawText(juce::String("build: ") + debugBuild, 34, footer.getY() + 92, getWidth() - 68, 14, juce::Justification::centredLeft, false);
}

void AueoboxAudioProcessorEditor::resized()
{
    helpButton.setBounds(getWidth() - 126, 28, 92, 42);
    reconnectButton.setBounds(getWidth() - 244, 28, 102, 42);
}

void AueoboxAudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &helpButton)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                               "Audeobox Sounds",
                                               "Open Audeobox Sounds, then play any connected sound to preview it in your DAW.");
    }
    else if (button == &reconnectButton)
    {
        audioProcessor.requestBridgeReconnect();
    }
}

void AueoboxAudioProcessorEditor::timerCallback()
{
    pulsePhase += 0.12f;
    reconnectButton.setVisible(audioProcessor.isBridgeManuallyDisconnected());
    refreshArtwork();
    repaint();
}

void AueoboxAudioProcessorEditor::refreshArtwork()
{
    const auto nextArtworkPath = audioProcessor.getNowPlayingArtworkPath();
    if (nextArtworkPath == artworkPath)
        return;

    artworkPath = nextArtworkPath;
    artworkImage = {};

    if (artworkPath.isNotEmpty())
        artworkImage = juce::ImageCache::getFromFile(juce::File(artworkPath));
}
