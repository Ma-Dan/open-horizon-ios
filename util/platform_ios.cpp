//
// open horizon -- undefined_darkness@outlook.com
// iPhone platform backend: implements the `platform` interface on top of the
// nya-engine app.mm lifecycle (EAGL/ES2 + CADisplayLink on the main thread).
//
// GLFW is NOT linked on iOS: only its key CODE values are needed as the
// shared "virtual key" space (OH_KEY_* in platform.h), so demo.cpp's control
// mapping compiles unchanged. Touch events arrive via
// nya_system::app::on_touch (routed by app.mm) and are translated here into
// analog stick/throttle state plus synthetic held keys.
//

#include "platform.h"

#ifdef OH_PLATFORM_IOS

#include <cstring>
#include <cmath>

namespace
{

enum touch_zone
{
    Z_NONE = 0,
    Z_STICK,
    Z_THROTTLE,
    Z_MSL,
    Z_MGUN,
    Z_TGT,
    Z_WPN,
    Z_BACK
};

struct touch_slot
{
    bool active;
    int x, y;        //current position
    int ox, oy;      //stick origin
    touch_zone zone;
};

touch_slot g_touches[8];

float g_stick_x = 0.0f;  //-1..1
float g_stick_y = 0.0f;  //-1..1, positive = finger pulled down (climb)
float g_throttle = 0.0f; //0..1
bool g_brake = false;

int g_frame_w = 0, g_frame_h = 0;

const float dead_zone = 0.15f;

touch_zone zone_at(int x, int y, int w, int h)
{
    const float m = float(w < h ? w : h);

    const struct { float cx, cy, r; touch_zone z; } buttons[] =
    {
        { 0.70f, 0.48f, 0.10f, Z_MGUN },
        { 0.70f, 0.72f, 0.10f, Z_MSL },
        { 0.83f, 0.28f, 0.08f, Z_TGT },
        { 0.55f, 0.15f, 0.08f, Z_WPN },
        { 0.08f, 0.12f, 0.07f, Z_BACK },
    };

    for (const auto &b : buttons)
    {
        const float dx = x - b.cx * w, dy = y - b.cy * h;
        const float r = b.r * m;
        if (dx * dx + dy * dy <= r * r)
            return b.z;
    }

    if (x >= 0.90f * w)
        return Z_THROTTLE;

    if (x < 0.45f * w)
        return Z_STICK;

    return Z_NONE;
}

void set_key(int key, bool value)
{
    platform::debug_buttons()[key] = value;
}

} //namespace

//------------------------------------------------------------
// static state

std::map<int, bool> platform::m_buttons;
platform::key_callback platform::m_key_callback;

//------------------------------------------------------------
// iOS virtual controls API

void platform::ios_set_size(int w, int h)
{
    g_frame_w = w, g_frame_h = h;
}

float platform::ios_stick_x() { return g_stick_x; }
float platform::ios_stick_y() { return g_stick_y; }
float platform::ios_throttle() { return g_throttle; }
bool platform::ios_brake() { return g_brake; }

