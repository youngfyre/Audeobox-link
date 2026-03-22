#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
void logBridge(const juce::String& event, const juce::String& detail = {})
{
    juce::Logger::writeToLog(detail.isNotEmpty() ? "[Bridge] " + event + " " + detail : "[Bridge] " + event);
}

float smoothMeterLevel(float currentLevel, float targetLevel) noexcept
{
    const auto attack = 0.45f;
    const auto release = 0.14f;
    const auto factor = targetLevel > currentLevel ? attack : release;
    return currentLevel + (targetLevel - currentLevel) * factor;
}
}

AueoboxAudioProcessor::AueoboxAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread("AudeoboxBridgeThread")
{
    startThread();
}

AueoboxAudioProcessor::~AueoboxAudioProcessor()
{
    signalThreadShouldExit();
    stopThread(1500);
    disconnectBridge();
}

void AueoboxAudioProcessor::prepareToPlay(double, int)
{
}

void AueoboxAudioProcessor::releaseResources()
{
}

bool AueoboxAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void AueoboxAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto measuredLevel = 0.0f;
    if (buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0)
    {
        for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            measuredLevel = juce::jmax(measuredLevel, buffer.getRMSLevel(channel, 0, buffer.getNumSamples()));
    }

    const auto currentLevel = outputLevel.load(std::memory_order_relaxed);
    outputLevel.store(smoothMeterLevel(currentLevel, juce::jlimit(0.0f, 1.0f, measuredLevel)), std::memory_order_relaxed);

    buffer.clear();

    if (auto* currentPlayHead = getPlayHead())
    {
        if (const auto positionInfo = currentPlayHead->getPosition())
        {
            hostPlaying.store(positionInfo->getIsPlaying(), std::memory_order_relaxed);
            hostRecording.store(positionInfo->getIsRecording(), std::memory_order_relaxed);

            if (const auto bpm = positionInfo->getBpm())
                hostBpm.store(*bpm, std::memory_order_relaxed);
            if (const auto ppq = positionInfo->getPpqPosition())
                hostPpqPosition.store(*ppq, std::memory_order_relaxed);
            if (const auto timeSeconds = positionInfo->getTimeInSeconds())
                hostTimeSeconds.store(*timeSeconds, std::memory_order_relaxed);
        }
    }
}

juce::AudioProcessorEditor* AueoboxAudioProcessor::createEditor()
{
    return new AueoboxAudioProcessorEditor(*this);
}

bool AueoboxAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String AueoboxAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AueoboxAudioProcessor::acceptsMidi() const
{
    return true;
}

bool AueoboxAudioProcessor::producesMidi() const
{
    return false;
}

bool AueoboxAudioProcessor::isMidiEffect() const
{
    return false;
}

double AueoboxAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AueoboxAudioProcessor::getNumPrograms()
{
    return 1;
}

int AueoboxAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AueoboxAudioProcessor::setCurrentProgram(int)
{
}

const juce::String AueoboxAudioProcessor::getProgramName(int)
{
    return {};
}

void AueoboxAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void AueoboxAudioProcessor::getStateInformation(juce::MemoryBlock&)
{
}

void AueoboxAudioProcessor::setStateInformation(const void*, int)
{
}

bool AueoboxAudioProcessor::isBridgeConnected() const noexcept
{
    return bridgeConnected.load(std::memory_order_relaxed);
}

bool AueoboxAudioProcessor::isBridgeReady() const noexcept
{
    return isBridgeConnected() && handshakeComplete.load(std::memory_order_relaxed);
}

bool AueoboxAudioProcessor::isBridgeSyncEnabled() const noexcept
{
    return syncEnabled.load(std::memory_order_relaxed);
}

bool AueoboxAudioProcessor::isBridgeManuallyDisconnected() const noexcept
{
    return bridgeConnectMode.load(std::memory_order_relaxed) == BridgeConnectMode::manualDisconnect;
}

