/*
 * BOUCLIER 2050 - sons synthetises au lancement.
 *
 * Chaque effet est calcule echantillon par echantillon (oscillateurs,
 * bruit, enveloppes) puis charge comme un Sound raylib : le jeu ne depend
 * d'aucun fichier audio. Sans peripherique audio (serveur, CI), tout est
 * silencieusement desactive.
 */
#include "sound.h"

#include "raylib.h"

#include <math.h>
#include <stdlib.h>

#define RATE 22050
#define PI_F 3.14159265f

static Sound sfx[SFX_COUNT];
static Sound ambient;
static bool  ready, enabled = true;
static unsigned noiseSeed = 1234567u;

static float Noise(void)
{
    noiseSeed = noiseSeed * 1664525u + 1013904223u;
    return ((noiseSeed >> 9) & 0x7FFF) / 16384.0f - 1.0f;
}

static float Env(float t, float attack, float decay, float len)
{
    if (t < attack) return t / attack;
    float r = (len - t) / decay;
    return r < 1 ? (r < 0 ? 0 : r) : 1;
}

typedef float (*SampleFn)(float t, float len);

static Sound Make(float len, SampleFn fn, float volume)
{
    int n = (int)(len * RATE);
    short *data = (short *)malloc(sizeof(short) * (size_t)n);
    for (int i = 0; i < n; i++) {
        float v = fn((float)i / RATE, len) * volume;
        if (v > 1) v = 1;
        if (v < -1) v = -1;
        data[i] = (short)(v * 32000);
    }
    Wave w = { (unsigned)n, RATE, 16, 1, data };
    Sound s = LoadSoundFromWave(w);
    free(data);
    return s;
}

static float Sq(float x) { return sinf(x) > 0 ? 1.0f : -1.0f; }

static float FnClick(float t, float len)  { return sinf(2 * PI_F * 1400 * t) * Env(t, 0.002f, 0.03f, len) * 0.5f; }
static float FnTurn(float t, float len)
{
    float f = 220 + 440 * t / len;
    return (sinf(2 * PI_F * f * t) * 0.6f + Noise() * 0.15f) * Env(t, 0.05f, 0.25f, len);
}
static float FnAlert(float t, float len)
{
    float f = fmodf(t, 0.3f) < 0.15f ? 880 : 660;
    return Sq(2 * PI_F * f * t) * 0.25f * Env(t, 0.01f, 0.1f, len);
}
static float FnWar(float t, float len)
{
    /* trois coups de tambour graves */
    float b = fmodf(t, 0.25f);
    float drum = sinf(2 * PI_F * (60 + 80 * expf(-b * 30)) * b) * expf(-b * 9);
    return (drum + Noise() * 0.2f * expf(-b * 25)) * Env(t, 0.005f, 0.1f, len);
}
static float FnGood(float t, float len)
{
    float f = t < len / 2 ? 523.25f : 783.99f;
    return (sinf(2 * PI_F * f * t) + 0.3f * sinf(4 * PI_F * f * t)) * 0.4f * Env(fmodf(t, len / 2), 0.01f, 0.2f, len / 2);
}
static float FnBad(float t, float len)
{
    float f = 196 - 60 * t / len;
    return (Sq(2 * PI_F * f * t) * 0.2f + sinf(2 * PI_F * f * 0.5f * t) * 0.4f) * Env(t, 0.02f, 0.3f, len);
}
static float FnNuke(float t, float len)
{
    float rumble = sinf(2 * PI_F * 35 * t) * 0.5f;
    return (Noise() * 0.8f * expf(-t * 1.5f) + rumble * expf(-t * 0.8f)) * Env(t, 0.01f, 1.0f, len);
}
static float FnVictory(float t, float len)
{
    static const float notes[4] = { 392.0f, 523.25f, 659.25f, 783.99f };
    int k = (int)(t / 0.3f);
    if (k > 3) k = 3;
    float f = notes[k];
    return (sinf(2 * PI_F * f * t) * 0.5f + sinf(2 * PI_F * f * 1.5f * t) * 0.15f) * Env(t, 0.02f, 0.6f, len);
}
static float FnDefeat(float t, float len)
{
    static const float notes[3] = { 392.0f, 311.13f, 261.63f };
    int k = (int)(t / 0.45f);
    if (k > 2) k = 2;
    return sinf(2 * PI_F * notes[k] * t) * 0.5f * Env(t, 0.02f, 0.8f, len);
}
/* nappe d'ambiance : accord grave qui ondule lentement, boucle sans couture */
static float FnAmbient(float t, float len)
{
    float lfo = 0.5f + 0.5f * sinf(2 * PI_F * t / len);
    float v = sinf(2 * PI_F * 55 * t) * 0.5f + sinf(2 * PI_F * 82.5f * t) * 0.3f * lfo + sinf(2 * PI_F * 110 * t) * 0.15f * (1 - lfo);
    return v * 0.35f;
}

void Snd_Init(void)
{
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    sfx[SFX_CLICK]   = Make(0.05f, FnClick, 0.6f);
    sfx[SFX_TURN]    = Make(0.45f, FnTurn, 0.5f);
    sfx[SFX_ALERT]   = Make(0.6f, FnAlert, 0.9f);
    sfx[SFX_WAR]     = Make(0.75f, FnWar, 0.8f);
    sfx[SFX_GOOD]    = Make(0.5f, FnGood, 0.5f);
    sfx[SFX_BAD]     = Make(0.6f, FnBad, 0.5f);
    sfx[SFX_NUKE]    = Make(3.0f, FnNuke, 0.75f);
    sfx[SFX_VICTORY] = Make(1.6f, FnVictory, 0.6f);
    sfx[SFX_DEFEAT]  = Make(1.8f, FnDefeat, 0.6f);
    ambient = Make(8.0f, FnAmbient, 0.25f);
    ready = true;
}

void Snd_Close(void)
{
    if (!ready) return;
    for (int i = 0; i < SFX_COUNT; i++) UnloadSound(sfx[i]);
    UnloadSound(ambient);
    CloseAudioDevice();
    ready = false;
}

void Snd_Play(Sfx s)
{
    if (ready && enabled && s >= 0 && s < SFX_COUNT) PlaySound(sfx[s]);
}

void Snd_Update(bool on)
{
    if (!ready) return;
    bool want = on && enabled;
    if (want && !IsSoundPlaying(ambient)) PlaySound(ambient);
    if (!want && IsSoundPlaying(ambient)) StopSound(ambient);
}

void Snd_SetEnabled(bool on) { enabled = on; }
bool Snd_Enabled(void) { return enabled; }
