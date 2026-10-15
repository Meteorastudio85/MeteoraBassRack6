#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;

    juce::String fmtDb (float v)
    {
        if (std::abs (v) < 0.05f) return "0.0 dB";
        return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
    }
    juce::String fmtHz (float hz)
    {
        if (hz < 1000.0f) return juce::String (juce::roundToInt (hz)) + " Hz";
        const float k = hz / 1000.0f;
        return (std::abs (k - std::round (k)) < 0.05f ? juce::String (juce::roundToInt (k)) : juce::String (k, 1)) + " kHz";
    }

// mappature dei pomelli
float vogFrequencyHz (float knob0to10, bool range200)
{
    const float lo = range200 ? 100.0f : 20.0f;   // range 40: 20-80 Hz, range 200: 100-400 Hz
    return lo * std::pow (4.0f, knob0to10 / 10.0f);
}
float vogQValue (float knob0to10)           { return 0.5f * std::pow (8.0f, knob0to10 / 10.0f); }
float la2aGainDb (float knob0to100)         { return (knob0to100 - 50.0f) * 0.4f; }   // 50 = 0 dB
float masterGain (float db)                 { return db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (db); }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout MeteoraBassRackProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout pl;
    auto id = [] (const char* s) { return juce::ParameterID { s, 1 }; };
    auto floatParam = [&] (const char* pid, const char* name, float lo, float hi, float step, float def,
                           std::function<juce::String (float)> fmt)
    {
        return std::make_unique<APF> (id (pid), name, juce::NormalisableRange<float> (lo, hi, step), def,
                                      juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                          [fmt] (float v, int) { return fmt (v); }));
    };

    pl.add (std::make_unique<APB> (id ("power"), "Power", true));

    // --- API 550B ---
    pl.add (std::make_unique<APB> (id ("eqIn"), "EQ In", true));
    const int defaultFreq[4] = { 3, 3, 2, 1 };
    for (int b = 0; b < 4; ++b)
    {
        juce::StringArray names;
        for (float f : AirwindowsApi550b::bandFreqs[b]) names.add (fmtHz (f));
        const juce::String n (b + 1);
        pl.add (std::make_unique<APC> (id (("b" + n + "f").toRawUTF8()), "Band " + n + " Freq", names, defaultFreq[b]));
        pl.add (floatParam (("b" + n + "g").toRawUTF8(), ("Band " + n + " Gain").toRawUTF8(), -12.0f, 12.0f, 2.0f, 0.0f, fmtDb));
    }
    pl.add (std::make_unique<APB> (id ("b1sh"), "Band 1 Shelf", false));
    pl.add (std::make_unique<APB> (id ("b4sh"), "Band 4 Shelf", false));

    // --- LA-2A ---
    pl.add (std::make_unique<APB> (id ("laIn"), "LA-2A In", true));
    pl.add (std::make_unique<APB> (id ("laLimit"), "LA-2A Limit", false));
    pl.add (floatParam ("laGain", "LA-2A Input", 0.0f, 100.0f, 0.1f, 50.0f, [] (float v) { return fmtDb (la2aGainDb (v)); }));
    pl.add (floatParam ("laPeak", "LA-2A Peak Reduction", 0.0f, 100.0f, 0.1f, 30.0f, [] (float v) { return juce::String (v / 10.0f, 1); }));

    // --- Tube Smile ---
    auto tenth = [] (float v) { return juce::String (v, 1); };
    pl.add (std::make_unique<APB> (id ("tsOn"), "Tube Smile On", false));
    pl.add (floatParam ("tsDrive", "Drive", 0.0f, 10.0f, 0.01f, 5.0f, tenth));
    pl.add (floatParam ("tsInput", "TS Input", -12.0f, 12.0f, 0.1f, 0.0f, fmtDb));
    pl.add (floatParam ("tsLevel", "TS Output", 0.0f, 10.0f, 0.01f, 5.0f, tenth));
    pl.add (floatParam ("tsBlend", "Blend", 0.0f, 100.0f, 0.1f, 50.0f, [] (float v) { return juce::String (juce::roundToInt (v)) + "% wet"; }));

    // --- VOG ---
    auto rangeParam = std::make_unique<APB> (id ("vogRange"), "VOG Range 200", false);
    auto* rangePtr = rangeParam.get();
    pl.add (std::make_unique<APB> (id ("vogIn"), "VOG In", false));
    pl.add (std::move (rangeParam));
    pl.add (floatParam ("vogFreq", "VOG Frequency", 0.0f, 10.0f, 0.01f, 5.0f,
                            [rangePtr] (float v) { return fmtHz (vogFrequencyHz (v, rangePtr->get())); }));
    pl.add (floatParam ("vogAmp", "VOG Amplitude", 0.0f, 10.0f, 0.01f, 4.0f, [] (float v) { return fmtDb (v * 1.5f); }));
    pl.add (floatParam ("vogQ", "VOG Q", 0.0f, 10.0f, 0.01f, 5.0f, [] (float v) { return "Q " + juce::String (vogQValue (v), 2); }));
    pl.add (std::make_unique<APB> (id ("vogHp"), "VOG High-Pass", true));
    pl.add (std::make_unique<APB> (id ("vogSlope"), "VOG 24 dB", false));

    // --- uscita ---
    pl.add (std::make_unique<APB> (id ("polarity"), "Polarity Invert", false));
    {
        // OUTPUT final: volume di tutto il suono uscito dalla catena (tutto a sinistra = silenzio)
        juce::NormalisableRange<float> outRange (-60.0f, 12.0f, 0.1f);
        outRange.setSkewForCentre (-12.0f);
        pl.add (std::make_unique<APF> (id ("master"), "Output (final)", outRange, 0.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                        [] (float v, int) { return v <= -59.9f ? juce::String ("-inf dB") : fmtDb (v); })));
    }
    return pl;
}

