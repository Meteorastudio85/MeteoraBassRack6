#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
    Image loadImage (const String& resourceName)
    {
        int size = 0;
        if (auto* data = BinaryData::getNamedResource (resourceName.toRawUTF8(), size))
            return ImageCache::getFromMemory (data, size);
        return {};
    }

    // riduzione a metà ripetuta: molto più pulita di un'unica riduzione grande
    Image downscale (const Image& src, int w, int h)
    {
        if (! src.isValid() || w <= 0 || h <= 0) return src;
        if (w >= src.getWidth()) return src;
        Image img = src;
        while (img.getWidth() / 2 >= w)
            img = img.rescaled (img.getWidth() / 2, img.getHeight() / 2, Graphics::highResamplingQuality);
        return img.rescaled (w, h, Graphics::highResamplingQuality);
    }

    void stroke (Graphics& g, Point<float> a, Point<float> b, float width, Colour c)
    {
        Path p;
        p.startNewSubPath (a);
        p.lineTo (b);
        g.setColour (c);
        g.strokePath (p, PathStrokeType (width, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // indice "stampato": ombra, alone morbido e tratto pieno, come nella foto
    void softLine (Graphics& g, Point<float> a, Point<float> b, float w, Colour c, float alpha)
    {
        const Point<float> off (0.7f, 0.9f);
        stroke (g, a + off, b + off, w * 1.5f, Colours::black.withAlpha (0.45f * alpha));
        stroke (g, a, b, w * 2.3f, c.withAlpha (0.22f * alpha));
        stroke (g, a, b, w, c.withAlpha (0.92f * alpha));
    }

    String fmtHz (float hz)
    {
        if (hz < 1000.0f) return String (roundToInt (hz)) + " Hz";
        const float k = hz / 1000.0f;
        return (std::abs (k - std::round (k)) < 0.05f ? String (roundToInt (k)) : String (k, 1)) + " kHz";
    }

    const char* stateParams[] = { "power", "eqIn", "b1sh", "b4sh", "laIn", "laLimit", "tsOn",
                                  "vogIn", "vogRange", "vogHp", "vogSlope", "polarity" };
}

// =============================================================================
// RackKnob
// =============================================================================
RackKnob::RackKnob (const layout::Knob& s, const Image* filmstrip, float sw, float sh)
    : spec (s), strip (filmstrip), stripW (sw), stripH (sh)
{
    setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    setMouseDragSensitivity (220);
    setScrollWheelEnabled (true);
}

Point<float> RackKnob::toLogical (Point<int> local) const noexcept
{
    return { ((float) (local.x + getX())) / editorScale, ((float) (local.y + getY())) / editorScale };
}

bool RackKnob::hitTest (int x, int y)
{
    const auto p = toLogical ({ x, y });
    const float d = p.getDistanceFrom ({ spec.x, spec.y });
    if (spec.style == layout::Style::ApiSkirt)
        return d <= spec.hit && d > layout::apiCapR;
    return d <= spec.hit;
}

void RackKnob::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        rightClickInProgress = true;
        if (onRightClick) onRightClick();
        return;
    }
    rightClickInProgress = false;
    Slider::mouseDown (e);
}

void RackKnob::mouseDrag (const MouseEvent& e)
{
    if (! rightClickInProgress) Slider::mouseDrag (e);
}

void RackKnob::mouseUp (const MouseEvent& e)
{
    if (rightClickInProgress) { rightClickInProgress = false; return; }
    Slider::mouseUp (e);
}