void platform::ios_touch(int x, int y, unsigned int touch_idx, bool pressed)
{
    if (touch_idx >= sizeof(g_touches) / sizeof(g_touches[0]))
        return;

    const int w = g_frame_w ? g_frame_w : 1;
    const int h = g_frame_h ? g_frame_h : 1;
    const float m = float(w < h ? w : h);

    touch_slot &t = g_touches[touch_idx];

    if (pressed && !t.active)
    {
        t.active = true;
        t.x = x, t.y = y;
        t.ox = x, t.oy = y;
        t.zone = zone_at(x, y, w, h);

        if (t.zone == Z_STICK)
        {
            g_stick_x = g_stick_y = 0.0f; //origin re-anchored on every new touch
            return;
        }

        //button zones must register on the press itself: falling through to
        //the switch below is what sets the key (an early return here left
        //every button permanently un-pressed on iOS)
    }
    else if (!t.active)
        return;
    else
        t.x = x, t.y = y;

    switch (t.zone)
    {
        case Z_STICK:
        {
            const float r = 0.30f * m;
            float nx = r > 0.0f ? (x - t.ox) / r : 0.0f;
            float ny = r > 0.0f ? (y - t.oy) / r : 0.0f;

            const float len = std::sqrt(nx * nx + ny * ny);
            if (len > 1.0f)
                nx /= len, ny /= len;

            g_stick_x = std::fabs(nx) < dead_zone ? 0.0f : nx;
            g_stick_y = std::fabs(ny) < dead_zone ? 0.0f : ny;
            break;
        }

        case Z_THROTTLE:
        {
            const float frac = 1.0f - float(y) / h; //top of the strip = full throttle
            g_brake = frac < 0.08f;
            g_throttle = g_brake ? 0.0f : (frac < 0.1f ? 0.0f : frac);
            break;
        }

        case Z_MSL: //missile (hold) / menu confirm (tap edge)
            set_key(OH_KEY_SPACE, pressed);
            set_key(OH_KEY_ENTER, pressed);
            break;

        case Z_MGUN: //machine gun (hold)
            set_key(OH_KEY_LEFT_CONTROL, pressed);
            set_key(OH_KEY_LEFT_SHIFT, pressed);
            break;

        case Z_TGT: //change target
            set_key(OH_KEY_E, pressed);
            break;

        case Z_WPN: //change weapon
            set_key(OH_KEY_Q, pressed);
            break;

        case Z_BACK: //menu back / exit current game
            set_key(OH_KEY_ESCAPE, pressed);
            break;

        default:
            break;
    }

    if (!pressed)
    {
        if (t.zone == Z_STICK)
            g_stick_x = g_stick_y = 0.0f;
        t.active = false;
        t.zone = Z_NONE;
    }

    ios_update_stick_keys();
}

//------------------------------------------------------------
// stick deflection -> held direction keys (menu navigation; also gives the
// flight path a digital fallback through demo.cpp's key mapping)

void platform::ios_update_stick_keys()
{
    //touch Y is screen-down positive, so finger-up = negative stick_y.
    //Menu convention: finger flicks up to move selection DOWN (like swiping
    //a list up to see what's below), matching the inverted pitch feel.
    set_key(OH_KEY_UP, g_stick_y > 0.35f);
    set_key(OH_KEY_DOWN, g_stick_y < -0.35f);
    set_key(OH_KEY_LEFT, g_stick_x < -0.35f);
    set_key(OH_KEY_RIGHT, g_stick_x > 0.35f);
}

//------------------------------------------------------------
// platform interface (GLFW-free)

bool platform::init(int width, int height, const char *title, bool fullscreen)
{
    (void)title, (void)fullscreen;
    if (!g_frame_w)
        g_frame_w = width;
    if (!g_frame_h)
        g_frame_h = height;
    m_window_w = g_frame_w, m_window_h = g_frame_h;
    return true;
}

void platform::terminate() {}

bool platform::should_terminate() { return false; }

void platform::end_frame()
{
    m_last_buttons = m_buttons;
}

bool platform::was_pressed(int key)
{
    auto k = m_buttons.find(key);
    if (k == m_buttons.end() || !k->second)
        return false;

    auto l = m_last_buttons.find(key);
    return l == m_last_buttons.end() || !l->second;
}

bool platform::get_key(int key)
{
    auto k = m_buttons.find(key);
    return k == m_buttons.end() ? false : k->second;
}

bool platform::get_mouse_lbtn() { return false; }
bool platform::get_mouse_rbtn() { return false; }
int platform::get_mouse_x() { return 0; }
int platform::get_mouse_y() { return 0; }
int platform::get_frame_width() { return g_frame_w; }
int platform::get_frame_height() { return g_frame_h; }
int platform::get_window_width() { return g_frame_w; }
int platform::get_window_height() { return g_frame_h; }

void platform::set_fullscreen(bool value, int windowed_width, int windowed_height)
{
    (void)value, (void)windowed_width, (void)windowed_height;
}

void platform::set_keyboard_callback(key_callback &k) { m_key_callback = k; }

void platform::key_func(GLFWwindow *, int key, int scancode, int action, int mods)
{
    (void)key, (void)scancode, (void)action, (void)mods; //unused on iOS
}

void platform::UpdateSize() {}

std::string platform::open_folder_dialog() { return ""; }
bool platform::show_msgbox(std::string message) { (void)message; return false; }

//------------------------------------------------------------

#endif //OH_PLATFORM_IOS
