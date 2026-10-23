#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include <array>
#include <random>
#include <vector>

enum class PropKind { pitch = 0, adsr = 1, effect = 2 };

struct Prop
{
    PropKind kind = PropKind::pitch;
    float value = 60.0f;
};

enum class ScaleMode : int
{
    Free = 0,
    Major,
    Minor,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Locrian,
    HarmonicMinor,
    MelodicMinor,
    MajorPentatonic,
    MinorPentatonic,
    Blues,
    WholeTone,
    Diminished,
    Chromatic,
    JustMajor,
    JustMinor,
    Edo24,
    Edo31,
    Edo48,
    Edo72,
    Count
};

const char* scaleName (int mode);

// One musical event produced by a collision. This is the integration boundary
// for downstream instruments/sequencers:
//   note / velocity  -> MIDI note and velocity
//   gateSeconds      -> how long the note should be held (can be changed by ADSR balls)
//   pan              -> -1..1 stereo position
//   send             -> FX / reverb send CV, 0..1 (also exposed as the REVERB macro)
//   waveform         -> oscillator waveform CV, 0..1 (0 = sine, 1 = square)
//   attack..release  -> ADSR envelope CV
struct SoundEvent
{
    float note = 60.0f;
    float pan = 0.0f;
    float send = 0.0f;
    float velocity = 0.5f;
    float gateSeconds = 0.08f;
    float waveform = 0.0f;
    float attack = 0.005f;
    float decay = 0.15f;
    float sustain = 0.6f;
    float release = 0.3f;
};

struct WorldSettings
{
    float ballScale = 0.55f;
    float ballSpeed = 1.0f;
    float speedShare = 1.0f;
    float speedStrength = 1.0f;
    float speedCap = 280.0f;
    float waveSpeed = 0.65f;
    float waveRange = 0.5f;
    float pitchRange = 4.0f;
    int scaleMode = 0;

    float defaultGate = 0.08f;
    float gateMin = 0.03f;
    float gateMax = 0.50f;

    float baseWaveform = 0.0f;
    float baseReverb = 0.0f;

    int ballCount = 12;
    int boosterCount = 2;

    std::array<float, 3> ratio { 0.50f, 0.25f, 0.25f };
};

struct BallVis
{
    juce::Point<float> pos;
    float radius = 6.0f;
    PropKind kind = PropKind::pitch;
    float value = 60.0f;
    float note = 60.0f;
    float flash = 0.0f;
    float phase = 0.0f;
};

struct SegmentVis
{
    juce::Point<float> a, b;
    bool hasProp = false;
    PropKind kind = PropKind::pitch;
    float value = 60.0f;
    float strength = 0.0f;
};

struct FieldVis
{
    juce::Point<float> pos;
    PropKind kind = PropKind::pitch;
    float value = 60.0f;
    float radius = 100.0f;
    float strength = 1.0f;
};

struct WaveVis
{
    juce::Point<float> pos;
    float radius = 0.0f;
    float amp = 1.0f;
    PropKind kind = PropKind::effect;
};

struct BoosterVis
{
    juce::Point<float> pos;
    float radius = 13.0f;
    float cooldown = 0.0f;
    float phase = 0.0f;
};

struct LinkVis
{
    juce::Point<float> a, b;
    float amount = 0.0f;
};

struct Snapshot
{
    std::vector<BallVis> balls;
    std::vector<SegmentVis> segments;
    std::vector<FieldVis> fields;
    std::vector<WaveVis> waves;
    std::vector<BoosterVis> boosters;
    std::vector<LinkVis> links;
    std::array<int, 3> counts { 0, 0, 0 };
    float averageSpeed = 0.0f;
    float elapsed = 0.0f;
    bool paused = false;
    int shape = 0;
};

class World
{
public:
    static constexpr float fieldX = 24.0f;
    static constexpr float fieldY = 92.0f;
    static constexpr float fieldW = 1532.0f;
    static constexpr float fieldH = 666.0f;

    World();

    void reset();
    void setShape (int newShape);
    void setFieldScale (float scale);
    void setBallScale (float scale);
    void setPitchRange (float octaves);
    void setScale (int mode);
    void setDefaultGate (float seconds);
    void setGateRange (float minSeconds, float maxSeconds);
    void setBaseWaveform (float value) { settings.baseWaveform = juce::jlimit (0.0f, 1.0f, value); }
    void setBaseReverb (float value) { settings.baseReverb = juce::jlimit (0.0f, 1.0f, value); }
    void setBallCount (int count);
    void setBoosterCount (int count);
    void setSettings (const WorldSettings& s) { settings = s; }
    const WorldSettings& getSettings() const { return settings; }
    void clearEffects();
    void addBall() { spawnBall(); }
    bool addBallAt (juce::Point<float> pos, int kind = -1);
    void addBooster() { spawnBooster(); }

