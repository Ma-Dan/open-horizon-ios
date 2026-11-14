//
// open horizon -- undefined_darkness@outlook.com
//

#include "game/mission.h"
#include "game/deathmatch.h"
#include "game/team_deathmatch.h"
#include "game/free_flight.h"
#include "game/network_client.h"
#include "game/network_server.h"
#include "game/hangar.h"
#include "game/world.h"
#include "gui/menu.h"

#include "scene/camera.h"
#include "render/render_buffered.h"

#include "util/controls.h"
#include "util/resources.h"
#include "util/config.h"
#include "util/platform.h"
#include "renderer/shared.h"

#include <thread>
#include <chrono>
#include <atomic>
#include <stdio.h>
#include <stdlib.h>

#ifndef OH_PLATFORM_IOS
#include "GLFW/glfw3.h"
#else
#include "system/app.h" //nya-engine iOS lifecycle (deps/nya-engine/system/app.mm)
#endif

#ifdef _WIN32
    #undef APIENTRY
    #include "log/composite_log.h"
    #include "log/windbg_log.h"
    #include <windows.h>

    int main();
    int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) 
    { 
        auto log = new nya_log::composite_log();
        log->add_log(&nya_log::log());
        log->add_log(new nya_log::windbg_log());
        nya_log::set_log(log);
        return main(); 
    }
#endif

//------------------------------------------------------------

#ifdef OH_PLATFORM_IOS
//iPhone virtual-control overlay: draws the touch zones as translucent
//outlines/fills so the layout is discoverable without any UI art assets.
//Coordinates match platform_ios.cpp's zone_at() exactly (normalised, by the
//short edge for radii). Rendered in 1366x768 ui_render space.
static void draw_ios_touch_overlay(gui::render &r, const gui::fonts &f)
{
    const float W = float(r.get_width()), H = float(r.get_height());
    const float m = W < H ? W : H;

    const nya_math::vec4 fill_active(0.3f, 0.6f, 1.0f, 0.35f);
    const nya_math::vec4 fill_idle(1.0f, 1.0f, 1.0f, 0.12f);
    const nya_math::vec4 stroke(1.0f, 1.0f, 1.0f, 0.55f);
    const nya_math::vec4 stick_fill(0.3f, 0.6f, 1.0f, 0.18f);

    auto active = [](int key) -> bool {
        auto &b = platform::debug_buttons();
        auto it = b.find(key);
        return it != b.end() && it->second;
    };

    //--- stick (left 45%): a big translucent box + deflection dot
    {
        const float sx = 0.0f, sy = 0.0f, sw = 0.45f * W, sh = H;
        std::vector<gui::rect_pair> rp = {
            { gui::rect(int(sx), int(sy), int(sw), int(sh)), gui::rect(0, 0, 1, 1) }
        };
        r.draw(rp, shared::get_white_texture(), stick_fill);
        //deflection dot
        const float jx = platform::ios_stick_x(), jy = -platform::ios_stick_y();
        const float dot_r = 0.05f * m;
        const float cx = sx + sw * 0.5f + jx * 0.18f * W;
        const float cy = sy + sh * 0.5f + jy * 0.18f * H;
        std::vector<gui::rect_pair> dot = {
            { gui::rect(int(cx - dot_r), int(cy - dot_r), int(dot_r * 2), int(dot_r * 2)), gui::rect(0, 0, 1, 1) }
        };
        r.draw(dot, shared::get_white_texture(), nya_math::vec4(0.3f, 0.8f, 1.0f, 0.8f));
    }

    //--- throttle strip (right 10%): vertical fill showing current throttle
    {
        const float tx = 0.90f * W, tw = 0.10f * W;
        std::vector<gui::rect_pair> bg = {
            { gui::rect(int(tx), 0, int(tw), int(H)), gui::rect(0, 0, 1, 1) }
        };
        r.draw(bg, shared::get_white_texture(), fill_idle);
        const float th = platform::ios_throttle();
        const float th_top = (1.0f - th) * H;
        std::vector<gui::rect_pair> fl = {
            { gui::rect(int(tx), int(th_top), int(tw), int(H - th_top)), gui::rect(0, 0, 1, 1) }
        };
        const nya_math::vec4 th_col = platform::ios_brake()
            ? nya_math::vec4(1.0f, 0.3f, 0.3f, 0.5f)
            : nya_math::vec4(0.3f, 0.8f, 1.0f, 0.5f);
        r.draw(fl, shared::get_white_texture(), th_col);
    }

    //--- circular buttons (cx, cy, r, label, active-key)
    struct btn { float cx, cy, r; const wchar_t *label; int key; };
    const btn buttons[] = {
        { 0.70f, 0.48f, 0.10f, L"MGUN", OH_KEY_LEFT_CONTROL },
        { 0.70f, 0.72f, 0.10f, L"MSL",  OH_KEY_SPACE },
        { 0.83f, 0.28f, 0.08f, L"TGT",  OH_KEY_E },
        { 0.55f, 0.15f, 0.08f, L"WPN",  OH_KEY_Q },
        { 0.08f, 0.12f, 0.07f, L"BACK", OH_KEY_ESCAPE },
    };
    for (const auto &b : buttons)
    {
        const float bx = b.cx * W, by = b.cy * H, br = b.r * m;
        //fill
        std::vector<gui::rect_pair> rp = {
            { gui::rect(int(bx - br), int(by - br), int(br * 2), int(br * 2)), gui::rect(0, 0, 1, 1) }
        };
        r.draw(rp, shared::get_white_texture(), active(b.key) ? fill_active : fill_idle);
        //outline (line loop of 24 segments)
        std::vector<nya_math::vec2> loop;
        loop.reserve(25);
        for (int i = 0; i <= 24; ++i)
        {
            const float a = 2.0f * nya_math::constants::pi * float(i) / 24.0f;
            loop.push_back(nya_math::vec2(bx + br * cosf(a), by + br * sinf(a)));
        }
        r.draw(loop, stroke, gui::render::transform(), true);
        //label (centred)
        const nya_math::vec4 lc = active(b.key)
            ? nya_math::vec4(1.0f, 1.0f, 0.4f, 0.95f)
            : nya_math::vec4(1.0f, 1.0f, 1.0f, 0.7f);
        const int tw = f.get_text_width(b.label, "Zurich30");
        f.draw_text(r, b.label, "Zurich30", int(bx - tw / 2), int(by - 10), lc);    }
}
#endif

