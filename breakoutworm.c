#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <mmsystem.h>
#include <stdio.h>
#include <math.h>

#pragma region declarations
#define SAFE_RELEASE(p) if(p){ (p)->lpVtbl->Release(p); p = NULL; }

LPDIRECT3D9 d3d;
LPDIRECT3DDEVICE9 d3ddev;

const UINT SCREEN_WIDTH = 1366 / 2 ;
const UINT SCREEN_HEIGHT = 768 / 2;

typedef struct {
    float x, y, z, rhw;
    DWORD color;
} CUSTOMVERTEX;

const D3DCOLOR D3DCOLOR_BLACK = D3DCOLOR_XRGB(0, 0, 0);
const D3DCOLOR D3DCOLOR_WHITE = D3DCOLOR_XRGB(255, 255, 255);
const D3DCOLOR D3DCOLOR_GREY = D3DCOLOR_XRGB(64, 64, 64);
const D3DCOLOR D3DCOLOR_RED = D3DCOLOR_XRGB(255, 0, 0);
const D3DCOLOR D3DCOLOR_GREEN = D3DCOLOR_XRGB(255, 255, 0);
const D3DCOLOR D3DCOLOR_YELLOW = D3DCOLOR_XRGB(255, 255, 0);

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)

static
float midf(float a, float b, float c) {
    if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
    if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
    return c;
}

static
float zero_if_small(float x) {
    return (x > -1e-6f && x < 1e-6f) ? 0.0f : x;
}

struct rect {
    float x, y;
    float w, h;
};

struct box {
    float x1, y1;
    float x2, y2;
};

struct box rect_to_box(struct rect r) {
    struct box b = {
        .x1 = r.x,
        .y1 = r.y,
        .x2 = r.x + r.w,
        .y2 = r.y + r.h
    };
    return b;
}

struct rect box_to_rect(struct box b) {
    struct rect r = {
        .x = b.x1,
        .y = b.y1,
        .w = b.x2 - b.x1,
        .h = b.y2 - b.y1
    };
    return r;
}

int collision_rect(struct rect a, struct rect b) {
    return !(a.x + a.w <= b.x || b.x + b.w <= a.x ||
             a.y + a.h <= b.y || b.y + b.h <= a.y);
}

int collision_box(struct box a, struct box b) {
    return !(a.x2 <= b.x1 || b.x2 <= a.x1 ||
             a.y2 <= b.y1 || b.y2 <= a.y1);
}

float randf() {
    return rand() * (1.0f / RAND_MAX);
}

float exponential_decay(float length, float x) {
    return expf(-x / length);
}

#pragma endregion
#pragma region init & teardown

ID3DXFont* d3dfontArial = NULL;
// Initialize font (call this after device creation)
void InitFont(LPDIRECT3DDEVICE9 pDevice, ID3DXFont** d3dfont, char FaceName[]) {
    D3DXFONT_DESC fontDesc = {
        .Height = 16,
        .Width = 0,
        .Weight = FW_NORMAL,
        .MipLevels = 1,
        .Italic = FALSE,
        .CharSet = DEFAULT_CHARSET,
        .OutputPrecision = OUT_DEFAULT_PRECIS,
        .Quality = DEFAULT_QUALITY,
        .PitchAndFamily = DEFAULT_PITCH | FF_DONTCARE,
    };
    lstrcpy(fontDesc.FaceName, FaceName);
    HRESULT hr = D3DXCreateFontIndirect(pDevice, &fontDesc, d3dfont);
    if (FAILED(hr)) abort();
}

// Draw text (call this in render loop)
void DrawTextToScreen(LPDIRECT3DDEVICE9 pDevice, ID3DXFont* d3dfont, LPCSTR text, int x, int y, D3DCOLOR color) {
    if (! d3dfont) {
        printf("failed to DrawTextToScreen(%s, x:%d, y:%d, c:%lx)\n", text, x, y, color);
        return;
    }
    RECT rect = { x, y, x + 800, y + 600 };  // Set rect size as needed
    d3dfont->lpVtbl->DrawTextA(d3dfont, NULL, text, -1, &rect, DT_LEFT | DT_TOP, color);
}


