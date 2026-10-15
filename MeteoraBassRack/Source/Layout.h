#pragma once
// =============================================================================
//  Posizioni dei comandi sulla grafica (coordinate logiche 1536 x 939, cioè i pixel
//  dell'immagine di partenza). La grafica vera è a 2x. File generato da tools/bake_assets.py
// =============================================================================
namespace layout
{
    constexpr float W = 1536.0f;
    constexpr float H = 939.0f;

    enum class Style
    {
        Line,      // indice bianco sui pomelli neri
        ApiSkirt,  // manopola EQ con indice a torre: frequenza (7 fotogrammi)
        ApiCap,    // cappuccio bianco EQ: guadagno (tacca scura)
        Real       // pomello fotografato che gira davvero (texture ruotata + luce fissa)
    };

    struct Knob
    {
        const char* paramId;
        float x, y;        // centro
        float r;           // raggio del pomello
        Style style;
        float r0, r1;      // inizio / fine dell'indice (raggi assoluti)
        float width;       // spessore dell'indice
        float hit;         // raggio cliccabile
        int band;          // banda EQ (-1 se non EQ)
    };

    inline constexpr float apiCapR = 22.0f;

    inline constexpr Knob knobs[] = {
        { "b1f", 225.0f, 240.0f, 48.0f, Style::ApiSkirt, 0.0f, 0.0f, 0.0f, 48.0f, 0 },
        { "b1g", 225.0f, 240.0f, 22.0f, Style::ApiCap, 3.0f, 19.0f, 2.4f, 22.0f, 0 },
        { "b2f", 225.0f, 380.0f, 48.0f, Style::ApiSkirt, 0.0f, 0.0f, 0.0f, 48.0f, 1 },
        { "b2g", 225.0f, 380.0f, 22.0f, Style::ApiCap, 3.0f, 19.0f, 2.4f, 22.0f, 1 },
        { "b3f", 225.0f, 520.0f, 48.0f, Style::ApiSkirt, 0.0f, 0.0f, 0.0f, 48.0f, 2 },
        { "b3g", 225.0f, 520.0f, 22.0f, Style::ApiCap, 3.0f, 19.0f, 2.4f, 22.0f, 2 },
        { "b4f", 225.0f, 660.0f, 48.0f, Style::ApiSkirt, 0.0f, 0.0f, 0.0f, 48.0f, 3 },
        { "b4g", 225.0f, 660.0f, 22.0f, Style::ApiCap, 3.0f, 19.0f, 2.4f, 22.0f, 3 },
        { "laPeak", 1110.0f, 640.0f, 79.0f, Style::Real, 0.0f, 0.0f, 0.0f, 80.0f, -1 },
        { "laGain", 620.0f, 640.0f, 79.0f, Style::Real, 0.0f, 0.0f, 0.0f, 80.0f, -1 },
        { "vogAmp", 535.0f, 250.0f, 55.0f, Style::Line, 4.0f, 44.0f, 4.0f, 80.0f, -1 },
        { "vogFreq", 780.0f, 250.0f, 55.0f, Style::Line, 4.0f, 44.0f, 4.0f, 80.0f, -1 },
        { "vogQ", 1200.0f, 231.0f, 25.0f, Style::Line, 2.0f, 20.0f, 2.6f, 36.0f, -1 },
        { "master", 1312.0f, 777.0f, 25.0f, Style::Line, 2.0f, 20.0f, 2.6f, 34.0f, -1 },
        { "tsInput", 1303.9f, 412.6f, 20.4f, Style::Line, 9.0f, 19.0f, 2.2f, 24.0f, -1 },
        { "tsLevel", 1366.3f, 412.6f, 20.4f, Style::Line, 9.0f, 19.0f, 2.2f, 24.0f, -1 },
        { "tsDrive", 1428.8f, 412.6f, 20.4f, Style::Line, 9.0f, 19.0f, 2.2f, 24.0f, -1 },
        { "tsBlend", 1302.8f, 580.3f, 19.0f, Style::Line, 8.0f, 17.0f, 2.0f, 22.0f, -1 },
    };

    struct Button
    {
        const char* paramId;
        float x, y, w, h;   // rotondi: x,y = centro e w = raggio; rettangolari: angolo + dimensioni
        bool round;
        const char* tip;
    };

    inline constexpr Button buttons[] = {
        { "vogIn", 925.0f, 290.0f, 22.0f, 0.0f, true, "VOG in" },
        { "vogHp", 985.0f, 290.0f, 22.0f, 0.0f, true, "VOG: taglio sotto la risonanza" },
        { "vogSlope", 1045.0f, 290.0f, 22.0f, 0.0f, true, "VOG: pendenza 12 / 24 dB" },
        { "polarity", 1105.0f, 290.0f, 22.0f, 0.0f, true, "Inversione di polarita'" },
        { "laIn", 493.0f, 500.0f, 24.0f, 0.0f, true, "LA-2A in / off" },
        { "laLimit", 598.0f, 500.0f, 24.0f, 0.0f, true, "LA-2A limit / comp" },
        { "eqIn", 138.0f, 705.0f, 50.0f, 90.0f, false, "EQ in" },
        { "tsOn", 1324.0f, 622.7f, 85.8f, 62.4f, false, "Tube Smile on / off" },
    };

    // tasti speciali (impostano un valore invece di commutare)
    inline constexpr float powerOff[4]  = { 1300.0f, 127.0f, 90.0f, 60.0f };
    inline constexpr float powerOn[4]   = { 1400.0f, 127.0f, 70.0f, 60.0f };
    inline constexpr float range40[3]   = { 940.0f, 183.0f, 14.0f };   // LED rosso: center 40
    inline constexpr float range200[3]  = { 940.0f, 227.0f, 14.0f };   // LED verde: center 200
    inline constexpr float clipLed[2]    = { 1450.0f, 777.0f };   // LED di picco dell'OUTPUT finale
    inline constexpr float leverIn[2]    = { 493.0f, 500.0f };
    inline constexpr float leverLimit[2] = { 598.0f, 500.0f };
}