void RackKnob::paint (Graphics& g)
{
    // disegno in coordinate logiche dell'editor
    g.addTransform (AffineTransform::scale (editorScale).translated ((float) -getX(), (float) -getY()));

    const float prop = (float) valueToProportionOfLength (getValue());
    const float angle = degreesToRadians (-135.0f + 270.0f * prop - 90.0f);
    const Point<float> dir (std::cos (angle), std::sin (angle));
    const Point<float> c (spec.x, spec.y);

    switch (spec.style)
    {
        case layout::Style::Line:
            softLine (g, c + dir * spec.r0, c + dir * spec.r1, spec.width, Colour (238, 234, 226), 1.0f);
            break;

        case layout::Style::ApiCap:   // tacca scura sul cappuccio bianco
            softLine (g, c + dir * spec.r0, c + dir * spec.r1, spec.width, Colour (38, 40, 48), 0.9f);
            break;

        case layout::Style::Real:     // il pomello fotografato gira davvero
            if (realSize > 0)
            {
                const float rot = degreesToRadians (-135.0f + 270.0f * prop);
                if (std::abs (rot - realCachedAngle) > 1.0e-4f || ! realCache.isValid())
                {
                    renderReal (rot);
                    realCachedAngle = rot;
                }
                g.setImageResamplingQuality (Graphics::highResamplingQuality);
                g.drawImage (realCache, Rectangle<float> (spec.x - realW * 0.5f, spec.y - realH * 0.5f, realW, realH));
            }
            break;

        case layout::Style::ApiSkirt: // manopola con indice a torre: 7 fotogrammi con la luce ferma
            if (strip != nullptr && strip->isValid())
            {
                const int frames = 7;
                const int frame = jlimit (0, frames - 1, roundToInt (getValue()));
                const int d = strip->getHeight();
                const auto sub = strip->getClippedImage ({ frame * d, 0, d, d });
                g.setImageResamplingQuality (Graphics::highResamplingQuality);
                g.drawImage (sub, Rectangle<float> (spec.x - stripW * 0.5f, spec.y - stripH * 0.5f, stripW, stripH));
            }
            break;
    }
}

void RackKnob::setRealTextures (const Image& texture, const Image& light, float w, float h, float discR)
{
    const int n = texture.getWidth();
    if (n <= 1 || texture.getHeight() != n || light.getWidth() != n || light.getHeight() != n) return;

    realDetail.assign ((size_t) (n * n * 3), 0.0f);
    realLight.assign ((size_t) (n * n * 3), 0.0f);
    const Image::BitmapData bt (texture, Image::BitmapData::readOnly);
    const Image::BitmapData bl (light, Image::BitmapData::readOnly);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            const auto ct = bt.getPixelColour (x, y);
            const auto cl = bl.getPixelColour (x, y);
            const size_t i = (size_t) ((y * n + x) * 3);
            realLight[i]     = cl.getFloatRed();
            realLight[i + 1] = cl.getFloatGreen();
            realLight[i + 2] = cl.getFloatBlue();
            realDetail[i]     = ct.getFloatRed()   - realLight[i];
            realDetail[i + 1] = ct.getFloatGreen() - realLight[i + 1];
            realDetail[i + 2] = ct.getFloatBlue()  - realLight[i + 2];
        }

    realSize = n;
    realW = w;
    realH = h;
    realDiscPx = discR * (float) n / w;
    realCache = Image (Image::ARGB, n, n, true);
    realCachedAngle = 1.0e9f;
    repaint();
}

// pixel = dettaglio ruotato + luce ferma: l'indice e la zigrinatura girano, i riflessi no
void RackKnob::renderReal (float angleRadians)
{
    const int n = realSize;
    const float c = (float) (n - 1) * 0.5f;
    const float cs = std::cos (angleRadians), sn = std::sin (angleRadians);
    const float maxCoord = (float) n - 1.001f;
    Image::BitmapData bd (realCache, Image::BitmapData::writeOnly);

    for (int y = 0; y < n; ++y)
    {
        for (int x = 0; x < n; ++x)
        {
            const float dx = (float) x - c, dy = (float) y - c;
            const float alpha = jlimit (0.0f, 1.0f, (realDiscPx - std::sqrt (dx * dx + dy * dy)) / 1.5f + 0.5f);
            if (alpha <= 0.0f)
            {
                bd.setPixelColour (x, y, Colours::transparentBlack);
                continue;
            }

            // punto della foto che finisce qui dopo la rotazione (bilineare)
            const float sx = jlimit (0.0f, maxCoord, c + cs * dx + sn * dy);
            const float sy = jlimit (0.0f, maxCoord, c - sn * dx + cs * dy);
            const int x0 = (int) sx, y0 = (int) sy;
            const float fx = sx - (float) x0, fy = sy - (float) y0;
            const float* p00 = realDetail.data() + (size_t) ((y0 * n + x0) * 3);
            const float* p10 = p00 + 3;
            const float* p01 = realDetail.data() + (size_t) (((y0 + 1) * n + x0) * 3);
            const float* p11 = p01 + 3;
            const float* lt  = realLight.data() + (size_t) ((y * n + x) * 3);

            float rgb[3];
            for (int k = 0; k < 3; ++k)
            {
                const float top = p00[k] + (p10[k] - p00[k]) * fx;
                const float bot = p01[k] + (p11[k] - p01[k]) * fx;
                rgb[k] = jlimit (0.0f, 1.0f, top + (bot - top) * fy + lt[k]);
            }
            bd.setPixelColour (x, y, Colour::fromFloatRGBA (rgb[0], rgb[1], rgb[2], alpha));
        }
    }
}