static
void initD3D(HWND hWnd) {
    d3d = Direct3DCreate9(D3D_SDK_VERSION);

    D3DPRESENT_PARAMETERS d3dpp = {0};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hWnd;
    d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    d3dpp.BackBufferWidth = SCREEN_WIDTH;
    d3dpp.BackBufferHeight = SCREEN_HEIGHT;

    d3d->lpVtbl->CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                              D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                              &d3dpp, &d3ddev);

    d3ddev->lpVtbl->SetRenderState(d3ddev, D3DRS_ALPHABLENDENABLE, TRUE);
    d3ddev->lpVtbl->SetRenderState(d3ddev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    d3ddev->lpVtbl->SetRenderState(d3ddev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    d3ddev->lpVtbl->SetRenderState(d3ddev, D3DRS_BLENDOP, D3DBLENDOP_ADD);

    d3ddev->lpVtbl->SetFVF(d3ddev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

    InitFont(d3ddev, &d3dfontArial, "Arial");
}

static
void cleanD3D(void) {
    SAFE_RELEASE(d3ddev);
    SAFE_RELEASE(d3d);
}
#pragma endregion
#pragma region drawing functions
struct Vertex_XYZRHW_C {
    float x, y, z, rhw;
    D3DCOLOR color;
};

static
void line(IDirect3DDevice9 *device, float x1, float y1, float x2, float y2, D3DCOLOR color) {
    struct Vertex_XYZRHW_C vertices[2] = {
        { x1, y1, 0.0f, 1.0f, color },
        { x2, y2, 0.0f, 1.0f, color }
    };

    device->lpVtbl->DrawPrimitiveUP(device, D3DPT_LINELIST, 1, vertices, sizeof(struct Vertex_XYZRHW_C));
}

#define CIRCLE_SEGMENTS 32

static
void circfill(IDirect3DDevice9 *device, float x, float y, float r, D3DCOLOR color) {
    struct Vertex_XYZRHW_C vertices[CIRCLE_SEGMENTS + 2] = { 0 };
    float angle_step = (2.0f * D3DX_PI) / CIRCLE_SEGMENTS;

    // Center point
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0.0f;
    vertices[0].rhw = 1.0f;
    vertices[0].color = color;

    for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
        float angle = i * angle_step;
        float px = x + cosf(angle) * r;
        float py = y + sinf(angle) * r;

        vertices[i + 1].x = px;
        vertices[i + 1].y = py;
        vertices[i + 1].z = 0.0f;
        vertices[i + 1].rhw = 1.0f;
        vertices[i + 1].color = color;
    }

    device->lpVtbl->DrawPrimitiveUP(device, D3DPT_TRIANGLEFAN, CIRCLE_SEGMENTS, vertices, sizeof(struct Vertex_XYZRHW_C));
}

static
void circ(IDirect3DDevice9 *device, float x, float y, float r, float width, D3DCOLOR color) {
    // If the width is too wide, fill it.
    if (width > r) return circfill(device, x, y, r, color);

    struct Vertex_XYZRHW_C vertices[(CIRCLE_SEGMENTS+1) * 6] = { 0 };
    float angle_step = (2.0f * D3DX_PI) / CIRCLE_SEGMENTS;

    float inner_r = r - width;
    float outer_r = r;

    for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
        float angle = i * angle_step;
        float dx = cosf(angle);
        float dy = sinf(angle);
        float dx1 = cosf(angle + angle_step);
        float dy1 = sinf(angle + angle_step);

        /* 4 points, 2 triangles, so that makes 6 vertices
         * ABC + CDA
         *
         *   B      C
         *   +-----/|
         *   |  __/ |
         *   | /    |
         *   |/-----+
         *   A      D
         */

        struct Vertex_XYZRHW_C a = { .x = x + dx * inner_r, .y = y + dy * inner_r, .rhw = 1.0f, .color = color };
        struct Vertex_XYZRHW_C b = { .x = x + dx * outer_r, .y = y + dy * outer_r, .rhw = 1.0f, .color = color };
        struct Vertex_XYZRHW_C c = { .x = x + dx1 * outer_r, .y = y + dy1 * outer_r, .rhw = 1.0f, .color = color };
        struct Vertex_XYZRHW_C d = { .x = x + dx1 * inner_r, .y = y + dy1 * inner_r, .rhw = 1.0f, .color = color };

        // Outer vertex
        vertices[i * 6 + 0] = a;
        vertices[i * 6 + 1] = b;
        vertices[i * 6 + 2] = c;
        vertices[i * 6 + 3] = c;
        vertices[i * 6 + 4] = d;
        vertices[i * 6 + 5] = a;
    }

    device->lpVtbl->DrawPrimitiveUP(device, D3DPT_TRIANGLELIST, CIRCLE_SEGMENTS * 2, vertices, sizeof(struct Vertex_XYZRHW_C));
}

