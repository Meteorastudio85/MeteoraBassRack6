# Meteora Bass Rack

Plugin all-in-one per basso di **Meteora Plugins** (VST3 / AU / Standalone).

Catena del segnale (oversampling 4x):
**EQ 570b → LA-2A → Tube Smile → VOG → polarità → OUTPUT finale**

## Compilare con GitHub Actions
1. Carica tutta questa cartella in un repository GitHub (account Meteorastudio85).
2. Il workflow `.github/workflows/build.yml` parte da solo a ogni push
   (oppure da *Actions → Build Meteora Bass Rack → Run workflow*).
3. A fine build scarichi gli artefatti `MeteoraBassRack-Windows` e `MeteoraBassRack-macOS`
   (dentro trovi `VST3`, `AU` su Mac e `Standalone`).

JUCE 8.0.4 viene scaricato in automatico, non serve installarlo.

## Comandi
| Modulo | Comando | Note |
|---|---|---|
| EQ 570b | manopola (indice a torre) | frequenza, 7 scatti reali per banda |
| | cappuccio bianco | guadagno ±12 dB a scatti di 2 dB, Q proporzionale |
| | tasto destro | menu frequenze + campana/shelf (bande 1 e 4) |
| | IN | EQ inserito (LED rosso) |
| LA-2A | IN / OFF, LIMIT / COMP | levette |
| | INPUT, PEAK REDUCTION | pomelli fotografati che ruotano davvero (indice e zigrinatura girano, la luce resta ferma) |
| | PEAK REDUCTION | quantità di compressione (cella ottica T4B) |
| | INPUT | livello che entra nel compressore (5 = 0 dB, ±20 dB), grande quanto PEAK REDUCTION |
| Tube Smile | INPUT, OUTPUT, DRIVE, BLEND | circuito TS808; BLEND mescola pulito/distorto, OUTPUT è il volume di tutto il pedale (5 = 0 dB); footswitch = on/off |
| VOG | AMPLITUDE, FREQUENCY, Q | risonanza sulle basse |
| | LED 40 / 200 | clic sul LED per scegliere il range (20–80 Hz / 100–400 Hz) |
| | in, hp, 24dB, ø | VOG inserito, taglio sotto la risonanza, pendenza, polarità |
| Alimentatore | OFF / ON | bypass totale; lampade, VU e logo sono accesi solo con ON e plugin attivo nella DAW |
| OUTPUT (modulo in basso a destra) | OUTPUT final | volume finale di tutta la catena: tutto a sinistra = silenzio, fino a +12 dB (0 dB a ore 2 circa); LED rosso = picco sopra −0,5 dBFS |

Doppio clic su un pomello = valore iniziale. La finestra si apre a 960×587
(un quarto di uno schermo 1920×1080) e si ridimensiona trascinando l'angolo.

## Cartelle
- `Source/Dsp` — algoritmi audio (trasformatori, cella ottica, TS808, VOG)
- `Source/PluginProcessor.*` — parametri e catena audio
- `Source/PluginEditor.*`, `Source/Layout.h` — interfaccia
- `Assets` — grafica (3x) e coordinate degli elementi (`layout.json`)
- `tools/bake_assets.py` — script che genera la grafica da `tools/rack.jpg`, `tools/tube_smile.jpg` e `tools/logo_meteora.png`