//------------------------------------------------------------

    struct game_thread
    {
        nya_render::render_buffered &render;
        std::thread thread;
        game_thread(nya_render::render_buffered &r,int w, int h): render(r)
        {
            platform_width = w, platform_height = h;
            platform_terminate = false;            platform_fullscreen = false;
            frame_pending = false;
            paused = false, exit = false;
            thread = std::thread([this]()
            {
                renderer::scene scene;
                sound::world sound_world;
                game::world world(scene, sound_world, scene.hud);
                game::mission game_mode_ms(world);
                game::free_flight game_mode_ff(world);
                game::deathmatch game_mode_dm(world);
                game::team_deathmatch game_mode_tdm(world);
                game::hangar hangar(scene);
                game::game_mode *active_game_mode = 0;
                game::network_client client;
                game::network_server server;

                gui::menu menu(sound_world);
/*
                platform::key_callback kcb = std::bind(&gui::menu::on_input, &menu, std::placeholders::_1);
                platform.set_keyboard_callback(kcb);
 */
                int mx = mouse_x, my = mouse_y;

                int screen_width = platform_width, screen_height = platform_height;
                scene.resize(screen_width, screen_height);
                scene.loading(true);
                nya_render::clear(true, true);
                scene.draw();
                end_frame();

                sound_world.set_volume(config::get_var_int("master_volume") / 10.0f);
                sound_world.set_music_volume(config::get_var_int("music_volume") / 10.0f);

                menu.init();
                sound_world.set_music("BGM_menu");

                bool viewer_mode = false;
                bool is_client = false, is_server = false;

                gui::menu::on_action on_menu_action = [&](const std::string &event)
                {
                    if (event == "start")
                    {
                        exit = false;
                        bool first = true;
                        auto loading_callback=[this,&scene,&first]()
                        {
                            static bool init = true;
                            static gui::tiles loading_shape;
                            if (init)
                            {
                                loading_shape.load("UI/comp_loading.lar");
                                init = false;
                            }

                            if(!first && render.get_buffer_size() < 16 * 1024 * 1024)
                                return;

                            first = false;

                            const int anim_time = (int)nya_system::get_time();
                            nya_render::clear(true, true);

                            const auto color = nya_math::vec4::one();
                            scene.ui_fonts.draw_text(scene.ui_render, L"LOADING", "ZurichBD20outline", scene.ui_render.get_width()/2 - 30, scene.ui_render.get_height()/2 - 10, color);
                            gui::rect r;
                            r. w = r.h = 32;
                            r.x = r.y = -r.w/2;
                            gui::render::transform t;
                            t.x=(scene.ui_render.get_width())/2 + 40;
                            t.y=(scene.ui_render.get_height())/2;
                            t.yaw = -(anim_time % 1000) * 0.002f * nya_math::constants::pi;
                            t.sx = 0.5f,t.sy = -0.5f;
                            loading_shape.draw_tx(scene.ui_render, 0, 1, r, color, t);

                            end_frame();
                        };

                        loading_callback();
                        shared::set_loading_callback(loading_callback);

                        sound_world.stop_music();
                        const char *music_names[] = {"BGM_ms10_08", "BGM_ms06", "BGM_ms08x", "BGM_ms11a", "BGM_ms11b", "BGM_ms12_02"};
                        sound_world.set_music(music_names[rand() % (sizeof(music_names) / sizeof(music_names[0]))]);

                        auto location = menu.get_var("map");
                        auto plane = menu.get_var("aircraft");
                        const int color = atoi(menu.get_var("color").c_str());

                        is_client = false, is_server = false;

                        auto mode = menu.get_var("mode");
                        auto mp_var = menu.get_var("multiplayer");
                        if (mp_var == "server")
                        {
                            world.set_network(&server);
                            auto port = menu.get_var_int("port");
                            server.open(port, config::get_var("name").c_str(), mode.c_str(), menu.get_var("map").c_str(), menu.get_var_int("max_players"));
                            if (menu.get_var("mp_public") == "true")
                                game::servers_list::register_server(port);
                            is_server = true;
                        }
                        else if (mp_var == "client")
                        {
                            world.set_network(&client);
                            client.start();
                            is_client = true;
                        }

                        if (mode == "ms")
                        {
                            active_game_mode = &game_mode_ms;
                            game_mode_ms.start(plane.c_str(), color, menu.get_var("mission").c_str());
                        }
                        else if (mode == "dm")
                        {
                            const int bots_count = (is_client || is_server) ? 0 : 11;

                            active_game_mode = &game_mode_dm;
                            game_mode_dm.start(plane.c_str(), color, 0, location.c_str(), bots_count);
                        }
                        else if (mode == "tdm")
                        {
                            const int bots_count = (is_client || is_server) ? 0 : 7;

                            active_game_mode = &game_mode_tdm;
                            game_mode_tdm.start(plane.c_str(), color, 0, location.c_str(), bots_count);
                        }
                        else if (mode == "ff")
                        {
                            active_game_mode = &game_mode_ff;
                            game_mode_ff.start(plane.c_str(), color, location.c_str());
                        }

                        shared::update_loading();
                        shared::set_loading_callback(0);
                    }
                    else if (event == "connect")
                    {
                        menu.set_error("");
                        client.disconnect();
                        auto port = menu.get_var_int("port");
                        if (client.connect(menu.get_var("address").c_str(), port))
                        {
                            menu.send_event("map=" + client.get_server_info().location);
                            menu.send_event("mode=" + client.get_server_info().game_mode);
                            menu.send_event("screen=ac_select");
                        }
                        else
                            menu.set_error(client.get_error());
                    }
                    else if (event == "viewer_start")
                    {
                        viewer_mode = true;
                        scene.camera.add_delta_rot(0.2f, 2.5f);
                    }
                    else if (event == "viewer_update_bg")
                    {
                        hangar.set_bkg(menu.get_var("bkg").c_str());
                    }
                    else if (event == "viewer_update_ac")
                    {
                        const auto dr = scene.camera.get_delta_rot();
                        hangar.set_plane(menu.get_var("aircraft").c_str());
                        scene.camera.add_delta_rot(dr.x, dr.y - 3.14f);
                    }
                    else if (event == "viewer_update_color")
                    {
                        const auto dr = scene.camera.get_delta_rot();
                        const int color = atoi(menu.get_var("color").c_str());
                        hangar.set_plane_color(color);
                        scene.camera.add_delta_rot(dr.x, dr.y - 3.14f);
                    }
                    else if (event == "viewer_end")
                    {
                        viewer_mode = false;
                        hangar.end();
                    }
                    else if (event == "update_volume")
                    {
                        sound_world.set_volume(config::get_var_int("master_volume") / 10.0f);
                        sound_world.set_music_volume(config::get_var_int("music_volume") / 10.0f);
                    }/*
                    else if (event == "update_joy_config") //ToDo
                    {
                        if (!joysticks.empty())
                            joysticks.front().update_config();
                    }*/
                    else if (event == "fullscreen_toggle")
                    {
                        platform_fullscreen = true;
                    }
                    else if (event == "exit")
                    {
                        server.close();
                        client.disconnect();
                        platform_terminate=true;
                    }
                    else
                        nya_log::log("unknown event: %s\n", event.c_str());
                };

                menu.set_callback(on_menu_action);

                bool reset_camera = false;

                unsigned long app_time = nya_system::get_time();
                while (!platform_terminate)
                {
                    unsigned long time = nya_system::get_time();
                    int dt = int(time - app_time);
                    if (dt > 1000 && !is_client && !is_server)
                        dt = 1000;

                    app_time = time;

                    static bool speed10x = false;

                    if (platform_width != screen_width || platform_height != screen_height)
                    {
                        screen_width = platform_width, screen_height = platform_height;
                        scene.resize(screen_width, screen_height);

                        if (!config::get_var_bool("fullscreen"))
                        {
                            config::set_var("screen_width", std::to_string(platform_window_width));
                            config::set_var("screen_height", std::to_string(platform_window_height));
                        }
                    }

                    //pick up the freshest polled input: copy the shared controls at
                    //loop start. Copying only in end_frame() (i.e. after the frame is
                    //drawn) leaves every button press one full frame staler, which
                    //gamepad players feel as trigger delay.
                    controls = controls_update;
                    menu_controls = menu_controls_update;

                    if (active_game_mode)
                    {
                        if(is_server || is_client)
                            paused = false;

                        scene.pause(paused);

                        if (!paused)
                        {
                            active_game_mode->update(speed10x ? dt * 10 : dt, controls);

                            //ToDo: move to game code
                            //camera - tracking enemy
                            auto p = world.get_player();
                            if (p && p->change_target_hold_time >= game::plane::change_target_hold_max_time && !p->targets.empty() && !p->targets.front().target.expired())
                            {
                                static auto last_target = p->targets.front().target;
                                if (last_target.expired() || last_target.lock() != p->targets.front().target.lock())
                                {
                                    last_target = p->targets.front().target;
                                    p->change_target_hold_time = game::plane::change_target_hold_max_time;
                                }

                                auto t = p->targets.front().target.lock();
                                auto tdir = t->get_pos() - p->get_pos(); // + (t->get_vel() - p->get_vel()) * (dt * 0.001f)
                                nya_math::quat q(nya_math::vec3::forward(), tdir);
                                q = nya_math::quat::invert(p->get_rot()) * q;

                                auto da = q.get_euler();
                                scene.camera.reset_delta_rot();

                                const float k = nya_math::min((p->change_target_hold_time - game::plane::change_target_hold_max_time) / 500.0f, 1.0);

                                scene.camera.add_delta_rot(da.x * k, -da.y * k);
                                reset_camera = true;
                            }
                        }

                        scene.draw();
/*
                        //util debug draw
                        nya_render::clear(false, true);
                        nya_render::set_state(nya_render::state());
                        nya_render::set_modelview_matrix(nya_scene::get_camera().get_view_matrix());
                        get_debug_draw().set_line_width(2.0f);
                        get_debug_draw().set_point_size(3.0f);
                        get_debug_draw().draw();
*/
                    }
                    else
                    {
                        nya_render::clear(true, true);

                        if (viewer_mode)
                        {
                            hangar.update(dt);
                            scene.draw();
                        }
                        menu.draw(scene.ui_render);
                        menu.update(dt, menu_controls);
                    }

                    const char *ui_ref = 0;
                    //ui_ref = "ui_ref4.tga";
                    if (ui_ref)
                    {
                        static nya_scene::texture ui_ref_texture(ui_ref);
                        static std::vector<gui::rect_pair> ui_ref_rects(1);
                        ui_ref_rects[0].r.w = scene.ui_render.get_width();
                        ui_ref_rects[0].r.y = scene.ui_render.get_height();
                        ui_ref_rects[0].r.h = -scene.ui_render.get_height();
                        ui_ref_rects[0].tc.w = ui_ref_texture.get_width();
                        ui_ref_rects[0].tc.h = ui_ref_texture.get_height();
                        static int alpha_anim = 0;
                        alpha_anim += dt;
                        alpha_anim = alpha_anim % 4000;
                        //alpha_anim = 1000;
                        scene.ui_render.draw(ui_ref_rects, ui_ref_texture, nya_math::vec4(1.0f, 1.0f, 1.0f, fabsf(alpha_anim / 2000.0f - 1.0f)));
                    }

                    end_frame();

                    //controls

                    if (mouse_l)
                        scene.camera.add_delta_rot((mouse_y - my) * 0.03, (mouse_x - mx) * 0.03);
                    if (mouse_r && viewer_mode)
                        scene.camera.add_delta_pos(0, 0, my - mouse_y);
                    mx = mouse_x, my = mouse_y;

                    if (active_game_mode)
                    {
                        bool should_stop = exit;
                        if (is_client && !client.is_up())
                            should_stop = true;
                        if (is_server && !server.is_up())
                            should_stop = true;

                        if (should_stop)
                        {
                            if (paused)
                                scene.pause(paused = !paused);
                            active_game_mode->end();
                            active_game_mode = 0;
                            server.close();
                            client.disconnect();
                            world.set_network(0);
                            sound_world.stop_sounds();
                            menu_controls.prev = false;
                            if (is_client)
                                menu.send_event("screen=mp_connect");

                            sound_world.set_music("BGM_menu");
                        }
                    }

                    if (controls.change_camera || (reset_camera && !controls.change_target))
                    {
                        scene.camera.reset_delta_rot();
                        reset_camera = false;
                    }
                    //right-stick free look (see also joystick_config::apply_controls,
                    //which fills controls.cam_rot): stick deflection is the view offset
                    //relative to the flight direction, springing back on release.
                    //Full deflection = 180 degrees of yaw each way, proportional
                    //below that. Skipped while the mouse drags the view, while a
                    //target is tracked (its block drives the camera) and in the
                    //hangar viewer.
                    if (!mouse_l && !controls.change_target && !viewer_mode)
                    {
                        scene.camera.reset_delta_rot();
                        scene.camera.add_delta_rot(-controls.cam_rot.x * nya_math::constants::pi_2,
                                                   -controls.cam_rot.y * nya_math::constants::pi);
                    }
                }

                server.close();
                client.disconnect();
                sound::release_context();
            });
        }

        std::atomic<bool> platform_terminate, platform_fullscreen;
        std::atomic<int> platform_width, platform_height;
        std::atomic<int> platform_window_width, platform_window_height;
        std::atomic<bool> mouse_l, mouse_r;
        std::atomic<int> mouse_x, mouse_y;
        std::atomic<bool> paused, exit;

        game::plane_controls controls_update;
        gui::menu_controls menu_controls_update;

        std::atomic<bool> frame_pending;

    private:
        game::plane_controls controls;
        gui::menu_controls menu_controls;

        void end_frame()
        {
            while (!platform_terminate)
            {
                if(frame_pending)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                else
                {
                    render.commit();
                    controls = controls_update;
                    menu_controls = menu_controls_update;
                    frame_pending = true;
                    break;
                }
            }
        }

};