bool AueoboxAudioProcessor::isHostPlaying() const noexcept
{
    return hostPlaying.load(std::memory_order_relaxed);
}

double AueoboxAudioProcessor::getHostBpm() const noexcept
{
    return hostBpm.load(std::memory_order_relaxed);
}

float AueoboxAudioProcessor::getOutputLevel() const noexcept
{
    return juce::jmax(outputLevel.load(std::memory_order_relaxed),
                      remoteOutputLevel.load(std::memory_order_relaxed));
}

bool AueoboxAudioProcessor::isSoundActive() const noexcept
{
    return nowPlayingActive.load(std::memory_order_relaxed) || getOutputLevel() > 0.015f;
}

juce::String AueoboxAudioProcessor::getNowPlayingTitle() const
{
    const juce::ScopedLock lock(bridgeDataLock);
    return nowPlayingTitle;
}

juce::String AueoboxAudioProcessor::getNowPlayingArtworkPath() const
{
    const juce::ScopedLock lock(bridgeDataLock);
    return nowPlayingArtworkPath;
}

juce::String AueoboxAudioProcessor::getBridgeSessionId() const
{
    const juce::ScopedLock lock(bridgeDataLock);
    return sessionId;
}

juce::String AueoboxAudioProcessor::getPluginInstanceId() const
{
    return pluginInstanceId;
}

juce::String AueoboxAudioProcessor::getBridgeLastEvent() const
{
    const juce::ScopedLock lock(bridgeDataLock);
    return lastBridgeEvent;
}

juce::String AueoboxAudioProcessor::getBridgeLastError() const
{
    const juce::ScopedLock lock(bridgeDataLock);
    return lastBridgeError;
}

juce::String AueoboxAudioProcessor::getBridgeBuildMarker() const
{
    return buildMarker;
}

juce::String AueoboxAudioProcessor::getBridgeStatusText() const
{
    if (isBridgeManuallyDisconnected())
        return "Disconnected (Manual)";

    if (! isBridgeConnected())
        return "Waiting For Desktop App";

    if (! handshakeComplete.load(std::memory_order_relaxed))
        return "Handshake Pending";

    if (! syncEnabled.load(std::memory_order_relaxed))
        return "Connected (Sync Off)";

    if (isHostPlaying())
        return "Synced To DAW Transport";

    return "Connected To Desktop App";
}

void AueoboxAudioProcessor::requestBridgeReconnect()
{
    bridgeConnectMode.store(BridgeConnectMode::autoConnect, std::memory_order_relaxed);
    bridgeState.store(BridgeConnectionState::disconnected, std::memory_order_relaxed);
    lastBridgeAttemptMs.store(0, std::memory_order_relaxed);
    clearBridgeLastError();
    setBridgeLastEvent("reconnect_requested");
    logBridge("reconnect_requested");
}

void AueoboxAudioProcessor::run()
{
    setBridgeLastEvent("bridge_thread_started");
    logBridge("bridge_thread_started");

    while (! threadShouldExit())
    {
        if (isBridgeManuallyDisconnected())
        {
            wait(120);
            continue;
        }

        ensureBridgeConnected();

        if (! isBridgeConnected())
        {
            wait(120);
            continue;
        }

        readDesktopMessages();

        if (isBridgeConnected())
        {
            if (handshakeComplete.load(std::memory_order_relaxed))
            {
                const auto now = static_cast<int64_t>(juce::Time::getMillisecondCounterHiRes());
                const auto lastHeartbeatMs = lastHeartbeatSentMs.load(std::memory_order_relaxed);
                if (now - lastHeartbeatMs >= 1000)
                    sendHeartbeat();

                sendTransportState();
            }
            else
            {
                const auto now = static_cast<int64_t>(juce::Time::getMillisecondCounterHiRes());
                const auto lastHelloMs = lastHelloSentMs.load(std::memory_order_relaxed);

                if (now - lastHelloMs >= 500)
                    sendBridgeHandshake();
            }
        }

        wait(100);
    }

    setBridgeLastEvent("bridge_thread_stopped");
    logBridge("bridge_thread_stopped");
}