void rect_gradient(IDirect3DDevice9 *device, float x1, float y1, float x2, float y2, D3DCOLOR color_top, D3DCOLOR color_bottom) {
    struct Vertex_XYZRHW_C vertices[4] = {
        { x1, y1, 0.0f, 1.0f, color_top },  // top-left
        { x2, y1, 0.0f, 1.0f, color_top },  // top-right
        { x2, y2, 0.0f, 1.0f, color_bottom },  // bottom-right
        { x1, y2, 0.0f, 1.0f, color_bottom }   // bottom-left
    };

    device->lpVtbl->DrawPrimitiveUP(device, D3DPT_TRIANGLEFAN, 2, vertices, sizeof(struct Vertex_XYZRHW_C));
}

void rect(IDirect3DDevice9 *device, float x1, float y1, float x2, float y2, D3DCOLOR color) {
    rect_gradient(device, x1, y1, x2, y2, color, color);
}

// Particles
void points(IDirect3DDevice9 *device, float x[], float y[], int nb_points, D3DCOLOR color) {
    struct Vertex_XYZRHW_C vertices[1024];

    for (int i = 0; i < nb_points; i ++) {
        struct Vertex_XYZRHW_C v = { x[i], y[i], 0.0f, 1.0f, color };
        vertices[i] = v;
    }

    device->lpVtbl->DrawPrimitiveUP(device, D3DPT_POINTLIST, nb_points, vertices, sizeof(struct Vertex_XYZRHW_C));
}

#pragma endregion

#pragma region sfx

#define MAX_CHANNELS 16
static int channel_busy[MAX_CHANNELS] = { 0 };

// Find a free non-drum channel (not channel 9)
int alloc_channel(void) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (i == 9) continue; // ignore channel 9
        if (channel_busy[i]) continue;
        channel_busy[i] = 1;
        return i;
    }
    return -1; // none free
}

void free_channel(int ch) {
    channel_busy[ch] = 0;
}

typedef void (emit_sfx)(void);

DWORD midi_program_change(BYTE ch, BYTE program) {
#ifndef NDEBUG
    printf("[%lu] midi_program_change(ch:%d, program:%d)\n",  GetTickCount(), ch, program);
#endif
    return (program << 8) | (0xC0 | (ch & 0x0F));
}

DWORD midi_note_msg(BYTE on, BYTE ch, BYTE note, BYTE vel) {
#ifndef NDEBUG
    printf("[%lu] midi_note_msg(on:%d, ch:%d, note:%d, vel:%d)\n", GetTickCount(), on, ch, note, vel);
#endif
    return (vel << 16) | (note << 8) | ((on ? 0x90 : 0x80) | (ch & 0x0F));
}

#define VOLUME 100
#define VOLUME_SFX VOLUME
#define VOLUME_MAIN VOLUME
#define VOLUME_SECOND (VOLUME_MAIN - 16)


HMIDIOUT hmo;
void emit_ding_wall(void) {
    DWORD dwMsg = midi_note_msg(1, 9, 76, VOLUME_SFX);
#ifndef SOUND_DISABLED
    midiOutShortMsg(hmo, dwMsg);
#endif
}

void emit_ding_bottom(void) {
    DWORD dwMsg = midi_note_msg(1, 9, 66, VOLUME_SFX);
#ifndef SOUND_DISABLED
    midiOutShortMsg(hmo, dwMsg);
#endif
}