#ifdef OH_PLATFORM_IOS

//iPhone entry: the GLFW desktop main() is replaced by the nya-engine app
//lifecycle (app.mm: EAGL/ES2 + CADisplayLink on the main thread).
//on_init mirrors desktop setup (resource pack, render api, game thread);
//on_frame is the "platform thread" loop body: read the touch-virtual controls
//from platform_ios, hand them to the game thread, execute the GL command
//buffer; app.mm presents the framebuffer afterwards.

class oh_ios_game_app : public nya_system::app
{
public:
    void on_resize(unsigned int w, unsigned int h) override
    {
        platform::ios_set_size(int(w), int(h));
    }

    void on_init() override
    {
        //engine log() uses printf(stdout) which is block-buffered when piped;
        //unbuffer it so resource/loading failures show up in console captures
        setvbuf(stdout, 0, _IONBF, 0);

        //Enable anisotropic filtering by default: without it, ground/building
        //textures viewed at grazing angles flicker (mip-level popping along
        //walls and distant terrain). Apple GLES exposes
        //EXT_texture_filter_anisotropic; 4x is a safe default that eliminates
        //the flicker with negligible cost on A-class GPUs.
        nya_render::texture::set_default_aniso(4);

        setup_resources();

        config::register_var("screen_width", "1000");
        config::register_var("screen_height", "562");
        config::register_var("fullscreen", "false");
        config::register_var("master_volume", "10");
        config::register_var("music_volume", "5");
        config::register_var("difficulty", "hard");

        m_render = new nya_render::render_buffered(nya_render::get_api_interface());
        nya_render::set_render_api(m_render);

        m_gt = new game_thread(*m_render, m_platform.get_frame_width(), m_platform.get_frame_height());

        //MFi gamepad: start observing GCController connect/disconnect.
        //joystick_config::init loads joystick.xml which matches by controller
        //name; Xbox Wireless / DualSense report their vendor name over BLE.
        platform::ios_gamepad_init();

        //Audio: activate AVAudioSession before OpenAL can produce sound.
        platform::ios_audio_init();
    }