void AueoboxAudioProcessor::ensureBridgeConnected()
{
    if (isBridgeManuallyDisconnected())
        return;

    if (bridgeSocket && bridgeSocket->isConnected())
    {
        bridgeConnected.store(true, std::memory_order_relaxed);
        return;
    }

    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto lastAttempt = static_cast<double>(lastBridgeAttemptMs.load(std::memory_order_relaxed));
    if (now - lastAttempt < 1000.0)
        return;

    lastBridgeAttemptMs.store(static_cast<int64_t>(now), std::memory_order_relaxed);
    setBridgeLastEvent("connect_attempt");
    logBridge("connect_attempt");

    auto socket = std::make_unique<juce::StreamingSocket>();
    if (! socket->connect("127.0.0.1", 48142, 200))
    {
        setBridgeLastError("connect returned false");
        setBridgeLastEvent("connect_failure");
        logBridge("connect_failed");
        disconnectBridge();
        return;
    }

    bridgeSocket = std::move(socket);
    bridgeState.store(BridgeConnectionState::connecting, std::memory_order_relaxed);
    bridgeConnected.store(true, std::memory_order_relaxed);
    handshakeComplete.store(false, std::memory_order_relaxed);
    clearBridgeLastError();
    setBridgeLastEvent("connect_success");
    logBridge("connect_success");
    setBridgeLastEvent("read_loop_started");
    logBridge("read_loop_started");
    logBridge("socket_connected");
    sendBridgeHandshake();
    logBridge("hello_sent");
}

void AueoboxAudioProcessor::disconnectBridge()
{
    if (bridgeSocket)
        bridgeSocket->close();

    bridgeSocket.reset();
    {
        const juce::ScopedLock lock(bridgeDataLock);
        incomingMessageBuffer.clear();
        sessionId.clear();
        nowPlayingTitle.clear();
        nowPlayingArtworkPath.clear();
    }
    bridgeConnected.store(false, std::memory_order_relaxed);
    bridgeState.store(BridgeConnectionState::disconnected, std::memory_order_relaxed);
    handshakeComplete.store(false, std::memory_order_relaxed);
    syncEnabled.store(true, std::memory_order_relaxed);
    nowPlayingActive.store(false, std::memory_order_relaxed);
    lastHelloSentMs.store(0, std::memory_order_relaxed);
    lastHeartbeatSentMs.store(0, std::memory_order_relaxed);
    messageSequence.store(0, std::memory_order_relaxed);
    remoteOutputLevel.store(0.0f, std::memory_order_relaxed);
    setBridgeLastEvent("socket_closed");
    logBridge("socket_closed");
}

void AueoboxAudioProcessor::sendBridgeHandshake()
{
    if (! bridgeSocket || ! bridgeSocket->isConnected())
        return;

    lastHelloSentMs.store(static_cast<int64_t>(juce::Time::getMillisecondCounterHiRes()), std::memory_order_relaxed);
    setBridgeLastEvent("hello_sent");

    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("type", "hello");
    payload->setProperty("version", 1);
    payload->setProperty("sentAt", static_cast<double>(juce::Time::currentTimeMillis()));
    payload->setProperty("pluginVersion", JucePlugin_VersionString);
    payload->setProperty("pluginInstanceId", pluginInstanceId);
    payload->setProperty("hostName", juce::PluginHostType().getHostDescription());
    payload->setProperty("activeProjectName", JucePlugin_Name);

    const auto line = juce::JSON::toString(juce::var(payload.get())) + "\n";
    if (bridgeSocket->write(line.toRawUTF8(), static_cast<int>(line.getNumBytesAsUTF8())) < 0)
    {
        setBridgeLastError("hello write failed");
        setBridgeLastEvent("socket_error");
        disconnectBridge();
    }
}