void emit_ding_paddle(void) {
    DWORD dwMsg = midi_note_msg(1, 9, 76, VOLUME_SFX);
#ifndef SOUND_DISABLED
    midiOutShortMsg(hmo, dwMsg);
#endif
}

void emit_ding_hole(void) {
    DWORD dwMsg = midi_note_msg(1, 9, 81, VOLUME_SFX);
#ifndef SOUND_DISABLED
    midiOutShortMsg(hmo, dwMsg);
#endif
}

void emit_ding_level(void) {
    DWORD dwMsg = midi_note_msg(1, 9, 45, VOLUME_SFX);
#ifndef SOUND_DISABLED
    midiOutShortMsg(hmo, dwMsg);
#endif
}

void initMidi(void) {
    if (midiOutOpen(&hmo, 0, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        fprintf(stderr, "Failed to open MIDI out\n");
        abort();
    }
}

#include <stdint.h>
#include <string.h>
#include <ctype.h>

int note_to_midi(const char *note_str) {
    static const int base_notes[7] = {
        9,  // A
        11, // B
        0,  // C
        2,  // D
        4,  // E
        5,  // F
        7   // G
    };

    if (!note_str || strlen(note_str) < 2)
        return -1;

    char note = toupper((unsigned char)note_str[0]);
    int note_index = -1;

    switch (note) {
        case 'C': note_index = 2; break;
        case 'D': note_index = 3; break;
        case 'E': note_index = 4; break;
        case 'F': note_index = 5; break;
        case 'G': note_index = 6; break;
        case 'A': note_index = 0; break;
        case 'B': note_index = 1; break;
        default: return -1;
    }

    int semitone = base_notes[note_index];

    size_t i = 1;
    if (note_str[i] == '#' || note_str[i] == 'b') {
        if (note_str[i] == '#') semitone++;
        else semitone--;
        i++;
    }

    int octave = 0;
    int sign = 1;
    if (note_str[i] == '-') {
        sign = -1;
        i++;
    }
    while (note_str[i] && isdigit((unsigned char)note_str[i])) {
        octave = octave * 10 + (note_str[i] - '0');
        i++;
    }
    octave *= sign;

    return (octave + 1) * 12 + semitone;
}

#define MAX_EVENTS 1024

enum event_state {
    new, playing, played,
};


typedef struct __attribute__((packed)) {
    DWORD32 start_time;
    DWORD32 duration;
    BYTE instrument;
    char note[4];
    BYTE volume;
    BYTE channel;
    enum event_state active;
    int is_drum;
} Event;

#define BEAT_MS (1000 * 60 / 80 / 4)
#define INSTRUMENT_MAIN 41
#define INSTRUMENT_SECOND 1

#define BEAT_MS_MEASURE (BEAT_MS * 24)

int event_count = 0;
Event events[] = {

#ifndef MAIN_MUSIC_DISABLED
// 1st voice A3G2 E3A2 G2        | E3D3  C3 z3
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 0, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "A2", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 3, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_MAIN, .note = "G2", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 5, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "E2", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 8, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_MAIN, .note = "A2", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 10, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "G2", .volume = VOLUME_MAIN },

    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 13, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "E2", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 16, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "D2", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 18, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "C2", .volume = VOLUME_MAIN },

    // Silence for 3 beats
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 0, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "A3", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 3, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_MAIN, .note = "G3", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 5, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "E3", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 8, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_MAIN, .note = "A3", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 10, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "G3", .volume = VOLUME_MAIN },

    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 13, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "E3", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 16, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "D3", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 18, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "C2", .volume = VOLUME_MAIN },

    // Silence for 3 beats
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 0, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "A3", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 3, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_MAIN, .note = "G3", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 5, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "E3", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 8, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_MAIN, .note = "A3", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 10, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "G3", .volume = VOLUME_MAIN },

    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 13, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "E3", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 16, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "D3", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 18, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "C3", .volume = VOLUME_MAIN },

    // Silence for 3 beats
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 0, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "A2", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 3, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_MAIN, .note = "G2", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 5, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "E2", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 8, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_MAIN, .note = "A2", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 10, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "G2", .volume = VOLUME_MAIN },

    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 13, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "E2", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 16, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "D2", .volume = VOLUME_MAIN },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 18, .duration = BEAT_MS * 3, .instrument = INSTRUMENT_MAIN, .note = "C2", .volume = VOLUME_MAIN },