// =============================================================================
// RackButton
// =============================================================================
RackButton::RackButton (const String& name, Rectangle<float> logicalArea, bool isRound)
    : Button (name), area (logicalArea), round (isRound)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
}

bool RackButton::hitTest (int x, int y)
{
    const Point<float> p (((float) (x + getX())) / editorScale, ((float) (y + getY())) / editorScale);
    if (round)
        return p.getDistanceFrom (area.getCentre()) <= area.getWidth() * 0.5f;
    return area.contains (p);
}

// =============================================================================
// Editor
// =============================================================================
MeteoraBassRackEditor::MeteoraBassRackEditor (MeteoraBassRackProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    loadAssets();

    // --- pomelli
    for (const auto& spec : layout::knobs)
    {
        const Image* strip = nullptr;
        float sw = 0, sh = 0;
        if (spec.style == layout::Style::ApiSkirt)
        {
            const auto it = sprites.find ("api_skirt_" + String (spec.band));
            if (it != sprites.end()) { strip = &it->second.image; sw = it->second.w; sh = it->second.h; }
        }
        auto* k = knobs.add (new RackKnob (spec, strip, sw, sh));

        if (spec.style == layout::Style::Real)
        {
            const String base = String (spec.paramId) == "laGain" ? "knob_in" : "knob_pr";
            const auto t = sprites.find (base), l = sprites.find (base + "_light");
            if (t != sprites.end() && l != sprites.end())
                k->setRealTextures (t->second.image, l->second.image, t->second.w, t->second.h, t->second.disc);
        }
        addAndMakeVisible (k);
        sliderAttachments.add (new AudioProcessorValueTreeState::SliderAttachment (proc.apvts, spec.paramId, *k));

        if (auto* param = proc.apvts.getParameter (spec.paramId))
        {
            k->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
            k->setTooltip (param->getName (64));
        }
        k->setPopupDisplayEnabled (true, true, this, 1500);

        if (spec.band >= 0)
        {
            const int band = spec.band;
            k->onRightClick = [this, band, k] { showBandMenu (band, *k); };
        }
    }

    // --- pulsanti (lo stato lo disegna l'editor)
    for (const auto& b : layout::buttons)
    {
        const auto area = b.round ? Rectangle<float> (b.x - b.w, b.y - b.w, b.w * 2.0f, b.w * 2.0f)
                                  : Rectangle<float> (b.x, b.y, b.w, b.h);
        auto* btn = buttons.add (new RackButton (b.paramId, area, b.round));
        btn->setClickingTogglesState (true);
        btn->setTooltip (b.tip);
        addAndMakeVisible (btn);
        buttonAttachments.add (new AudioProcessorValueTreeState::ButtonAttachment (proc.apvts, b.paramId, *btn));
    }

    powerOffButton = std::make_unique<RackButton> ("off", Rectangle<float> (layout::powerOff[0], layout::powerOff[1], layout::powerOff[2], layout::powerOff[3]), false);
    powerOnButton  = std::make_unique<RackButton> ("on",  Rectangle<float> (layout::powerOn[0],  layout::powerOn[1],  layout::powerOn[2],  layout::powerOn[3]),  false);
    powerOffButton->setTooltip ("Spegni");
    powerOnButton->setTooltip ("Accendi");
    powerOffButton->onClick = [this] { setParam ("power", 0.0f); };
    powerOnButton->onClick  = [this] { setParam ("power", 1.0f); };
    addAndMakeVisible (*powerOffButton);
    addAndMakeVisible (*powerOnButton);

    // i due LED del VOG sono il selettore CENTER: rosso = 40, verde = 200
    auto ledArea = [] (const float* c) { return Rectangle<float> (c[0] - c[2], c[1] - c[2], c[2] * 2.0f, c[2] * 2.0f); };
    range40Button  = std::make_unique<RackButton> ("40",  ledArea (layout::range40),  true);
    range200Button = std::make_unique<RackButton> ("200", ledArea (layout::range200), true);
    range40Button->setTooltip ("VOG center: 40 (20-80 Hz)");
    range200Button->setTooltip ("VOG center: 200 (100-400 Hz)");
    range40Button->onClick  = [this] { setParam ("vogRange", 0.0f); };
    range200Button->onClick = [this] { setParam ("vogRange", 1.0f); };
    addAndMakeVisible (*range40Button);
    addAndMakeVisible (*range200Button);

    lamp = (isOn ("power") && proc.isRunning()) ? 1.0f : 0.0f;
    needleDb = lamp > 0.5f ? 0.0f : -24.0f;
    lastHash = stateHash();

    // --- finestra: almeno un quarto di uno schermo 1920x1080, ridimensionabile in proporzione
    const int savedWidth = (int) proc.apvts.state.getProperty ("uiWidth", 960);
    setResizable (true, true);
    setResizeLimits (688, (int) std::round (688 * layout::H / layout::W),
                     2064, (int) std::round (2064 * layout::H / layout::W));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (layout::W / layout::H);
    const int w = jlimit (688, 2064, savedWidth);
    setSize (w, (int) std::round (w * layout::H / layout::W));

    lastTime = Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (30);
}

