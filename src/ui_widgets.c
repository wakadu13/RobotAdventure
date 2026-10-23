/*
 * BOUCLIER 2050 - palette et primitives d'interface (texte, boutons, infobulles, barres).
 */
#include "ui.h"

/* ------------------------------------------------------------------ */
/* Palette                                                             */
/* ------------------------------------------------------------------ */

const Color DOMAIN_COLORS[D_COUNT] = {
    { 150, 170,  90, 255 }, {  70, 150, 230, 255 }, { 120, 210, 240, 255 }, { 200, 160, 255, 255 },
    {  80, 230, 170, 255 }, { 250, 200,  80, 255 }, { 250, 120, 200, 255 },
};

const Color FACTION_COLORS[F_COUNT] = {
    { 200, 100, 100, 255 }, { 230, 180,  90, 255 }, {  80, 200, 200, 255 }, { 220, 100, 230, 255 },
};

const Color IDEOLOGY_COLORS[I_COUNT] = {
    {  90, 190, 100, 255 }, { 220,  80,  90, 255 }, { 240, 200,  70, 255 },
    {  80, 130, 220, 255 }, { 120,  80, 180, 255 }, { 120, 120, 120, 255 },
};

/* ------------------------------------------------------------------ */
/* Primitives d'interface                                              */
/* ------------------------------------------------------------------ */
void Text(const char *s, float x, float y, int size, Color c)
{
    /* positions entieres : la police pixel se deforme sur des coordonnees fractionnaires */
    s = Tr(s);
    DrawTextEx(GetFontDefault(), s, (Vector2){ floorf(x), floorf(y) }, (float)size, size / 10.0f, c);
}

int TextW(const char *s, int size)
{
    s = Tr(s);
    return (int)MeasureTextEx(GetFontDefault(), s, (float)size, size / 10.0f).x;
}

int TextWrap(const char *s, float x, float y, float w, int size, Color c)
{
    s = Tr(s);
    char line[256], word[128];
    int lineLen = 0, lines = 0;
    line[0] = 0;
    const char *p = s;
    while (*p) {
        int wl = 0;
        while (*p == ' ') p++;
        while (*p && *p != ' ' && wl < 127) word[wl++] = *p++;
        word[wl] = 0;
        if (!wl) break;
        char test[256];
        snprintf(test, sizeof(test), "%s%s%s", line, lineLen ? " " : "", word);
        if (lineLen && TextW(test, size) > w) {
            Text(line, x, y + lines * (size + 4), size, c);
            lines++;
            snprintf(line, sizeof(line), "%s", word);
        } else {
            snprintf(line, sizeof(line), "%s", test);
        }
        lineLen = (int)strlen(line);
    }
    if (lineLen) { Text(line, x, y + lines * (size + 4), size, c); lines++; }
    return lines * (size + 4);
}

bool Hover(Rectangle r)
{
    return inputLayer == activeLayer && CheckCollisionPointRec(mouse, r);
}

void Panel(Rectangle r, Color fill)
{
    DrawRectangleRec(r, fill);
    DrawRectangleLinesEx(r, 1, C_BORDER);
}

bool Button(Rectangle r, const char *label, bool enabled, int size)
{
    bool h = enabled && Hover(r);
    Color fill = !enabled ? (Color){ 30, 36, 44, 255 } : h ? (Color){ 52, 82, 120, 255 } : C_PANEL2;
    DrawRectangleRec(r, fill);
    DrawRectangleLinesEx(r, 1, enabled ? (h ? C_ACCENT : C_BORDER) : (Color){ 45, 52, 62, 255 });
    int tw = TextW(label, size);
    Text(label, r.x + (r.width - tw) / 2, r.y + (r.height - size) / 2, size, enabled ? C_TEXT : C_DIM);
    if (h && clicked) { clicked = false; Snd_Play(SFX_CLICK); return true; }
    return false;
}

void Tooltip(Rectangle r, const char *text)
{
    if (Hover(r)) snprintf(tooltip, sizeof(tooltip), "%s", Tr(text));
}

/* bouton avec infobulle et raison d'indisponibilite */
bool ActionButton(Rectangle r, const char *label, const char *blocked, const char *desc)
{
    bool res = Button(r, label, blocked == NULL, 10);
    char tip[500];
    snprintf(tip, sizeof(tip), "%s\n%s%s%s", Tr(label), Tr(desc), blocked ? Tr("\nIndisponible : ") : "", blocked ? Tr(blocked) : "");
    Tooltip(r, tip);
    return res;
}

/* selecteur radio : renvoie true si clique */
bool Choice(Rectangle r, const char *label, bool sel)
{
    bool h = Hover(r);
    DrawRectangleRec(r, sel ? (Color){ 40, 62, 92, 255 } : h ? C_PANEL2 : C_PANEL);
    DrawRectangleLinesEx(r, 1, sel ? C_ACCENT : C_BORDER);
    Text(label, r.x + (r.width - TextW(label, 10)) / 2, r.y + (r.height - 10) / 2, 10, sel ? C_TEXT : C_DIM);
    if (h && clicked) { clicked = false; Snd_Play(SFX_CLICK); return true; }
    return false;
}

void Bar(Rectangle r, float v, float max, Color c)
{
    DrawRectangleRec(r, (Color){ 10, 12, 16, 255 });
    float w = r.width * (v < 0 ? 0 : (v > max ? 1 : v / max));
    DrawRectangle((int)r.x, (int)r.y, (int)w, (int)r.height, c);
    DrawRectangleLinesEx(r, 1, C_BORDER);
}

float Clampf2(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

Color LevelColor(float v)
{
    if (v < 20) return C_BAD;
    if (v < 40) return C_WARN;
    return C_GOOD;
}

Color MilColor(float m) { return m > 6 ? C_BAD : m > 4 ? C_WARN : C_GOOD; }

Color NationColor(int n)
{
    const unsigned char *c = NATIONS[n].color;
    return (Color){ c[0], c[1], c[2], 255 };
}

Color Shade(Color c, float k)
{
    return (Color){ (unsigned char)fminf(255, c.r * k), (unsigned char)fminf(255, c.g * k),
                    (unsigned char)fminf(255, c.b * k), 255 };
}

Color Lerp3(Color a, Color b, Color c, float t)
{
    a = CbColor(a); b = CbColor(b); c = CbColor(c);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    Color x = t < 0.5f ? a : b, y = t < 0.5f ? b : c;
    float u = t < 0.5f ? t * 2 : (t - 0.5f) * 2;
    return (Color){ (unsigned char)(x.r + (y.r - x.r) * u), (unsigned char)(x.g + (y.g - x.g) * u),
                    (unsigned char)(x.b + (y.b - x.b) * u), 255 };
}

void Toast(const char *msg)
{
    snprintf(toast, sizeof(toast), "%s", msg);
    toastTime = 3.0f;
}