void AueoboxAudioProcessor::sendTransportState()
{
    if (! bridgeSocket || ! bridgeSocket->isConnected())
    {
        disconnectBridge();
        return;
    }

    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("type", "transport");
    payload->setProperty("version", 1);
    payload->setProperty("sentAt", static_cast<double>(juce::Time::currentTimeMillis()));
    {
        const juce::ScopedLock lock(bridgeDataLock);
        payload->setProperty("sessionId", sessionId);
    }
    payload->setProperty("seq", static_cast<int>(++messageSequence));
    payload->setProperty("isPlaying", hostPlaying.load(std::memory_order_relaxed));
    payload->setProperty("isRecording", hostRecording.load(std::memory_order_relaxed));
    payload->setProperty("bpm", hostBpm.load(std::memory_order_relaxed));
    payload->setProperty("ppqPosition", hostPpqPosition.load(std::memory_order_relaxed));
    payload->setProperty("timeSeconds", hostTimeSeconds.load(std::memory_order_relaxed));

    const auto line = juce::JSON::toString(juce::var(payload.get())) + "\n";
    if (bridgeSocket->write(line.toRawUTF8(), static_cast<int>(line.getNumBytesAsUTF8())) < 0)
    {
        setBridgeLastError("transport write failed");
        setBridgeLastEvent("socket_error");
        disconnectBridge();
    }
}

void AueoboxAudioProcessor::sendHeartbeat()
{
    if (! bridgeSocket || ! bridgeSocket->isConnected())
        return;

    lastHeartbeatSentMs.store(static_cast<int64_t>(juce::Time::getMillisecondCounterHiRes()), std::memory_order_relaxed);
    setBridgeLastEvent("heartbeat_sent");
    logBridge("heartbeat_sent");

    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("type", "heartbeat");
    payload->setProperty("version", 1);
    payload->setProperty("sentAt", static_cast<double>(juce::Time::currentTimeMillis()));
    {
        const juce::ScopedLock lock(bridgeDataLock);
        payload->setProperty("sessionId", sessionId);
    }

    const auto line = juce::JSON::toString(juce::var(payload.get())) + "\n";
    if (bridgeSocket->write(line.toRawUTF8(), static_cast<int>(line.getNumBytesAsUTF8())) < 0)
    {
        setBridgeLastError("heartbeat write failed");
        setBridgeLastEvent("socket_error");
        disconnectBridge();
    }
}

void AueoboxAudioProcessor::sendGoodbye(const juce::String& reason)
{
    if (! bridgeSocket || ! bridgeSocket->isConnected())
        return;

    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("type", "goodbye");
    payload->setProperty("version", 1);
    payload->setProperty("sentAt", static_cast<double>(juce::Time::currentTimeMillis()));
    {
        const juce::ScopedLock lock(bridgeDataLock);
        payload->setProperty("sessionId", sessionId);
    }
    payload->setProperty("reason", reason);

    const auto line = juce::JSON::toString(juce::var(payload.get())) + "\n";
    bridgeSocket->write(line.toRawUTF8(), static_cast<int>(line.getNumBytesAsUTF8()));
}