MeteoraBassRackEditor::~MeteoraBassRackEditor()
{
    stopTimer();
}

void MeteoraBassRackEditor::loadAssets()
{
    background = loadImage ("background_jpg");

    int size = 0;
    const auto* data = BinaryData::getNamedResource ("layout_json", size);
    if (data == nullptr) return;
    const auto json = JSON::parse (String::fromUTF8 (data, size));

    if (auto* obj = json["sprites"].getDynamicObject())
    {
        for (const auto& nv : obj->getProperties())
        {
            Sprite s;
            const auto& v = nv.value;
            s.x = (float) v["x"];
            s.y = (float) v["y"];
            s.w = (float) v["w"];
            s.h = (float) v["h"];
            s.frames = v.hasProperty ("frames") ? (int) v["frames"] : 1;
            s.disc = v.hasProperty ("disc") ? (float) v["disc"] : 0.0f;
            const String name = nv.name.toString();
            s.image = loadImage (name + "_png");
            sprites[name] = s;
        }
    }

    const auto vu = json["vu"];
    if (vu.isObject())
    {
        vuPivot = { (float) vu["pivot"][0], (float) vu["pivot"][1] };
        vuRadius = (float) vu["radius"];
        vuSpan = (float) vu["span"];
        vuFace = { (float) vu["face"][0], (float) vu["face"][1], (float) vu["face"][2], (float) vu["face"][3] };
    }
}

bool MeteoraBassRackEditor::isOn (const String& id) const
{
    if (auto* v = proc.apvts.getRawParameterValue (id))
        return v->load() > 0.5f;
    return false;
}

