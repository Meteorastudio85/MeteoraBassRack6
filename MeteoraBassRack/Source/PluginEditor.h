#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include "PluginProcessor.h"
#include "Layout.h"

// -----------------------------------------------------------------------------
// Pomello disegnato sulla foto: il corpo è nella grafica, qui si disegna solo
// l'indice (o il fotogramma del cappuccio API), così la luce resta quella della foto.
// -----------------------------------------------------------------------------
class RackKnob : public juce::Slider
{
public:
    RackKnob (const layout::Knob& s, const juce::Image* filmstrip, float stripW, float stripH);

    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    float boxRadius() const noexcept { return juce::jmax (juce::jmax (spec.hit, spec.r), realW * 0.5f) + 2.0f; }

    // pomello "vero": foto del pomello (con l'indice) e la sua luce; ruota la foto, la luce resta ferma
    void setRealTextures (const juce::Image& texture, const juce::Image& light, float w, float h, float discR);

    const layout::Knob spec;
    float editorScale = 1.0f;
    std::function<void()> onRightClick;

private:
    juce::Point<float> toLogical (juce::Point<int> local) const noexcept;
    void renderReal (float angleRadians);

    const juce::Image* strip = nullptr;
    float stripW = 0, stripH = 0;
    bool rightClickInProgress = false;

    std::vector<float> realDetail, realLight;   // RGB: dettaglio (ruota) e luce (ferma)
    int realSize = 0;
    float realW = 0, realH = 0, realDiscPx = 0;
    juce::Image realCache;
    float realCachedAngle = 1.0e9f;
};

// Pulsante invisibile: lo stato (LED, levette, lampade) lo disegna l'editor con le immagini.
class RackButton : public juce::Button
{
public:
    RackButton (const juce::String& name, juce::Rectangle<float> logicalArea, bool isRound);
    void paintButton (juce::Graphics&, bool, bool) override {}
    bool hitTest (int x, int y) override;

    const juce::Rectangle<float> area;   // coordinate logiche
    const bool round;
    float editorScale = 1.0f;
};

class MeteoraBassRackEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MeteoraBassRackEditor (MeteoraBassRackProcessor&);
    ~MeteoraBassRackEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Sprite
    {
        juce::Image image;
        float x = 0, y = 0, w = 0, h = 0;
        int frames = 1;
        float disc = 0;
    };

    void timerCallback() override;
    void loadAssets();
    void drawSprite (juce::Graphics&, const juce::String& name, float alpha = 1.0f) const;
    void drawSpriteAt (juce::Graphics&, const juce::String& name, float x, float y, float alpha = 1.0f) const;
    void drawNeedle (juce::Graphics&) const;
    void showBandMenu (int band, RackKnob& knob);
    void setParam (const juce::String& id, float plainValue);
    bool isOn (const juce::String& id) const;
    juce::uint32 stateHash() const;
    float scale() const noexcept { return (float) getWidth() / layout::W; }
    juce::Rectangle<int> toEditor (juce::Rectangle<float> logical) const;

    MeteoraBassRackProcessor& proc;

    juce::Image background, backgroundCache;
    std::map<juce::String, Sprite> sprites;
    juce::Point<float> vuPivot { 670.5f, 552.0f };
    float vuRadius = 104.0f, vuSpan = 44.0f;
    juce::Rectangle<float> vuFace { 579.5f, 422.0f, 182.0f, 87.0f };

    juce::OwnedArray<RackKnob> knobs;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
    juce::OwnedArray<RackButton> buttons;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachments;
    std::unique_ptr<RackButton> powerOffButton, powerOnButton, range40Button, range200Button;
    juce::TooltipWindow tooltipWindow { this, 700 };

    float lamp = 1.0f;          // 0 = lampade spente, 1 = accese (con riscaldamento)
    float needleDb = -20.0f;    // posizione dell'ago (scala VU)
    float clipHold = 0.0f;      // LED di picco dell'uscita finale
    bool lastRunning = false;
    juce::uint32 lastHash = 0;
    double lastTime = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeteoraBassRackEditor)
};