#endif

#ifndef BASS_MUSIC_DISABLED
// 2nd voice C,2G,2E,2 G,2C,2G,2 | E,2G,2C,2 G,2E,2G,2
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 0, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "C2", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 2, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G2", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 4, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "E2", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 6, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G2", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 8, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "C2", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 10, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G2", .volume = VOLUME_SECOND },

    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 12, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "E2", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 14, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G2", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 16, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "C2", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 18, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G2", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 20, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "E2", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 0 + BEAT_MS * 22, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G2", .volume = VOLUME_SECOND },


    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 0, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "C1", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 2, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G1", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 4, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "E1", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 6, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G1", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 8, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "C1", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 10, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G1", .volume = VOLUME_SECOND },

    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 12, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "E1", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 14, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G1", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 16, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "C1", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 18, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G1", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 20, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "E1", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 1 + BEAT_MS * 22, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G1", .volume = VOLUME_SECOND },


    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 0, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "C4", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 2, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G4", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 4, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "E4", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 6, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G4", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 8, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "C4", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 10, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G4", .volume = VOLUME_SECOND },

    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 12, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "E4", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 14, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G4", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 16, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "C4", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 18, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G4", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 20, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "E4", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 2 + BEAT_MS * 22, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G4", .volume = VOLUME_SECOND },


    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 0, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "C3", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 2, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G3", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 4, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "E3", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 6, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G3", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 8, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "C3", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 10, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G3", .volume = VOLUME_SECOND },

    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 12, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "E3", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 14, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G3", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 16, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "C3", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 18, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G3", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 20, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "E3", .volume = VOLUME_SECOND },
    { .start_time = BEAT_MS_MEASURE * 3 + BEAT_MS * 22, .duration = BEAT_MS * 2, .instrument = INSTRUMENT_SECOND, .note = "G3", .volume = VOLUME_SECOND },
#endif

};

int mute = 0;

void updateMusic() {
    const int nb_events = sizeof(events) / sizeof(Event);
    static DWORD start = 0;

    if (mute) {
        if (mute == 1) for (int i = 0; i < MAX_CHANNELS; i ++) {
            midiOutShortMsg(hmo, midi_note_msg(0, i, 0, 0));
        }
        mute ++;
        start = 0;
        return;
    };

    if (!start) start = GetTickCount();
    DWORD now = GetTickCount() - start;

    // loop
    if (now > BEAT_MS_MEASURE * 4) {
        for (int i = 0; i < nb_events; i++) {
            events[i].active = new;
        }
        start += BEAT_MS_MEASURE * 4;
        now -= BEAT_MS_MEASURE * 4;
    }

    for (int i = 0; i < nb_events; i++) {
        Event *e = events + i;
        if (e->active == played) continue;

        int midi_note = note_to_midi(e->note);

        if (e->active == new && now > e->start_time) {
            e->active = playing;
            if (e->is_drum) {
                midiOutShortMsg(hmo, midi_note_msg(1, 9, midi_note, e->volume));
            } else {
                int ch = alloc_channel();
                if (ch < 0) {
                    continue; // skip note if no channel available
                }
                e->channel = ch;
                midiOutShortMsg(hmo, midi_program_change(e->channel, e->instrument));
                midiOutShortMsg(hmo, midi_note_msg(1, e->channel, midi_note, e->volume));
            }
        }

        if (e->active == playing && now > e->start_time + e->duration) {
            e->active = played;
            // Stop
            midiOutShortMsg(hmo, midi_note_msg(0, e->channel, midi_note, 0));
            free_channel(e->channel);
        }
    }
}

#pragma endregion

struct point {
    float x, y;
};

struct circle {
    struct point p;
    float r;
};

struct mover {
    struct circle c;
    struct point delta;
};

struct hole {
    struct circle c;
    UINT32 hole_dest; // if non zero, the hole is a wormhole, and this points to its destination.
};

#define HOLE_MAX 32
int nb_holes = HOLE_MAX;
struct hole holes[HOLE_MAX] = { 0 };