void MeteoraBassRackEditor::setParam (const String& id, float plainValue)
{
    if (auto* param = proc.apvts.getParameter (id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
        param->endChangeGesture();
    }
    repaint();
}

uint32 MeteoraBassRackEditor::stateHash() const
{
    uint32 h = 0;
    for (auto* id : stateParams)
        h = (h << 1) | (isOn (id) ? 1u : 0u);
    return h;
}

Rectangle<int> MeteoraBassRackEditor::toEditor (Rectangle<float> logical) const
{
    return (logical * scale()).getSmallestIntegerContainer();
}

void MeteoraBassRackEditor::drawSprite (Graphics& g, const String& name, float alpha) const
{
    if (alpha <= 0.002f) return;
    const auto it = sprites.find (name);
    if (it == sprites.end() || ! it->second.image.isValid()) return;
    const auto& s = it->second;
    g.setOpacity (jlimit (0.0f, 1.0f, alpha));
    g.drawImage (s.image, Rectangle<float> (s.x - s.w * 0.5f, s.y - s.h * 0.5f, s.w, s.h));
    g.setOpacity (1.0f);
}

void MeteoraBassRackEditor::drawSpriteAt (Graphics& g, const String& name, float x, float y, float alpha) const
{
    const auto it = sprites.find (name);
    if (it == sprites.end() || ! it->second.image.isValid() || alpha <= 0.002f) return;
    const auto& s = it->second;
    g.setOpacity (jlimit (0.0f, 1.0f, alpha));
    g.drawImage (s.image, Rectangle<float> (x - s.w * 0.5f, y - s.h * 0.5f, s.w, s.h));
    g.setOpacity (1.0f);
}

void MeteoraBassRackEditor::drawNeedle (Graphics& g) const
{
    auto vuPos = [] (float db) { return (std::pow (10.0f, db / 20.0f) - 0.1f) / (std::pow (10.0f, 3.0f / 20.0f) - 0.1f); };
    const float deg = -vuSpan + 2.0f * vuSpan * jlimit (-0.06f, 1.04f, vuPos (needleDb));
    const float a = degreesToRadians (deg - 90.0f);
    const Point<float> dir (std::cos (a), std::sin (a));
    const float r0 = vuRadius * 0.35f, r1 = vuRadius + 10.0f;
    const auto p0 = vuPivot + dir * r0, p1 = vuPivot + dir * r1;

    Graphics::ScopedSaveState state (g);
    Path clip;
    clip.addRectangle (vuFace.reduced (1.0f));
    g.reduceClipRegion (clip);

    const Point<float> shadow (2.4f, 2.8f);
    stroke (g, p0 + shadow, p1 + shadow, 2.4f, Colour (70, 24, 0).withAlpha (0.18f));   // ombra sul quadrante
    stroke (g, p0, p1, 2.8f, Colour (30, 10, 4).withAlpha (0.25f));
    stroke (g, p0, p1, 1.6f, Colour (22, 8, 3).withAlpha (0.95f));
}

void MeteoraBassRackEditor::paint (Graphics& g)
{
    g.fillAll (Colours::black);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (backgroundCache.isValid() ? backgroundCache : background, getLocalBounds().toFloat());

    g.addTransform (AffineTransform::scale (scale()));

    // VU: lampada e ago
    drawSprite (g, "vu_off", 1.0f - lamp);
    drawNeedle (g);

    // logo che si illumina quando il plugin è acceso
    drawSprite (g, "logo_glow", lamp);

    // levette LA-2A (su = IN / LIMIT, giù = OFF / COMP)
    drawSpriteAt (g, isOn ("laIn")    ? "lever_up" : "lever_down", layout::leverIn[0],    layout::leverIn[1]);
    drawSpriteAt (g, isOn ("laLimit") ? "lever_up" : "lever_down", layout::leverLimit[0], layout::leverLimit[1]);

    // lampade
    if (lamp > 0.002f)
    {
        drawSprite (g, "pow_on", lamp);
        drawSprite (g, isOn ("vogRange") ? "led_200_on" : "led_40_on", lamp);
        if (isOn ("tsOn"))     drawSprite (g, "led_ts_on", lamp);
        if (isOn ("eqIn"))     drawSprite (g, "led_eq_on", lamp);
        if (isOn ("vogIn"))    drawSprite (g, "btn_on_0", lamp);
        if (isOn ("vogHp"))    drawSprite (g, "btn_on_1", lamp);
        if (isOn ("vogSlope")) drawSprite (g, "btn_on_2", lamp);
        if (isOn ("polarity")) drawSprite (g, "btn_on_3", lamp);
        if (clipHold > 0.0f)   drawSprite (g, "led_clip_on", lamp);
    }
}

void MeteoraBassRackEditor::resized()
{
    const float s = scale();

    for (auto* k : knobs)
    {
        k->editorScale = s;
        const float rb = k->boxRadius();
        k->setBounds (toEditor ({ k->spec.x - rb, k->spec.y - rb, rb * 2.0f, rb * 2.0f }));
    }
    for (auto* b : buttons)
    {
        b->editorScale = s;
        b->setBounds (toEditor (b->area));
    }
    for (auto* b : { powerOffButton.get(), powerOnButton.get(), range40Button.get(), range200Button.get() })
    {
        b->editorScale = s;
        b->setBounds (toEditor (b->area));
    }

    // sfondo già ridotto alla risoluzione dello schermo (più nitido e più leggero)
    const float displayScale = Component::getApproximateScaleFactorForComponent (this);
    backgroundCache = downscale (background, roundToInt ((float) getWidth() * displayScale),
                                             roundToInt ((float) getHeight() * displayScale));

    proc.apvts.state.setProperty ("uiWidth", getWidth(), nullptr);
}

void MeteoraBassRackEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = jlimit (0.0f, 0.1f, (float) (now - lastTime));
    lastTime = now;

    // lampade, VU e logo accesi solo se il plugin è acceso (ON) e la DAW lo sta facendo lavorare
    const bool power = isOn ("power") && proc.isRunning();
    const float target = power ? 1.0f : 0.0f;
    const float tau = power ? 0.4f : 0.18f;          // le valvole si scaldano più lentamente di come si spengono
    const float newLamp = lamp + (target - lamp) * (1.0f - std::exp (-dt / tau));

    const float gr = proc.popGainReductionDb();
    const float needleTarget = power ? (isOn ("laIn") ? -gr : 0.0f) : -24.0f;
    needleDb += (needleTarget - needleDb) * (1.0f - std::exp (-dt / 0.08f));   // balistica VU

    // LED di picco: si accende sopra -0,5 dBFS e resta acceso un attimo (gain staging)
    const bool wasClip = clipHold > 0.0f;
    if (proc.popOutputPeak() > 0.944f) clipHold = 0.35f;
    else clipHold = jmax (0.0f, clipHold - dt);

    const auto h = stateHash();
    const bool full = h != lastHash || std::abs (newLamp - lamp) > 0.002f || wasClip != (clipHold > 0.0f)
                      || power != lastRunning;
    lastRunning = power;
    lamp = std::abs (target - newLamp) < 0.002f ? target : newLamp;
    lastHash = h;

    if (full) repaint();
    else      repaint (toEditor (vuFace.expanded (2.0f)));
}