MeteoraBassRackProcessor::MeteoraBassRackProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MeteoraBassRack", createLayout())
{
    auto get = [this] (const char* pid) { return apvts.getRawParameterValue (pid); };
    p.power = get ("power");     p.eqIn = get ("eqIn");
    for (int b = 0; b < 4; ++b)
    {
        const juce::String n (b + 1);
        p.bandFreq[b] = get (("b" + n + "f").toRawUTF8());
        p.bandGain[b] = get (("b" + n + "g").toRawUTF8());
    }
    p.lowShelf = get ("b1sh");   p.highShelf = get ("b4sh");
    p.laIn = get ("laIn");       p.laLimit = get ("laLimit");  p.laGain = get ("laGain");  p.laPeak = get ("laPeak");
    p.tsOn = get ("tsOn");       p.tsDrive = get ("tsDrive");  p.tsInput = get ("tsInput");  p.tsLevel = get ("tsLevel");
    p.tsBlend = get ("tsBlend");
    p.vogIn = get ("vogIn");     p.vogRange = get ("vogRange"); p.vogFreq = get ("vogFreq"); p.vogAmp = get ("vogAmp");
    p.vogQ = get ("vogQ");     p.vogHp = get ("vogHp");     p.vogSlope = get ("vogSlope");
    p.polarity = get ("polarity"); p.master = get ("master");
}

bool MeteoraBassRackProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void MeteoraBassRackProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = juce::jlimit (1, 2, getTotalNumOutputChannels());

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, (size_t) oversamplingOrder,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    const double osRate = sampleRate * (1 << oversamplingOrder);
    const int osBlock = samplesPerBlock * (1 << oversamplingOrder);

    updateModules();   // i moduli partono già con i valori giusti
    eq.prepare (osRate);
    la2a.prepare (osRate);
    tubeSmile.prepare (osRate);
    vog.prepare (osRate);

    stageDry.setSize (numCh, osBlock);

    auto init = [] (juce::SmoothedValue<float>& s, double rate, double secs, float value)
    {
        s.reset (rate, secs);
        s.setCurrentAndTargetValue (value);
    };
    init (eqMix,  osRate, 0.02, on (p.eqIn) ? 1.0f : 0.0f);
    init (laMix,  osRate, 0.02, on (p.laIn) ? 1.0f : 0.0f);
    init (tsMix,  osRate, 0.02, on (p.tsOn) ? 1.0f : 0.0f);
    init (vogMix, osRate, 0.02, on (p.vogIn) ? 1.0f : 0.0f);
    init (polarity,   sampleRate, 0.015, on (p.polarity) ? -1.0f : 1.0f);
    init (outputGain, sampleRate, 0.03, masterGain (p.master->load()));
    init (powerMix,   sampleRate, 0.03, on (p.power) ? 1.0f : 0.0f);

    // il percorso "spento" viene ritardato quanto l'oversampling: accendere/spegnere non sposta la fase
    const int latency = (int) std::round (oversampling->getLatencyInSamples());
    setLatencySamples (latency);
    dryDelay.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh });
    dryDelay.setMaximumDelayInSamples (juce::jmax (latency + 1, 64));
    dryDelay.setDelay ((float) latency);
    dryDelay.reset();
    dryBuffer.setSize (numCh, samplesPerBlock);
}

void MeteoraBassRackProcessor::updateModules() noexcept
{
    for (int b = 0; b < 4; ++b)
    {
        const bool shelf = (b == 0 && on (p.lowShelf)) || (b == 3 && on (p.highShelf));
        eq.setBand (b, juce::roundToInt (p.bandFreq[b]->load()), p.bandGain[b]->load(), shelf);
    }

    la2a.setPeakReduction (p.laPeak->load());
    la2a.setInputDb (la2aGainDb (p.laGain->load()));   // pomello INPUT del compressore
    la2a.setGainDb (0.0f);
    la2a.setLimit (on (p.laLimit));
    la2a.setOutputDb (0.0f);   // l'uscita dell'LA-2A è il pomello OUTPUT (gain)

    tubeSmile.setDrive (p.tsDrive->load());
    tubeSmile.setInputDb (p.tsInput->load());
    tubeSmile.setTone (5.0f);   // il nuovo pedale non ha il TONE: resta a metà
    tubeSmile.setLevel (p.tsLevel->load());
    tubeSmile.setBlend (p.tsBlend->load());

    vog.setFrequency (vogFrequencyHz (p.vogFreq->load(), on (p.vogRange)));
    vog.setAmplitudeDb (p.vogAmp->load() * 1.5f);
    vog.setQ (vogQValue (p.vogQ->load()));
    vog.setOutputDb (0.0f);   // il VOG non ha più un pomello d'uscita: il livello si regola con OUTPUT final
    vog.setHighPass (on (p.vogHp));
    vog.setSlope24 (on (p.vogSlope));
}

void MeteoraBassRackProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // plugin escluso dalla DAW: passa il suono originale (ritardato come quando è attivo) e spegne le lampade
    lastBypassMs.store (juce::Time::getMillisecondCounterHiRes());
    const int numCh = juce::jmin (buffer.getNumChannels(), dryBuffer.getNumChannels());
    const int n = juce::jmin (buffer.getNumSamples(), dryBuffer.getNumSamples());
    if (oversampling == nullptr || numCh == 0 || n == 0) return;
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            dryDelay.pushSample (ch, d[i]);
            d[i] = dryDelay.popSample (ch);
        }
    }
}

bool MeteoraBassRackProcessor::isRunning() const noexcept
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double last = lastProcessMs.load();
    return last > 0.0 && (now - last) < 500.0 && last >= lastBypassMs.load();
}

void MeteoraBassRackProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    lastProcessMs.store (juce::Time::getMillisecondCounterHiRes());
    const int numCh = juce::jmin (buffer.getNumChannels(), stageDry.getNumChannels());
    const int n = buffer.getNumSamples();
    if (oversampling == nullptr || numCh == 0 || n == 0) return;

    updateModules();

    // inserimenti dei moduli: quando un modulo si riaccende riparte da uno stato pulito
    auto setStage = [] (juce::SmoothedValue<float>& mix, bool want, auto&& resetModule)
    {
        const float t = want ? 1.0f : 0.0f;
        if (t != mix.getTargetValue())
        {
            if (want && mix.getCurrentValue() <= 0.0f) resetModule();
            mix.setTargetValue (t);
        }
    };
    setStage (eqMix,  on (p.eqIn),  [this] { eq.reset(); });
    setStage (laMix,  on (p.laIn),  [this] { la2a.reset(); });
    setStage (tsMix,  on (p.tsOn),  [this] { tubeSmile.reset(); });
    setStage (vogMix, on (p.vogIn), [this] { vog.reset(); });
    polarity.setTargetValue (on (p.polarity) ? -1.0f : 1.0f);
    outputGain.setTargetValue (masterGain (p.master->load()));
    powerMix.setTargetValue (on (p.power) ? 1.0f : 0.0f);

    // percorso pulito ritardato (per il tasto OFF)
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* src = buffer.getReadPointer (ch);
        float* dst = dryBuffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            dryDelay.pushSample (ch, src[i]);
            dst[i] = dryDelay.popSample (ch);
        }
    }

    if (! powerMix.isSmoothing() && powerMix.getTargetValue() < 0.5f)
    {
        for (int ch = 0; ch < numCh; ++ch) buffer.copyFrom (ch, 0, dryBuffer, ch, 0, n);
        return;
    }

    juce::dsp::AudioBlock<float> block (buffer);
    auto io = block.getSubsetChannelBlock (0, (size_t) numCh);
    auto os = oversampling->processSamplesUp (io);
    const int osN = (int) os.getNumSamples();

    float* ptr[2] = { os.getChannelPointer (0), os.getChannelPointer (numCh > 1 ? 1 : 0) };

    auto runStage = [&] (juce::SmoothedValue<float>& mix, auto&& process)
    {
        if (! mix.isSmoothing())
        {
            if (mix.getTargetValue() > 0.5f) process();
            return;
        }
        for (int ch = 0; ch < numCh; ++ch) stageDry.copyFrom (ch, 0, ptr[ch], osN);
        process();
        for (int i = 0; i < osN; ++i)
        {
            const float m = mix.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float d = stageDry.getSample (ch, i);
                ptr[ch][i] = d + (ptr[ch][i] - d) * m;
            }
        }
    };

    runStage (eqMix,  [&] { eq.process (ptr, numCh, osN); });
    runStage (laMix,  [&] { la2a.process (ptr, numCh, osN); });
    runStage (tsMix,  [&] { tubeSmile.process (ptr, numCh, osN); });
    runStage (vogMix, [&] { vog.process (ptr, numCh, osN); });

    oversampling->processSamplesDown (io);

    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = polarity.getNextValue() * outputGain.getNextValue();
        const float pm = powerMix.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            const float dry = dryBuffer.getSample (ch, i);
            d[i] = dry + (d[i] * g - dry) * pm;
            peak = juce::jmax (peak, std::abs (d[i]));
        }
    }
    float prev = outputPeak.load();
    while (peak > prev && ! outputPeak.compare_exchange_weak (prev, peak)) {}

    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
}

juce::AudioProcessorEditor* MeteoraBassRackProcessor::createEditor()
{
    return new MeteoraBassRackEditor (*this);
}

void MeteoraBassRackProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void MeteoraBassRackProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MeteoraBassRackProcessor();
}