#define WORM_TAIL_SIZE 32
#define WORM_BODY_SIZE 8
#define WORM_RADIUS 10.
#define WORM_HOLE_PERCENT 30

struct worm {
    struct mover head;
    int num_elements;
    struct point tail[WORM_TAIL_SIZE];
};

struct worm w = { 0 };
struct mover p = { 0 };

/* Collide the mover m with the obstable o.
 * Returns the new mover m
 */
int collide_mover(struct mover *m, struct circle o, emit_sfx *sfx) {
    float nx = m->c.p.x - o.p.x;
    float ny = m->c.p.y - o.p.y;
    float len = sqrtf(nx * nx + ny * ny);
    if (len > m->c.r + o.r) return 0; // No collision

    if (len == 0.0f) return 0; // avoid division by zero

    nx /= len;
    ny /= len;

    float dot = m->delta.x * nx + m->delta.y * ny;

    m->delta.x -= 2.0f * dot * nx;
    m->delta.y -= 2.0f * dot * ny;

    // TODO - Move mover outside the obstacle
    float minlen = m->c.r + o.r;
    if (len < m->c.r + o.r) {
        m->c.p.x = o.p.x + nx * minlen;
        m->c.p.y = o.p.y + ny * minlen;
    }

    (*sfx)();

    return 1;
}

UINT level = 1;
UINT time = 0;
UINT cheat = 0;
UINT nb_fail = 0;
UINT nb_hits = 0;

enum pause {
    running,
    paused,
    pause_pressed,
    running_pressed,
} pause;

void add_hole(void) {
    if (nb_holes == HOLE_MAX) return; // no hold left

    struct hole h = {
        .c.p.x = randf() * (SCREEN_WIDTH - 100) + 50,
        .c.p.y = randf() * (SCREEN_HEIGHT - 100) + 50,
        .c.r = WORM_RADIUS
    };

    if (randf() < WORM_HOLE_PERCENT / 100. && nb_holes > 1) {
        int hole_dest = rand() % (nb_holes - 1);
        if (holes[hole_dest].hole_dest == 0) {  // only point to regular holes
            h.hole_dest = ~(rand() % (nb_holes - 1));
        }
    }

    holes[nb_holes++] = h;
}

#define MAX_PLAYER_SPEED 40
#define MAX_WORM_SPEED   10

static LARGE_INTEGER freq = {0};
static LARGE_INTEGER last_render_time = {0};

int should_render(float target_hz) {
    LARGE_INTEGER now;
    double target_dt = 1.0 / target_hz;

    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&last_render_time);
        return 1;
    }

    QueryPerformanceCounter(&now);
    double elapsed = (double)(now.QuadPart - last_render_time.QuadPart) / (double)freq.QuadPart;

    if (elapsed >= target_dt) {
        last_render_time = now;
        return 1;
    }

    return 0;
}

int nb_particles = 0;
struct mover particles[4096];
void update_particles(void) {
    for (int i = 0; i < nb_particles; i ++) {
        struct mover *p = particles + i;

        p->c.r *= 0.9;
            
        // gravity
        p->delta.y += 0.05f;
        p->c.p.x += p->delta.x;
        p->c.p.y += p->delta.y;
    }
}