void MeteoraBassRackEditor::showBandMenu (int band, RackKnob& knob)
{
    const String freqId = "b" + String (band + 1) + "f";
    const String shelfId = band == 0 ? "b1sh" : (band == 3 ? "b4sh" : "");
    int current = 0;
    if (auto* v = proc.apvts.getRawParameterValue (freqId))
        current = roundToInt (v->load());

    PopupMenu m;
    m.addSectionHeader ("Banda " + String (band + 1) + " - frequenza");
    for (int i = 0; i < 7; ++i)
        m.addItem (1 + i, fmtHz (AirwindowsApi550b::bandFreqs[band][i]), true, i == current);

    if (shelfId.isNotEmpty())
    {
        const bool shelf = isOn (shelfId);
        m.addSeparator();
        m.addSectionHeader ("Forma");
        m.addItem (100, "Campana", true, ! shelf);
        m.addItem (101, "Shelf", true, shelf);
    }

    Component::SafePointer<MeteoraBassRackEditor> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&knob),
                     [safe, freqId, shelfId] (int result)
                     {
                         if (safe == nullptr || result == 0) return;
                         if (result >= 1 && result <= 7)       safe->setParam (freqId, (float) (result - 1));
                         else if (result == 100 || result == 101) safe->setParam (shelfId, result == 101 ? 1.0f : 0.0f);
                     });
}