    void update (float dt);
    void fillSnapshot (Snapshot& snap) const;

    std::vector<SoundEvent>& getEvents() { return events; }

    int shape = 0;
    float fieldScale = 1.0f;
    bool paused = false;

    juce::Point<float> randomPoint (float margin = 0.0f) const;
    bool inside (juce::Point<float> p, float margin = 0.0f) const;

private:
    struct Segment
    {
        juce::Point<float> a, b, n, mid;
        float length = 0.0f;
        bool hasProp = false;
        Prop prop;
        float strength = 0.0f;
    };

    struct Ball
    {
        juce::Point<float> pos, vel;
        float radius = 6.0f;
        float r0 = 6.0f;
        Prop prop;
        float note = 60.0f;
        float wallCd = 0.0f;
        float ballCd = 0.0f;
        float flash = 0.0f;
        std::array<float, 3> waveAmp { 0.0f, 0.0f, 0.0f };
        std::array<float, 3> waveTimer { 0.0f, 0.0f, 0.0f };
        float phase = 0.0f;
        float gate = 0.08f;
        bool gateOverridden = false;
        int uid = 0;
    };

    struct Field
    {
        juce::Point<float> pos;
        Prop prop;
        float radius = 100.0f;
        float strength = 1.0f;
        float age = 0.0f;
        float life = 6.0f;
    };

    struct Wave
    {
        juce::Point<float> pos;
        float radius = 6.0f;
        float speed = 250.0f;
        float amp = 1.0f;
        Prop prop;
        int generation = 0;
        bool reflected = false;
        float age = 0.0f;
        std::vector<int> hitUids;
    };

    struct Booster
    {
        juce::Point<float> pos;
        float radius = 13.0f;
        float cooldown = 0.0f;
        float phase = 0.0f;
    };

    struct Spark
    {
        juce::Point<float> pos, vel;
        float life = 0.3f;
        float maxLife = 0.3f;
        PropKind kind = PropKind::pitch;
        float value = 60.0f;
    };

    struct Link
    {
        juce::Point<float> a, b;
        float amount = 0.0f;
    };

    void buildBoundary();
    void spawnBall (int forcedKind = -1, juce::Point<float> forcedPos = {});
    void ensureTypes();
    void spawnBooster (juce::Point<float> forcedPos = {});
    void addField (juce::Point<float> pos, const Prop& prop, float radius, float life = 6.0f);
    void addWave (juce::Point<float> pos, const Prop& prop, float amp, int generation = 0);
    void addSparks (juce::Point<float> pos, const Prop& prop, int count, float speed);
    void bounceOffWall (Ball& ball);
    void ensureInside (Ball& ball);
    void shareSpeeds (float dt);
    void rebalanceType (Ball& ball);
    void enforceTypePresence();
    void collideBalls();
    void updateWaves (float dt);
    void updateEffects (float dt);

    juce::Point<float> closestOnSegment (juce::Point<float> p, const Segment& s) const;
    float influenceAt (juce::Point<float> p, PropKind kind) const;
    float effectiveNote (const Ball& b) const;
    std::array<float, 3> targetRatios() const;
    Prop randomProp (PropKind kind) const;
    float randomNote() const;
    float randomGate();
    float quantizeNote (float note) const;
    void rebuildScaleTable();
    void giveGate (Ball& ball);
    void emitSound (const Ball& b, const Prop& oldProp, float oldNote, float impact);
    void swapWall (Ball& ball, Segment& seg, juce::Point<float> contact, float impact);
    static int kindIndex (PropKind k) { return static_cast<int> (k); }

    std::vector<Segment> segments;
    std::vector<juce::Point<float>> boundaryPoints;
    std::vector<Ball> balls;
    std::vector<Field> fields;
    std::vector<Wave> waves;
    std::vector<Booster> boosters;
    std::vector<Spark> sparks;
    std::vector<Link> links;
    std::vector<SoundEvent> events;
    std::vector<float> scaleTable;

    WorldSettings settings;
    mutable std::mt19937 rng;
    float elapsed = 0.0f;
    float typeGuardTimer = 0.0f;
    float wallGraceTimer = 0.0f;
    int nextUid = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (World)
};