void AueoboxAudioProcessor::readDesktopMessages()
{
    if (! bridgeSocket || ! bridgeSocket->isConnected())
        return;

    const auto readyState = bridgeSocket->waitUntilReady(true, 1);
    if (readyState < 0)
    {
        if (! bridgeSocket->isConnected())
        {
            setBridgeLastError("read_ready_failed");
            setBridgeLastEvent("socket_error");
            logBridge("socket_error", "read_ready_failed");
            disconnectBridge();
        }
        return;
    }

    if (readyState == 0)
        return;

    char buffer[4096] {};
    const int bytesRead = bridgeSocket->read(buffer, sizeof(buffer) - 1, false);

    if (bytesRead < 0)
    {
        if (! bridgeSocket->isConnected())
        {
            setBridgeLastError("read_failed");
            setBridgeLastEvent("socket_error");
            logBridge("socket_error", "read_failed");
            disconnectBridge();
        }
        return;
    }

    if (bytesRead == 0)
        return;

    {
        const juce::ScopedLock lock(bridgeDataLock);
        incomingMessageBuffer += juce::String(buffer, bytesRead);
    }

    while (true)
    {
        juce::String line;
        {
            const juce::ScopedLock lock(bridgeDataLock);
            if (! incomingMessageBuffer.containsChar('\n'))
                break;

            const auto newlineIndex = incomingMessageBuffer.indexOfChar('\n');
            line = incomingMessageBuffer.substring(0, newlineIndex).trim();
            incomingMessageBuffer = incomingMessageBuffer.substring(newlineIndex + 1);
        }

        if (line.isEmpty())
            continue;

        setBridgeLastEvent("read_loop_message_received");
        logBridge("read_loop_message_received", line.substring(0, juce::jmin(64, line.length())));
        const auto parsed = juce::JSON::parse(line);
        if (! parsed.isVoid())
            handleDesktopMessage(parsed);
    }
}

void AueoboxAudioProcessor::handleDesktopMessage(const juce::var& message)
{
    if (auto* object = message.getDynamicObject())
    {
        const auto type = object->getProperty("type").toString();

        if (type == "helloAck")
        {
            handshakeComplete.store(true, std::memory_order_relaxed);
            bridgeState.store(BridgeConnectionState::connected, std::memory_order_relaxed);
            syncEnabled.store(static_cast<bool>(object->getProperty("syncEnabled")), std::memory_order_relaxed);
            {
                const juce::ScopedLock lock(bridgeDataLock);
                sessionId = object->getProperty("sessionId").toString();
            }
            clearBridgeLastError();
            setBridgeLastEvent("hello_ack_received");
            logBridge("hello_ack");
            setBridgeLastEvent("session_established");
            logBridge("session_established", getBridgeSessionId());
            return;
        }

        if (type == "setSyncEnabled")
        {
            syncEnabled.store(static_cast<bool>(object->getProperty("enabled")), std::memory_order_relaxed);
            logBridge(syncEnabled.load(std::memory_order_relaxed) ? "sync_on" : "sync_off");
            return;
        }

        if (type == "nowPlaying")
        {
            nowPlayingActive.store(static_cast<bool>(object->getProperty("isPlaying")), std::memory_order_relaxed);
            remoteOutputLevel.store(juce::jlimit(0.0f, 1.0f, static_cast<float>(object->getProperty("level"))),
                                    std::memory_order_relaxed);
            {
                const juce::ScopedLock lock(bridgeDataLock);
                nowPlayingTitle = object->getProperty("title").toString();
                nowPlayingArtworkPath = object->getProperty("artworkPath").toString();
            }
            setBridgeLastEvent("now_playing_received");
            return;
        }

        if (type == "disconnect")
        {
            bridgeConnectMode.store(BridgeConnectMode::manualDisconnect, std::memory_order_relaxed);
            setBridgeLastEvent("disconnect_manual");
            logBridge("disconnect_manual");
            sendGoodbye("Desktop requested disconnect");
            disconnectBridge();
            return;
        }
    }
}

void AueoboxAudioProcessor::setBridgeLastEvent(const juce::String& event)
{
    const juce::ScopedLock lock(bridgeDataLock);
    lastBridgeEvent = event;
}

void AueoboxAudioProcessor::setBridgeLastError(const juce::String& error)
{
    const juce::ScopedLock lock(bridgeDataLock);
    lastBridgeError = error;
}

void AueoboxAudioProcessor::clearBridgeLastError()
{
    const juce::ScopedLock lock(bridgeDataLock);
    lastBridgeError.clear();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AueoboxAudioProcessor();
}