static
void update(void) {
    // Exit on ESC
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
        exit(0);
    }

    if (GetAsyncKeyState(VK_PAUSE) & 0x8000 || GetAsyncKeyState('P') & 0x8000) {
        if (pause == running) pause = pause_pressed;
        if (pause == paused) pause = running_pressed;
    } else {
        if (pause == pause_pressed) pause = paused;
        if (pause == running_pressed) pause = running;
    }

    if (pause != running) return;

    if (! should_render(60)) return;

    time ++;

    float worm_speed = sqrt(w.head.delta.x * w.head.delta.x + w.head.delta.y * w.head.delta.y);
    if (worm_speed > MAX_WORM_SPEED) {
        w.head.delta.x *= MAX_WORM_SPEED / worm_speed;
        w.head.delta.y *= MAX_WORM_SPEED / worm_speed;
    }

   // --- Keyboard control (left/right arrows)
    p.delta.x *= 0.9;
    p.delta.x = zero_if_small(p.delta.x);

    if (GetAsyncKeyState(VK_DELETE) & 0x8000) {
        if (nb_holes > 1) nb_holes --;
        cheat ++;
        Sleep(60); // avoid multiples
    }

     if (GetAsyncKeyState('M') & 0x8000) {
        mute = mute ? 0 : 1;
        Sleep(100);
    }

    if (GetAsyncKeyState(VK_LEFT) & 0x8000)  p.delta.x -= 1.0f;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) p.delta.x += 1.0f;
    p.delta.x = midf(-MAX_PLAYER_SPEED, p.delta.x, MAX_PLAYER_SPEED);

    p.c.p.x += p.delta.x;
    p.c.p.x  = midf(0, p.c.p.x , SCREEN_WIDTH);

    memmove(w.tail + 1, w.tail, sizeof(struct point) * (WORM_TAIL_SIZE - 2));
    w.tail[0] = w.head.c.p;

    // Friction
    w.head.delta.x *= 1 - 0.0009f;
    w.head.delta.y *= 1 - 0.0009f;

    // gravity
    w.head.delta.y += 0.05f;

    w.head.c.p.x += w.head.delta.x;
    w.head.c.p.y += w.head.delta.y;


    if (w.head.c.p.x < 0 || w.head.c.p.x > SCREEN_WIDTH) {
        w.head.delta.x = - w.head.delta.x;
        w.head.c.p.x += 2 * w.head.delta.x;
        emit_ding_wall();
    }

    if (w.head.c.p.y < 0 ) {
        w.head.delta.y = - w.head.delta.y;
        w.head.c.p.y += 2 * w.head.delta.y;
        emit_ding_wall();
    }

    if (w.head.c.p.y > SCREEN_HEIGHT) {
        w.head.delta.y = - 2 * w.head.delta.y;
        w.head.c.p.y += 2 * w.head.delta.y;

        // Add new holes
        for (int i = 0; i < 2; i++) {
            add_hole();
        }

        nb_fail ++;
        emit_ding_bottom();
    }

    // Collide Worm / Player
    if (collide_mover(&w.head, p.c, &emit_ding_paddle)) {
        w.head.delta.x *= 1.1;
        w.head.delta.y *= 1.1;
    }

    // Collide Worm / Hole
    for (int i = 0; i < nb_holes; i ++) {
        struct mover old_head = w.head;
        int collide = collide_mover(&w.head, holes[i].c, &emit_ding_hole);
        if (collide) {
            // Warp to destination if wormhole
            if (holes[i].hole_dest) {
                // Restore the old head
                w.head = old_head;
                int hole_dest = ~(holes[i].hole_dest);
#if 0
                w.head.c.p.x += holes[hole_dest].c.p.x - holes[i].c.p.x;
                w.head.c.p.y += holes[hole_dest].c.p.y - holes[i].c.p.y;
#else
                w.head.c.p.x = holes[hole_dest].c.p.x;
                w.head.c.p.y = holes[hole_dest].c.p.y;
#endif
                continue;
            }

            nb_hits ++;

            // normal hole, destroy it
            nb_holes --;
            holes[i] = holes[nb_holes];

            // destroy any wormhole that points to it
            for (int j = 0; j < nb_holes; j ++) {
                if (holes[j].hole_dest == ~i) holes[j].hole_dest = 0;
            }
        }
    }

    if (nb_holes == 0) {
        Sleep(60);
        emit_ding_level();
        level ++;
        for (int i = 0; i < level; i ++) {
            add_hole();
        }
    }
}