    void on_frame(unsigned int dt) override
    {
        (void)dt;
        if (!m_gt)
            return;

        //If the game thread exited (menu "exit" sets platform_terminate),
        //quit the app — iOS doesn't have a window-close button, so we
        //call exit() directly as app.mm's finish() does.
        if (m_gt->platform_terminate)
        {
            if (m_gt->thread.joinable())
                m_gt->thread.join();
            exit(0);
        }

        //refresh the edge-detect snapshot: touches since the previous frame
        //become "current" for was_pressed()
        m_platform.end_frame();

        //TEMP DEBUG: headless-simulator autopilot. No Simulator.app GUI on
        //this machine (slim Xcode install), so drive the real touch pipeline
        //programmatically: menu down x3 -> Free flight, MSL confirm through
        //map/aircraft/color selects. Remove when touch UI is validated.
        static int oh_auto_frame = -1;
        if (oh_auto_frame < 0)
        {
            const char *e = getenv("OH_FF_AUTO");
            oh_auto_frame = (e && e[0]) ? 0 : -2;
            fprintf(stderr, "OH_AUTO: env=%s -> %s\n", e ? e : "(null)",
                    oh_auto_frame == 0 ? "armed" : "off");
        }
        if (oh_auto_frame >= 0)
        {
            struct step { int f; int x, y; unsigned idx; bool press; };
            const int w = m_platform.get_frame_width(), h = m_platform.get_frame_height();
            const int msl_x = int(0.70f * w), msl_y = int(0.72f * h);
            const int sx = int(0.22f * w), sy = int(0.50f * h);
            const int dy = int(0.55f * 0.30f * (w < h ? w : h));
            const step steps[] =
            {
                //three short stick-up flicks (screen-down positive; menu maps
                //finger-up = DOWN per touch-list convention, so flick up to
                //advance selection downward)
                { 120, sx, sy, 1, true },
                { 124, sx, sy - dy, 1, true },
                { 140, sx, sy - dy, 1, false },
                { 180, sx, sy, 1, true },
                { 184, sx, sy - dy, 1, true },
                { 200, sx, sy - dy, 1, false },
                { 240, sx, sy, 1, true },
                { 244, sx, sy - dy, 1, true },
                { 260, sx, sy - dy, 1, false },
                //MSL confirm -> map_select
                { 330, msl_x, msl_y, 2, true },
                { 360, msl_x, msl_y, 2, false },
                //stick up three flicks in map_select: entry 1 is "def" (Test,
                //Debug-only, no sky/terrain assets), entry 2..4 Miami/Dubai/Paris;
                //pick Paris (4th) - fully packed with sky, ocean, terrain, clouds
                { 390, sx, sy, 1, true },
                { 394, sx, sy - dy, 1, true },
                { 410, sx, sy - dy, 1, false },
                { 430, sx, sy, 1, true },
                { 434, sx, sy - dy, 1, true },
                { 450, sx, sy - dy, 1, false },
                { 470, sx, sy, 1, true },
                { 474, sx, sy - dy, 1, true },
                { 490, sx, sy - dy, 1, false },
                //MSL confirm -> ac_select
                { 540, msl_x, msl_y, 2, true },
                { 570, msl_x, msl_y, 2, false },
                //MSL confirm -> color_select
                { 700, msl_x, msl_y, 2, true },
                { 730, msl_x, msl_y, 2, false },
                //MSL confirm -> start free flight
                { 880, msl_x, msl_y, 2, true },
                { 910, msl_x, msl_y, 2, false },
            };
            for (const auto &s : steps)
            {
                if (s.f == oh_auto_frame)
                {
                    platform::ios_touch(s.x, s.y, s.idx, s.press);
                    fprintf(stderr, "OH_AUTO f=%d touch(%d,%d,idx=%d,%d) DOWN=%d ENTER=%d\n",
                            oh_auto_frame, s.x, s.y, s.idx, (int)s.press,
                            (int)m_platform.get_key(OH_KEY_DOWN), (int)m_platform.get_key(OH_KEY_ENTER));
                }
            }
            ++oh_auto_frame;
        }

        game::plane_controls controls;
        gui::menu_controls menu_controls;

        //MFi gamepad takes priority when connected; touch controls are the
        //fallback. The gamepad feeds the same standardized layout
        //([LX,LY,RX,RY,LT,RT] / [A,B,X,Y,...]) that joystick.xml maps, so
        //the Xbox Wireless Controller mapping works identically to desktop.
        bool gamepad_active = false;
        if (platform::ios_gamepad_connected())
        {
            if (!m_gamepad_inited)
            {
                m_gamepad.init(platform::ios_gamepad_name());
                m_gamepad_inited = true;
            }

            float axes[6] = {0};
            unsigned char buttons[15] = {0};
            if (platform::ios_gamepad_poll(axes, buttons, 6, 15))
            {
                m_gamepad.update(axes, 6, buttons, 15);
                bool joy_pause = false;
                m_gamepad.apply_controls(controls, joy_pause);

                //iOS: triggers report 0..1 but Bluetooth controllers may not
                //reach full deflection on light press. Scale up so that
                //half-pressure already gives meaningful throttle/brake.
                float lt = axes[4], rt = axes[5];
                if (rt > 0.05f) rt = nya_math::min(rt * 5.0f, 1.0f);
                if (lt > 0.05f) lt = nya_math::min(lt * 5.0f, 1.0f);
                if (lt > 0.05f) controls.brake = lt;
                if (rt > 0.05f) controls.throttle = rt;

                //Suppress menu navigation while B is held after exiting from
                //pause, so the same B press (mapped to menu_next in
                //joystick.xml) doesn't immediately re-launch the game.
                bool b_pressed = (buttons[1] != 0);
                if (m_menu_suppress)
                {
                    if (!b_pressed)
                        m_menu_suppress = false;
                }
                else
                {
                    m_gamepad.apply_controls(menu_controls);
                }
                gamepad_active = true;

                //Start (btn7) toggles pause — edge-detected so a single
                //press doesn't flip repeatedly while held. Mirrors desktop
                //was_pressed(GLFW_KEY_P).
                if (joy_pause && !m_prev_joy_pause)
                    m_gt->paused = !m_gt->paused;
                m_prev_joy_pause = joy_pause;

                //B (btn1) while paused → exit to main menu. Edge-detected.
                //The game thread checks `exit` after scene.draw() and calls
                //active_game_mode->end(), returning to the menu.
                if (b_pressed && !m_prev_b && m_gt->paused)
                {
                    m_gt->exit = true;
                    m_gt->paused = false;
                    menu_controls = gui::menu_controls();
                    m_menu_suppress = true;
                }
                m_prev_b = b_pressed;
            }
        }
        else
        {
            m_gamepad_inited = false;
        }

        if (!gamepad_active)
        {
        //virtual stick (analog): push down = nose down (dive), push up =
        //nose up (climb), right = roll right. iOS touch Y is screen-down
        //positive, so stick_y>0 means finger moved down; rot.x>0 = pitch up
        //in the physics layer, so negate to make "push down = dive".
        controls.rot.x = platform::ios_stick_y();
        controls.rot.z = platform::ios_stick_x();

        //throttle slider
        controls.throttle = platform::ios_throttle();
        controls.brake = platform::ios_brake() ? 1.0f : 0.0f;

        //buttons (synthetic GLFW keys set by the touch zones in platform_ios)
        if (m_platform.get_key(OH_KEY_SPACE))
            controls.missile = true, menu_controls.next = true;
        if (m_platform.get_key(OH_KEY_LEFT_CONTROL) || m_platform.get_key(OH_KEY_LEFT_SHIFT))
            controls.mgun = true;
        if (m_platform.get_key(OH_KEY_Q))
            controls.change_weapon = true;
        if (m_platform.get_key(OH_KEY_E))
            controls.change_target = true;
        if (m_platform.get_key(OH_KEY_F))
            controls.flares = true;
        if (m_platform.get_key(OH_KEY_V))
            controls.change_camera = true;

        //menu navigation: stick deflection is mapped to held direction keys
        //by platform_ios; confirm/back come from the MSL/BACK zones
        if (m_platform.get_key(OH_KEY_UP))
            menu_controls.up = true;
        if (m_platform.get_key(OH_KEY_DOWN))
            menu_controls.down = true;
        if (m_platform.get_key(OH_KEY_LEFT))
            menu_controls.left = true;
        if (m_platform.get_key(OH_KEY_RIGHT))
            menu_controls.right = true;
        if (m_platform.get_key(OH_KEY_ENTER))
            menu_controls.next = true;
        } //end !gamepad_active

        //mirror of the desktop "platform thread" handoff
        if (m_gt->frame_pending)
        {
            m_render->push();

            m_gt->controls_update = controls;
            m_gt->menu_controls_update = menu_controls;
            m_gt->frame_pending = false;

            m_render->execute();

            if (m_platform.was_pressed(OH_KEY_ESCAPE))
                menu_controls.prev = true, m_gt->exit = true;
        }

        m_gt->platform_width = m_platform.get_frame_width();
        m_gt->platform_height = m_platform.get_frame_height();
        m_gt->platform_window_width = m_platform.get_window_width();
        m_gt->platform_window_height = m_platform.get_window_height();
    }

