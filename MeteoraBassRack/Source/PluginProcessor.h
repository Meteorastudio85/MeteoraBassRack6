#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Dsp/AirwindowsApi550b.h"
#include "Dsp/AirwindowsLa2a.h"
#include "Dsp/AirwindowsTubeSmile.h"
#include "Dsp/AirwindowsVog.h"

// =============================================================================
//  Meteora Bass Rack
//  Catena: API 550B -> LA-2A -> Tube Smile (TS808) -> VOG -> polarità -> MASTER OUT
//  Tutta la catena gira in oversampling 4x (meno aliasing sulla distorsione).
// =============================================================================
class MeteoraBassRackProcessor : public juce::AudioProcessor
{
public:
    MeteoraBassRackProcessor();
    ~MeteoraBassRackProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlockBypassed;

    // true se la DAW sta facendo lavorare il plugin (non escluso): accende lampade, VU e logo
    bool isRunning() const noexcept;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // riduzione di guadagno dell'LA-2A per il VU (dB positivi), letta dall'editor
    float popGainReductionDb() noexcept { return la2a.popGainReductionDb(); }
    // picco in uscita dall'ultima lettura (per il LED di picco dell'OUTPUT finale)
    float popOutputPeak() noexcept { return outputPeak.exchange (0.0f); }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr int oversamplingOrder = 2;   // 2^2 = 4x

private:
    void updateModules() noexcept;

    // puntatori ai parametri (letti nel thread audio senza allocazioni)
    struct Params
    {
        std::atomic<float>* power = nullptr;
        std::atomic<float>* eqIn = nullptr;
        std::atomic<float>* bandFreq[4] {}; std::atomic<float>* bandGain[4] {};
        std::atomic<float>* lowShelf = nullptr; std::atomic<float>* highShelf = nullptr;
        std::atomic<float>* laIn = nullptr; std::atomic<float>* laLimit = nullptr;
        std::atomic<float>* laGain = nullptr; std::atomic<float>* laPeak = nullptr;
        std::atomic<float>* tsOn = nullptr; std::atomic<float>* tsDrive = nullptr; std::atomic<float>* tsInput = nullptr;
        std::atomic<float>* tsLevel = nullptr; std::atomic<float>* tsBlend = nullptr;
        std::atomic<float>* vogIn = nullptr; std::atomic<float>* vogRange = nullptr; std::atomic<float>* vogFreq = nullptr;
        std::atomic<float>* vogAmp = nullptr; std::atomic<float>* vogQ = nullptr;
        std::atomic<float>* vogHp = nullptr; std::atomic<float>* vogSlope = nullptr;
        std::atomic<float>* polarity = nullptr; std::atomic<float>* master = nullptr;
    } p;

    static bool on (const std::atomic<float>* a) noexcept { return a->load() > 0.5f; }

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    AirwindowsApi550b eq;
    AirwindowsLa2a la2a;
    AirwindowsTubeSmile tubeSmile;
    AirwindowsVog vog;

    juce::AudioBuffer<float> stageDry;
    juce::SmoothedValue<float> eqMix, laMix, tsMix, vogMix;   // inserimento/esclusione senza click
    juce::SmoothedValue<float> polarity, outputGain, powerMix;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 14 };
    juce::AudioBuffer<float> dryBuffer;
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<double> lastProcessMs { 0.0 }, lastBypassMs { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeteoraBassRackProcessor)
};
