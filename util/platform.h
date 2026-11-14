//
// open horizon -- undefined_darkness@outlook.com
//

#pragma once

#include <string>
#include <functional>
#include <map>

//------------------------------------------------------------
//Virtual key codes (GLFW-compatible values). Declared here so the shared
//key space compiles on iOS where GLFW/glfw3.h cannot be included (it drags
//in OpenGL/gl.h, absent from the iOS SDK). Desktop builds still use the
//real glfw3.h via platform.cpp; values are identical.
enum
{
    OH_KEY_SPACE = 32,
    OH_KEY_COMMA = 44,
    OH_KEY_MINUS = 45,
    OH_KEY_PERIOD = 46,
    OH_KEY_SLASH = 47,
    OH_KEY_0 = 48, OH_KEY_1 = 49, OH_KEY_2 = 50, OH_KEY_3 = 51,
    OH_KEY_4 = 52, OH_KEY_5 = 53, OH_KEY_6 = 54, OH_KEY_7 = 55,
    OH_KEY_8 = 56, OH_KEY_9 = 57,
    OH_KEY_A = 65, OH_KEY_B = 66, OH_KEY_C = 67, OH_KEY_D = 68,
    OH_KEY_E = 69, OH_KEY_F = 70, OH_KEY_G = 71, OH_KEY_H = 72,
    OH_KEY_I = 73, OH_KEY_J = 74, OH_KEY_K = 75, OH_KEY_L = 76,
    OH_KEY_M = 77, OH_KEY_N = 78, OH_KEY_O = 79, OH_KEY_P = 80,
    OH_KEY_Q = 81, OH_KEY_R = 82, OH_KEY_S = 83, OH_KEY_T = 84,
    OH_KEY_U = 85, OH_KEY_V = 86, OH_KEY_W = 87, OH_KEY_X = 88,
    OH_KEY_Y = 89, OH_KEY_Z = 90,
    OH_KEY_ESCAPE = 256,
    OH_KEY_ENTER = 257,
    OH_KEY_UP = 265,
    OH_KEY_DOWN = 266,
    OH_KEY_RIGHT = 267,
    OH_KEY_LEFT = 268,
    OH_KEY_LEFT_CONTROL = 343,
    OH_KEY_LEFT_SHIFT = 340,
};

struct GLFWwindow;

class platform
{
public:
    bool init(int width, int height, const char *title, bool fullscreen);
    void terminate();
    bool should_terminate();
    void end_frame();
    bool was_pressed(int key);

    bool get_key(int key);
    bool get_mouse_lbtn();
    bool get_mouse_rbtn();
    int get_mouse_x();
    int get_mouse_y();
    int get_frame_width();
    int get_frame_height();
    int get_window_width();
    int get_window_height();

    void set_fullscreen(bool value, int windowed_width, int windowed_height);

    typedef std::function<void(char c)> key_callback;
    void set_keyboard_callback(key_callback &k);

    static std::string open_folder_dialog();
    static bool show_msgbox(std::string message);

    //debug: raw access for key injection (auto-drive)
    static std::map<int, bool> &debug_buttons() { return m_buttons; }

    platform(): m_window(0) {}

private:
    static void key_func(GLFWwindow *, int key, int scancode, int action, int mods);
    void UpdateSize();

private:
    GLFWwindow *m_window;
    int m_mouse_x, m_mouse_y;
    int m_frame_w, m_frame_h;
    int m_window_w, m_window_h;
    static std::map<int, bool> m_buttons;
    std::map<int, bool> m_last_buttons;
    static key_callback m_key_callback;

#ifdef OH_PLATFORM_IOS
public:
    //iPhone backend (platform_ios.cpp): fed by app.mm on_touch/on_resize via
    //the oh_ios_game_app entry in demo.cpp; read by the on_frame control loop.
    static void ios_set_size(int w, int h);
    static void ios_touch(int x, int y, unsigned int touch_idx, bool pressed);

    //virtual flight controls, landscape screen coordinates:
    static float ios_stick_x();  //-1..1, right = roll right
    static float ios_stick_y();  //-1..1, finger pulled down = climb
    static float ios_throttle(); //0..1
    static bool ios_brake();

    //MFi gamepad (GameController framework). When a controller is connected,
    //on_frame polls it and feeds the standardized layout into joystick_config,
    //which uses the same joystick.xml mapping as the desktop build. Touch
    //controls are used as fallback when no gamepad is connected.
    static void ios_gamepad_init();
    static bool ios_gamepad_connected();
    static const char *ios_gamepad_name();
    static bool ios_gamepad_poll(float *axes, unsigned char *buttons,
                                 int axes_count, int btns_count);
    static void ios_audio_init();

private:
    static void ios_update_stick_keys();
#endif
};

//------------------------------------------------------------
