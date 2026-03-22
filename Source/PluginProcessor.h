#pragma once

#include <JuceHeader.h>
#include <atomic>

enum class BridgeConnectionState
{
    disconnected,
    connecting,
    connected,
    error
};

enum class BridgeConnectMode
{
    autoConnect,
    manualDisconnect
};

class AueoboxAudioProcessor final : public juce::AudioProcessor,
                                    private juce::Thread
{
public:
    AueoboxAudioProcessor();
    ~AueoboxAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isBridgeConnected() const noexcept;
    bool isBridgeReady() const noexcept;
    bool isBridgeSyncEnabled() const noexcept;
    bool isBridgeManuallyDisconnected() const noexcept;
    bool isHostPlaying() const noexcept;
    double getHostBpm() const noexcept;
    float getOutputLevel() const noexcept;
    bool isSoundActive() const noexcept;
    juce::String getNowPlayingTitle() const;
    juce::String getNowPlayingArtworkPath() const;
    juce::String getBridgeSessionId() const;
    juce::String getPluginInstanceId() const;
    juce::String getBridgeLastEvent() const;
    juce::String getBridgeLastError() const;
    juce::String getBridgeBuildMarker() const;
    juce::String getBridgeStatusText() const;
    void requestBridgeReconnect();

private:
    void run() override;
    void ensureBridgeConnected();
    void disconnectBridge();
    void sendBridgeHandshake();
    void sendTransportState();
    void sendHeartbeat();
    void sendGoodbye(const juce::String& reason);
    void readDesktopMessages();
    void handleDesktopMessage(const juce::var& message);
    void setBridgeLastEvent(const juce::String& event);
    void setBridgeLastError(const juce::String& error);
    void clearBridgeLastError();

    std::unique_ptr<juce::StreamingSocket> bridgeSocket;
    mutable juce::CriticalSection bridgeDataLock;
    juce::String incomingMessageBuffer;
    juce::String sessionId;
    juce::String pluginInstanceId { juce::Uuid().toString() };
    juce::String nowPlayingTitle;
    juce::String nowPlayingArtworkPath;
    juce::String lastBridgeEvent { "init" };
    juce::String lastBridgeError;
    const juce::String buildMarker { juce::String(JucePlugin_VersionString) + " | " + juce::String(__DATE__) + " " + juce::String(__TIME__) };
    std::atomic<BridgeConnectionState> bridgeState { BridgeConnectionState::disconnected };
    std::atomic<BridgeConnectMode> bridgeConnectMode { BridgeConnectMode::autoConnect };
    std::atomic<bool> bridgeConnected { false };
    std::atomic<bool> handshakeComplete { false };
    std::atomic<bool> syncEnabled { true };
    std::atomic<bool> hostPlaying { false };
    std::atomic<bool> hostRecording { false };
    std::atomic<bool> nowPlayingActive { false };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<double> hostPpqPosition { 0.0 };
    std::atomic<double> hostTimeSeconds { 0.0 };
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> remoteOutputLevel { 0.0f };
    std::atomic<int64_t> lastBridgeAttemptMs { 0 };
    std::atomic<int64_t> lastHelloSentMs { 0 };
    std::atomic<int64_t> lastHeartbeatSentMs { 0 };
    std::atomic<uint32_t> messageSequence { 0 };
};