    void on_free() override
    {
        if (!m_gt)
            return;

        m_gt->platform_terminate = true;
        if (m_gt->thread.joinable())
            m_gt->thread.join();

        nya_render::set_render_api(new nya_render::render_api_interface()); //ToDo
    }

    void on_touch(int x, int y, unsigned int touch_idx, bool pressed) override
    {
        platform::ios_touch(x, y, touch_idx, pressed);
    }

private:
    nya_render::render_buffered *m_render = nullptr;
    game_thread *m_gt = nullptr;
    platform m_platform;

    //MFi gamepad: when a controller is connected it takes priority over
    //touch controls, using the same joystick.xml mapping as desktop.
    joystick_config m_gamepad;
    bool m_gamepad_inited = false;
    bool m_prev_joy_pause = false;
    bool m_prev_b = false;
    bool m_menu_suppress = false; //suppress menu nav while B held after exit
};

int main()
{
    oh_ios_game_app app;
    app.start_fullscreen(0, 0, 0); //never returns: hands control to UIApplicationMain
    return 0;
}

#else //desktop (GLFW)

int main()
{
    setup_resources();

    config::register_var("screen_width", "1000");
    config::register_var("screen_height", "562");
    config::register_var("fullscreen", "false");
    config::register_var("master_volume", "10");
    config::register_var("music_volume", "5");
    config::register_var("difficulty", "hard");

    platform platform;
    if (!platform.init(config::get_var_int("screen_width"), config::get_var_int("screen_height"), "Open Horizon", config::get_var_bool("fullscreen")))
        return -1;

    //Per-GUID gamepad mapping for the macOS "Xbox Wireless Controller" layout.
    //The raw layout (Game Controller framework) puts the right stick on raw axes 3/4,
    //the analog triggers on 2/5 resting at -1, LS/RS on buttons 8/9 and guide on 10 -
    //none of which GLFW's built-in SDL DB covers, so joysticks reported via raw axes
    //had a resting trigger feeding camera_yaw (default view stuck 90 degrees left)
    //and the right stick unmapped. With this mapping glfwGetGamepadState reports the
    //standardized layout that joystick.xml expects. Derived with debug-work/joy_probe.c.
    glfwUpdateGamepadMappings(
        "030000005e040000e00200001b010000,Xbox Wireless Controller (open-horizon),"
        "platform:Mac OS X,"
        "a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,"
        "guide:b10,leftstick:b8,rightstick:b9,"
        "dpup:b11,dpright:b12,dpdown:b13,dpleft:b14,"
        "leftx:a0,lefty:a1~,rightx:a3,righty:a4~,lefttrigger:+a2,righttrigger:+a5");

    std::vector<joystick_config> joysticks;

    for (int i = 0; glfwJoystickPresent(i); ++i)
    {
        const char *name = glfwGetJoystickName(i);

        joystick_config j;
        j.init(name);
        joysticks.push_back(j);

        int axis_count = 0, buttons_count = 0;
        glfwGetJoystickAxes(i, &axis_count);
        glfwGetJoystickButtons(i, &buttons_count);
        nya_log::log("joy%d: %s %d axis %d buttons %s\n", i, name, axis_count, buttons_count,
                     glfwJoystickIsGamepad(i) ? "(standard gamepad mapping)" :
                                                "(no gamepad mapping - raw axes, order varies by connection)");
    }
    auto render = nya_render::render_buffered(nya_render::get_api_interface());
    nya_render::set_render_api(&render);

    game_thread game_thread(render, platform.get_frame_width(), platform.get_frame_height());


    auto controls = game::plane_controls();
    auto menu_controls = gui::menu_controls();
    while (!game_thread.platform_terminate)
    {
        //poll input on EVERY pass (~1ms), not once per game frame: previously
        //the gamepad was polled after end_frame inside this frame-gated loop,
        //so with a slow game frame a button press waited a full frame before it
        //was even sampled. Button flags accumulate (OR) between game frames,
        //analog axes take the latest value - short taps can never be missed.
        bool joy_pause = false;
        for (int i = 0; i < (int)joysticks.size(); ++i)
        {
            //prefer GLFW's standardized gamepad layout: raw joystick axis ORDER and
            //trigger ranges differ per platform and per connection type (USB vs
            //Bluetooth report different indices), which made a resting trigger feed
            //the camera_yaw mapping and pulled the default view 90 degrees left.
            //The gamepad state always reports [LX,LY,RX,RY,LT(0..1),RT(0..1)] and
            //buttons [A,B,X,Y,LB,RB,back,start,guide,LS,RS,dpad...].
            GLFWgamepadstate gp;
            if (glfwGetGamepadState(i, &gp))
                joysticks[i].update(gp.axes, GLFW_GAMEPAD_AXIS_LAST + 1,
                                    gp.buttons, GLFW_GAMEPAD_BUTTON_LAST + 1);
            else
            {
                //no standardized mapping available (this GLFW build rejects the
                //mapping string injected above for unknown reasons), so rewrite
                //the KNOWN raw layout of this controller ourselves. Measured with
                //debug-work/joy_probe.c on macOS (Game Controller framework):
                //   raw axes   [LX=0, LY=1(down=+1), LT=-1..1@2, RX=3, RY=4(down=+1), RT=-1..1@5]
                //   raw buttons[A,B,X,Y=0-3, LB=4, RB=5, View=6, Start=7, LS=8, RS=9,
                //               guide=10(reports stuck pressed), dpad=11-14]
                // which becomes the standard layout (sticks +1=up, triggers 0..1):
                //   [LX, LY, RX, RY, LT, RT] / [A,B,X,Y,LB,RB,View,Start,Guide,LS,RS,dpad]
                const char *jname = glfwGetJoystickName(i);
                int axes_count = 0, buttons_count = 0;
                const float *raw_axes = glfwGetJoystickAxes(i, &axes_count);
                const unsigned char *raw_buttons = glfwGetJoystickButtons(i, &buttons_count);

                if (jname && strstr(jname, "Xbox Wireless") &&
                    axes_count == 6 && buttons_count == 15)
                {
                    float std_axes[6] =
                    {
                        raw_axes[0],                     // LX
                        -raw_axes[1],                    // LY: +1 = up
                        raw_axes[3],                     // RX
                        -raw_axes[4],                    // RY: +1 = up
                        (raw_axes[2] + 1.0f) * 0.5f,     // LT: -1..1 -> 0..1
                        (raw_axes[5] + 1.0f) * 0.5f      // RT: -1..1 -> 0..1
                    };
                    //std button -> raw button (guide left unmapped: raw #10
                    //reports stuck pressed and nothing binds to it)
                    static const int raw_of_std[15] =
                        { 0, 1, 2, 3, 4, 5, 6, 7, -1, 8, 9, 11, 12, 13, 14 };
                    unsigned char std_buttons[15] = { 0 };
                    for (int b = 0; b < 15; ++b)
                        if (raw_of_std[b] >= 0)
                            std_buttons[b] = raw_buttons[raw_of_std[b]];

                    joysticks[i].update(std_axes, 6, std_buttons, 15);
                }
                else
                    joysticks[i].update(raw_axes, axes_count, raw_buttons, buttons_count);
            }
            joysticks[i].apply_controls(controls, joy_pause);
            joysticks[i].apply_controls(menu_controls);

            //debug: OH_JOYLOG=1 dumps raw + standardized axes every 250ms (stderr),
            //to pin down axis order on controllers without a gamepad mapping
            static double joylog_last = 0.0;
            if (getenv("OH_JOYLOG") && glfwGetTime() - joylog_last > 0.25)
            {
                joylog_last = glfwGetTime();
                int ac = 0;
                const float *raw = glfwGetJoystickAxes(i, &ac);
                fprintf(stderr, "JOY%d raw:", i);
                for (int a = 0; a < ac; ++a)
                    fprintf(stderr, " %.2f", raw ? raw[a] : 0.0f);
                if (glfwGetGamepadState(i, &gp))
                {
                    fprintf(stderr, "  std:");
                    for (int a = 0; a <= GLFW_GAMEPAD_AXIS_LAST; ++a)
                        fprintf(stderr, " %.2f", gp.axes[a]);
                }
                fprintf(stderr, "\n");
            }
        }

        if (platform.get_key(GLFW_KEY_W)) controls.throttle = 1.0f;
        if (platform.get_key(GLFW_KEY_S)) controls.brake = 1.0f;
        if (platform.get_key(GLFW_KEY_A)) controls.rot.y = -1.0f;
        if (platform.get_key(GLFW_KEY_D)) controls.rot.y = 1.0f;
        if (platform.get_key(GLFW_KEY_UP)) controls.rot.x = 1.0f, menu_controls.up = true;
        if (platform.get_key(GLFW_KEY_DOWN)) controls.rot.x = -1.0f, menu_controls.down = true;
        if (platform.get_key(GLFW_KEY_LEFT)) controls.rot.z = -1.0f, menu_controls.left = true;
        if (platform.get_key(GLFW_KEY_RIGHT)) controls.rot.z = 1.0f, menu_controls.right = true;

        if (platform.get_key(GLFW_KEY_LEFT_CONTROL)) controls.mgun = true;
        if (platform.get_key(GLFW_KEY_LEFT_SHIFT)) controls.mgun = true;
        if (platform.get_key(GLFW_KEY_SPACE)) controls.missile = true, menu_controls.next = true;
        if (platform.get_key(GLFW_KEY_F)) controls.flares = true;
        if (platform.get_key(GLFW_KEY_Q)) controls.change_weapon = true;
        if (platform.get_key(GLFW_KEY_E)) controls.change_target = true;
        if (platform.get_key(GLFW_KEY_R)) controls.change_radar = true;
        if (platform.get_key(GLFW_KEY_V)) controls.change_camera = true;

        //debug: OH_CAMSTICK="pitch,yaw" synthesizes right-stick camera input,
        //to exercise the free-look pipeline without a gamepad attached
        static float camstick_x = 0.0f, camstick_y = 0.0f;
        static bool camstick_init = false;
        if (!camstick_init)
        {
            const char *v = getenv("OH_CAMSTICK");
            if (v)
                sscanf(v, "%f,%f", &camstick_x, &camstick_y);
            camstick_init = true;
        }
        if (camstick_x != 0.0f || camstick_y != 0.0f)
        {
            controls.cam_rot.x = camstick_x;
            controls.cam_rot.y = camstick_y;
        }

        if(!game_thread.frame_pending)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        render.push();

        game_thread.controls_update = controls;
        game_thread.menu_controls_update = menu_controls;
        game_thread.frame_pending = false;

        //input handed off: accumulate the next game frame from a clean state
        controls = game::plane_controls();
        menu_controls = gui::menu_controls();

        render.execute();
        platform.end_frame();

        if(platform.should_terminate())
            game_thread.platform_terminate = true;
        game_thread.platform_width = platform.get_frame_width();
        game_thread.platform_height = platform.get_frame_height();
        game_thread.platform_window_width = platform.get_window_width();
        game_thread.platform_window_height = platform.get_window_height();
        if (game_thread.platform_fullscreen)
        {
            platform.set_fullscreen(config::get_var_bool("fullscreen"), config::get_var_int("screen_width"), config::get_var_int("screen_height"));
            game_thread.platform_fullscreen = false;
        }

        game_thread.mouse_l = platform.get_mouse_lbtn();
        game_thread.mouse_r = platform.get_mouse_rbtn();
        game_thread.mouse_x = platform.get_mouse_x();
        game_thread.mouse_y = platform.get_mouse_y();

        //edge-detected keys: read exactly once per displayed frame; end_frame()
        //refreshes m_last_buttons, reading more often would re-trigger
        if (platform.was_pressed(GLFW_KEY_ESCAPE)) menu_controls.prev = true, game_thread.exit = true;
        if (platform.was_pressed(GLFW_KEY_ENTER)) menu_controls.next = true;

        if (platform.was_pressed(GLFW_KEY_P)) game_thread.paused = !game_thread.paused;

        if (platform.was_pressed(GLFW_KEY_COMMA))
            debug_variable::set(debug_variable::get() - 1);
        if (platform.was_pressed(GLFW_KEY_PERIOD))
            debug_variable::set(debug_variable::get() + 1);
    }

    nya_render::set_render_api(new nya_render::render_api_interface()); //ToDo
    game_thread.thread.join();
    platform.terminate();
    return 0;
}

#endif //OH_PLATFORM_IOS

//------------------------------------------------------------