static
void renderFrame(void) {
    d3ddev->lpVtbl->Clear(d3ddev, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.0f, 0);
    d3ddev->lpVtbl->BeginScene(d3ddev);

    // outside borders
    rect(d3ddev, 0, 0, 2, SCREEN_HEIGHT, D3DCOLOR_WHITE);
    rect(d3ddev, SCREEN_WIDTH - 2, 0, SCREEN_WIDTH, SCREEN_HEIGHT, D3DCOLOR_WHITE);
    rect(d3ddev, 0, 0, SCREEN_WIDTH, 2, D3DCOLOR_WHITE);

    // Bottom pit
    rect_gradient(d3ddev, 0, SCREEN_HEIGHT - 16, SCREEN_WIDTH, SCREEN_HEIGHT, D3DCOLOR_RED & 0x00FFFFFF, D3DCOLOR_RED & 0x55FFFFFF);

    // Player
    circ(d3ddev, p.c.p.x, p.c.p.y, p.c.r, 2, D3DCOLOR_RED);

    // Holes
    for (int i = 0; i < nb_holes; i ++) {
        D3DCOLOR c = D3DCOLOR_WHITE;
        if (holes[i].hole_dest) {
            // it is a wormhole
            c = D3DCOLOR_GREY;
            int hole_dest = ~(holes[i].hole_dest);
            line(d3ddev, holes[i].c.p.x, holes[i].c.p.y, holes[hole_dest].c.p.x, holes[hole_dest].c.p.y, c);
        }
        circfill(d3ddev, holes[i].c.p.x, holes[i].c.p.y, holes[i].c.r, c);
    }
    // Worm
    circfill(d3ddev, w.head.c.p.x, w.head.c.p.y, w.head.c.r, D3DCOLOR_GREEN);
    for (int i = 0; i < WORM_TAIL_SIZE; i ++) {
        if (i < WORM_BODY_SIZE) {
            circfill(d3ddev, w.tail[i].x, w.tail[i].y, w.head.c.r, D3DCOLOR_GREEN);
        }

        const D3DCOLOR D3DCOLOR_GREEN_TRANSPARENT = D3DCOLOR_RGBA(0, 255, 0, 32);
        circfill(d3ddev, w.tail[i].x, w.tail[i].y, w.head.c.r * exponential_decay(WORM_TAIL_SIZE - WORM_BODY_SIZE, i - WORM_BODY_SIZE), D3DCOLOR_GREEN_TRANSPARENT);

    }

    // HUD
    {
        char msg[256] = { 0 };
        sprintf(msg, "Level: %2u Time: %8u Left: %2u Hits: %4u Fails: %4u", level, time, nb_holes, nb_hits, nb_fail);
        DrawTextToScreen(d3ddev, d3dfontArial, msg, 14, 14, D3DCOLOR_WHITE);

        if (cheat) {
            char msg[256] = { 0 };
            sprintf(msg, "Cheat: %08u", cheat);
            DrawTextToScreen(d3ddev, d3dfontArial, msg, SCREEN_WIDTH - 100, 14, D3DCOLOR_RED);
        }
    }

    d3ddev->lpVtbl->EndScene(d3ddev);
    d3ddev->lpVtbl->Present(d3ddev, NULL, NULL, NULL, NULL);
}



static
void initGame(void) {
    p.c.p.x = SCREEN_WIDTH / 2;
    p.c.r = 100;

    p.c.p.y = SCREEN_HEIGHT + p.c.r / 2;

    p.delta.x = 0;

    w.head.c.r = WORM_RADIUS;
    w.head.delta.x = .5;
    w.head.delta.y = 1. * SCREEN_HEIGHT / SCREEN_WIDTH;

    w.head.delta.x *= 7;  w.head.delta.y  *= 7;

    nb_holes = 0;
    for (int i = 0; i < level; i ++) {
        add_hole();
    }
}

static
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = {0};
    wc.hInstance = hInstance;
    wc.lpszClassName = "WindowClass";
    wc.lpfnWndProc = WindowProc;
    RegisterClass(&wc);

    HWND hWnd = CreateWindow("WindowClass", "Breakout Worm DX9", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, SCREEN_WIDTH, SCREEN_HEIGHT, NULL, NULL, hInstance, NULL);
    ShowWindow(hWnd, nCmdShow);

    initD3D(hWnd);
    initMidi();

    initGame();

    MSG msg = {0};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            updateMusic();
            update();
            renderFrame();
        }
    }

    cleanD3D();
    return 0;
}

__attribute__((weak))
void __cdecl mainCRTStartup(void) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    LPSTR cmdLine = GetCommandLineA();
    int nCmdShow = SW_SHOWDEFAULT;

    WinMain(hInstance, NULL, cmdLine, nCmdShow);
    ExitProcess(0);
}
