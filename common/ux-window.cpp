#include "ux-window.h"
#include "model-views.h"

// We use STB image to load the splash-screen from memory
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
// int-rs-splash.hpp contains the PNG image from res/int-rs-splash.png
#include "res/int-rs-splash.hpp"

#include "tclap/CmdLine.h"

#ifdef UI_SCRIPTING_ENABLED
#include <regex>
#include "TinyJS.h"
#include "TinyJS_Functions.h"
#include "TinyJS_MathFunctions.h"
#include "../src/concurrency.h"
#endif

using namespace TCLAP;

namespace rs2
{
    class internal_automation : public automation
    {
    public:
        virtual bool to_exit() = 0;
        virtual void on_frame(bool start) = 0;
    };

#ifdef UI_SCRIPTING_ENABLED
    void js_sleep(CScriptVar *v, void *userdata) {
        auto ms = v->getParameter("ms")->getInt();
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    void js_log(CScriptVar *v, void *userdata);
    void js_button_click(CScriptVar *v, void *userdata);
    void js_fail(CScriptVar *v, void *userdata);
    void js_pass(CScriptVar *v, void *userdata);
    void js_find_element(CScriptVar *v, void *userdata);
    void js_get_elements(CScriptVar *v, void *userdata);
    void js_get_text(CScriptVar *v, void *userdata);
    void js_get_list(CScriptVar *v, void *userdata);
    void js_set_value(CScriptVar *v, void *userdata);

    class scripting_engine : public internal_automation
    {
    public:
        scripting_engine(std::string content)
            : _content(content),
            _thread([this]() { thread_function(); })
        {
        }

        void on_frame(bool start) override
        {
            auto queue = &_on_frame_end;
            if (start) queue = &_on_frame_start;
            std::vector<std::function<void()>> actions;
            std::function<void()> action;
            while (queue->try_dequeue(&action))
                actions.push_back(action);
            for (auto&& action : actions) action();
        }

        std::string get_text(std::string id)
        {
            std::string result;
            invoke_on_frame([&]() {
                ImGui::QueryElement(id.c_str());
            }, [&]() {
                result = ImGui::ReadElementValue();
            });
            return result;
        }

        std::vector<std::string> collect_ids()
        {
            std::vector<std::string> results;
            invoke_on_frame([&]() {
                ImGui::BeginReflection();
            }, [&]() {
                results = ImGui::EndReflection();
            });
            return results;
        }

        std::vector<std::string> get_list(std::string id)
        {
            std::vector<std::string> results;
            invoke_on_frame([&]() {
                ImGui::QueryElement(id.c_str());
            }, [&]() {
                results = ImGui::ReadElementValues();
            });
            return results;
        }

        void set_value(std::string id, double val)
        {
            invoke_on_frame([&]() {
                ImGui::SetElementValue(id.c_str(), val);
            });
        }

        void send_log(std::string line)
        {
            std::unique_lock<std::mutex> lock(_lock);
            _log.push_back(line);
            std::cout << line << std::endl;
            _log_lines.enqueue(std::move(line));
        }

        bool read_log(std::string& line) override
        {
            return _log_lines.try_dequeue(&line);
        }

        void send_button_click(const std::string& id)
        {
            std::string result;
            invoke_on_frame([&]() {
                ImGui::SignalElement(id.c_str());
            });
        }

        void fail_test(const std::string& message)
        {
            std::unique_lock<std::mutex> lock(_lock);
            _log.push_back(to_string() << "FAILED: " << message);
            _passed = false;
            _to_exit = true;
        }

        void pass_test(const std::string& id)
        {
            exit(0);
        }

        void invoke_on_frame(std::function<void()> on_start = []() {},
                             std::function<void()> on_end = []() {})
        {
            _on_frame_end.enqueue([&]() {
                _on_frame_start.enqueue([&]() {
                    on_start();
                    _acks.enqueue(true);
                });
                _on_frame_end.enqueue([&]() {
                    on_end();
                    _acks.enqueue(true);
                });
            });
            
            bool res;
            if (!_acks.dequeue(&res) || !_acks.dequeue(&res))
                throw new CScriptException("UI unresponsive!");
        }

        bool to_exit() override 
        {
            std::unique_lock<std::mutex> lock(_lock);
            return _to_exit; 
        }

        ~scripting_engine() override { 
            _thread.join(); 
        }

        CTinyJS* get_js() { return &_js; }
    private:
        void thread_function()
        {
            _js.addNative("function sleep(ms)", &js_sleep, 0);
            _js.addNative("function log(message)", &js_log, this);
            _js.addNative("function click_button(id)", &js_button_click, this);
            _js.addNative("function fail(message)", &js_fail, this);
            _js.addNative("function pass(message)", &js_pass, this);
            _js.addNative("function find_element(id)", &js_find_element, this);
            _js.addNative("function find_nth_element(id, number)", &js_find_element, this);
            _js.addNative("function get_elements()", &js_get_elements, this);
            _js.addNative("function get_text(id)", &js_get_text, this);
            _js.addNative("function get_list(id)", &js_get_list, this);
            _js.addNative("function set_value(id, val)", &js_set_value, this);
            registerFunctions(&_js);
            registerMathFunctions(&_js);

            {
                invoke_on_frame();
            }

            send_log("Running script...");

            try
            {
                _js.execute(_content.c_str());
            }
            catch (CScriptException* e)
            {
                send_log(to_string() << "ERROR: " << e->text);
            }

            send_log("Script Execution Halted");

            if (!_passed)
            {
                std::ofstream report;
                report.open("report.txt");
                for (auto&& line : _log)
                    report << line << "\n";
                report.close();

                exit(-1);
            }
        }

        single_consumer_queue<std::string> _log_lines;
        single_consumer_queue<std::function<void()>> _on_frame_start;
        single_consumer_queue<std::function<void()>> _on_frame_end;
        single_consumer_queue<bool> _acks;

        bool _to_exit = false;
        bool _passed = true;

        std::string _content;
        CTinyJS _js;
        std::thread _thread;
        std::mutex _lock;
        std::vector<std::string> _log;
    };

    void js_log(CScriptVar *v, void *userdata) {
        auto msg = v->getParameter("message")->getString();
        auto that = (scripting_engine*)userdata;
        that->send_log(msg);
    }

    void js_fail(CScriptVar *v, void *userdata) {
        auto msg = v->getParameter("message")->getString();
        auto that = (scripting_engine*)userdata;
        that->fail_test(msg);
        throw new CScriptException("Test failed");
    }

    void js_pass(CScriptVar *v, void *userdata) {
        auto msg = v->getParameter("message")->getString();
        auto that = (scripting_engine*)userdata;
        that->pass_test(msg);
    }

    void js_button_click(CScriptVar *v, void *userdata)
    {
        auto id = v->getParameter("id")->getString();
        auto that = (scripting_engine*)userdata;
        that->send_button_click(id);
    }

    void js_get_elements(CScriptVar *v, void *userdata)
    {
        auto that = (scripting_engine*)userdata;
        auto ids = that->collect_ids();
        v->getReturnVar()->setArray();
        int index = 0;
        for (auto&& id : ids)
        {
            auto t = new CScriptVar(id);
            v->getReturnVar()->setArrayIndex(index, t);
            index++;
        }
    }

    void js_get_list(CScriptVar *v, void *userdata)
    {
        auto that = (scripting_engine*)userdata;
        auto id = v->getParameter("id")->getString();
        auto ids = that->get_list(id);
        v->getReturnVar()->setArray();
        int index = 0;
        for (auto&& id : ids)
        {
            auto t = new CScriptVar(id);
            v->getReturnVar()->setArrayIndex(index, t);
            index++;
        }
    }

    void js_set_value(CScriptVar *v, void *userdata)
    {
        auto that = (scripting_engine*)userdata;
        auto id = v->getParameter("id")->getString();
        auto val = v->getParameter("val")->getDouble();
        that->set_value(id, val);
    }

    void js_get_text(CScriptVar *v, void *userdata)
    {
        auto id = v->getParameter("id")->getString();
        auto that = (scripting_engine*)userdata;
        v->getReturnVar()->setString(that->get_text(id));
    }

    void js_find_element(CScriptVar *v, void *userdata)
    {
        auto selected = v->getParameter("id")->getString();
        auto number = 1;
        if (v->getParameter("number")->isNumeric())
        {
            number = v->getParameter("number")->getInt();
        }
        auto that = (scripting_engine*)userdata;
        auto ids = that->collect_ids();
        for (auto&& id : ids)
        {
            std::regex dotdot("\\.\\.\\.");

            std::string new_selected;
            std::regex_replace(std::back_inserter(new_selected), 
                selected.begin(), selected.end(), dotdot, ")(.|\\r|\\n)*(");

            std::string pattern = to_string() 
                << "(.|\\r|\\n)*(" << new_selected << ")(.|\\r|\\n)*";
            std::regex e(pattern);

            if (std::regex_match(id, e))
            {
                number--;
                if (number == 0)
                {
                    v->getReturnVar()->setString(id);
                    return;
                }
            }
        }
    }
#endif

    class empty_scripting_engine : public internal_automation
    {
    public:
        empty_scripting_engine() {}

        bool read_log(std::string& line) override { return false; }
        bool to_exit() override { return false; }
        void on_frame(bool) override {}
    private:
    };

    void ux_window::open_window()
    {
        if (_win)
        {
            ImGui::GetIO().Fonts->ClearFonts();  // To be refactored into Viewer theme object
            ImGui_ImplGlfw_Shutdown();
            glfwDestroyWindow(_win);
        }

        rs2_error* e = nullptr;
        _title_str = to_string() << _title << " v" << api_version_to_string(rs2_get_api_version(&e));

        _width = 1024;
        _height = 768;

        // Dynamically adjust new window size (by detecting monitor resolution)
        auto primary = glfwGetPrimaryMonitor();
        if (primary)
        {
            const auto mode = glfwGetVideoMode(primary);
            if (_fullscreen)
            {
                _width = mode->width;
                _height = mode->height;
            }
            else
            {
                _width = int(mode->width * 0.7f);
                _height = int(mode->height * 0.7f);
            }
        }

        // Create GUI Windows
        _win = glfwCreateWindow(_width, _height, _title_str.c_str(),
            (_fullscreen ? primary : nullptr), nullptr);
        if (!_win)
            throw std::runtime_error("Could not open OpenGL window, please check your graphic drivers or use the textual SDK tools");

        glfwMakeContextCurrent(_win);
        ImGui_ImplGlfw_Init(_win, true);

        // Load fonts to be used with the ImGui - TODO move to RAII
        imgui_easy_theming(_font_14, _font_18);

        // Register for UI-controller events
        glfwSetWindowUserPointer(_win, this);


        glfwSetCursorPosCallback(_win, [](GLFWwindow* w, double cx, double cy)
        {
            auto data = reinterpret_cast<ux_window*>(glfwGetWindowUserPointer(w));
            data->_mouse.cursor = { (float)cx / data->_scale_factor,
                (float)cy / data->_scale_factor };
        });
        glfwSetMouseButtonCallback(_win, [](GLFWwindow* w, int button, int action, int mods)
        {
            auto data = reinterpret_cast<ux_window*>(glfwGetWindowUserPointer(w));
            data->_mouse.mouse_down = (button == GLFW_MOUSE_BUTTON_1) && (action != GLFW_RELEASE);
        });
        glfwSetScrollCallback(_win, [](GLFWwindow * w, double xoffset, double yoffset)
        {
            auto data = reinterpret_cast<ux_window*>(glfwGetWindowUserPointer(w));
            data->_mouse.mouse_wheel = static_cast<int>(yoffset);
            data->_mouse.ui_wheel += static_cast<int>(yoffset);
        });

        glfwSetDropCallback(_win, [](GLFWwindow* w, int count, const char** paths)
        {
            auto data = reinterpret_cast<ux_window*>(glfwGetWindowUserPointer(w));

            if (count <= 0) return;

            for (int i = 0; i < count; i++)
            {
                data->on_file_drop(paths[i]);
            }
        });
    }

    ux_window::ux_window(const char* title, int argc, const char** argv) :
        _win(nullptr), _width(0), _height(0), _output_height(0),
        _font_14(nullptr), _font_18(nullptr), _app_ready(false),
        _first_frame(true), _query_devices(true), _missing_device(false),
        _hourglass_index(0), _dev_stat_message{}, _keep_alive(true), _title(title),
        _script(std::make_shared<empty_scripting_engine>())
    {
        CmdLine cmd(title, ' ', RS2_API_VERSION_STR);

        SwitchArg fullscreen_arg("F", "fullscreen", "Launch the app in full screen");
        cmd.add(fullscreen_arg);
#ifdef UI_SCRIPTING_ENABLED
        ValueArg<std::string> input_script("s", "script", "JavaScript filename", false, "", "input_file");
        cmd.add(input_script);
#endif

        cmd.parse(argc, argv);

        _fullscreen = fullscreen_arg.getValue();

#ifdef UI_SCRIPTING_ENABLED
        if (input_script.isSet())
        {
            auto script_filename = input_script.getValue();
            if (ends_with(to_lower(script_filename), ".js"))
            {
                std::ifstream file(script_filename);
                if (!file.good())
                    throw std::runtime_error(to_string() << "Cannot open \"" << script_filename << "\"!");

                std::string content((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());

                _script = std::make_shared<scripting_engine>(content);
            }
            else
            {
                throw std::runtime_error(to_string() << "Input script must have .js file extension!");
            }
        }
#endif

        if (!glfwInit())
        {
            throw std::runtime_error("Could not open OpenGL window, please check your graphic drivers or use the textual SDK tools");
        }

        open_window();

        // Prepare the splash screen and do some initialization in the background
        int x, y, comp;
        auto r = stbi_load_from_memory(splash, (int)splash_size, &x, &y, &comp, false);
        _splash_tex.upload_image(x, y, r);
        stbi_image_free(r);

        // Apply initial UI state
        reset();
    }

    void ux_window::add_on_load_message(const std::string& msg)
    {
        std::lock_guard<std::mutex> lock(_on_load_message_mtx);
        _on_load_message.push_back(msg);
    }

    // Check that the graphic subsystem is valid and start a new frame
    ux_window::operator bool()
    {
        end_frame();

        // Yield the CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        auto res = !glfwWindowShouldClose(_win);

        if (_first_frame)
        {
            assert(!_first_load.joinable()); // You must call to reset() before initiate new thread

            _first_load = std::thread([&]() {
                while (_keep_alive && !_app_ready)
                {
                    try
                    {
                        _app_ready = on_load();
                    }
                    catch (...)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait for connect event and retry
                    }
                }
            });
            _first_frame = false;
        }

        // If we are just getting started, render the Splash Screen instead of normal UI
        while (res && (!_app_ready || _splash_timer.elapsed_ms() < 1500.f))
        {
            res = !glfwWindowShouldClose(_win);
            glfwPollEvents();

            begin_frame();

            glPushMatrix();
            glViewport(0, 0, _fb_width, _fb_height);
            glClearColor(0.036f, 0.044f, 0.051f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);

            glLoadIdentity();
            glOrtho(0, _width, _height, 0, -1, +1);

            // Fade-in the logo
            auto opacity = smoothstep(float(_splash_timer.elapsed_ms()), 100.f, 2000.f);
            _splash_tex.show({ 0.f,0.f,float(_width),float(_height) }, opacity);

            std::string hourglass = u8"\uf250";
            static periodic_timer every_200ms(std::chrono::milliseconds(200));
            bool do_200ms = every_200ms;
            if (_query_devices && do_200ms)
            {
                _missing_device = rs2::context().query_devices().size() == 0;
                _hourglass_index = (_hourglass_index + 1) % 5;

                if (!_missing_device)
                {
                    _dev_stat_message = u8"\uf287 RealSense device detected.";
                    _query_devices = false;
                }
            }

            hourglass[2] += _hourglass_index;

            bool blink = sin(_splash_timer.elapsed_ms() / 150.f) > -0.3f;

            auto flags = ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoTitleBar;

            ImGui::PushStyleColor(ImGuiCol_Text, light_grey);
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, white);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 5, 5 });
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 1);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, transparent);
            ImGui::SetNextWindowPos({ (float)_width / 2 - 150, (float)_height / 2 + 70 });
            ImGui::PushFont(_font_18);
            ImGui::SetNextWindowSize({ (float)_width, (float)_height });
            ImGui::Begin("Splash Screen Banner", nullptr, flags);

            ImGui::Text("%s   Loading %s...", hourglass.c_str(), _title_str.c_str());

            {
                std::lock_guard<std::mutex> lock(_on_load_message_mtx);
                if (_on_load_message.empty() && blink)
                {
                    ImGui::Text("%s", _dev_stat_message.c_str());
                }
                else if (!_on_load_message.empty())
                {
                    ImGui::Text("%s", _dev_stat_message.c_str());
                    for (auto& msg : _on_load_message)
                    {
                        auto is_last_msg = (msg == _on_load_message.back());
                        if (is_last_msg && blink)
                            ImGui::Text("%s", msg.c_str());
                        else if (!is_last_msg)
                            ImGui::Text("%s", msg.c_str());
                    }
                }
            }

            ImGui::End();
            ImGui::PopFont();
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);

            end_frame();

            glPopMatrix();

            // Yield the CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // reset graphic pipe
        begin_frame();

        auto ptr = (internal_automation*)_script.get();
        ptr->on_frame(true);

        if (ptr->to_exit()) return false;

        return res;
    }

    ux_window::~ux_window()
    {
        if (_first_load.joinable())
        {
            _keep_alive = false;
            _first_load.join();
        }

        ImGui::GetIO().Fonts->ClearFonts();  // To be refactored into Viewer theme object
        ImGui_ImplGlfw_Shutdown();
        glfwDestroyWindow(_win);
        glfwTerminate();
    }

    void ux_window::begin_frame()
    {
        glfwPollEvents();

        int state = glfwGetKey(_win, GLFW_KEY_F8);
        if (state == GLFW_PRESS)
        {
            _fullscreen_pressed = true;
        }
        else
        {
            if (_fullscreen_pressed)
            {
                _fullscreen = !_fullscreen;
                open_window();
            }
            _fullscreen_pressed = false;
        }

        int w = _width; int h = _height;

        glfwGetWindowSize(_win, &_width, &_height);

        int fw = _fb_width;
        int fh = _fb_height;

        glfwGetFramebufferSize(_win, &_fb_width, &_fb_height);

        if (fw != _fb_width || fh != _fb_height)
        {
            std::string msg = to_string() << "Framebuffer size changed to " << _fb_width << " x " << _fb_height;
            rs2::log(RS2_LOG_SEVERITY_INFO, msg.c_str());
        }

        auto sf = _scale_factor;

        // Update the scale factor each frame
        // based on resolution and physical display size
        _scale_factor = static_cast<float>(pick_scale_factor(_win));
        _width = static_cast<int>(_width / _scale_factor);
        _height = static_cast<int>(_height / _scale_factor);

        if (w != _width || h != _height)
        {
            std::string msg = to_string() << "Window size changed to " << _width << " x " << _height;
            rs2::log(RS2_LOG_SEVERITY_INFO, msg.c_str());
        }

        if (_scale_factor != sf)
        {
            std::string msg = to_string() << "Scale Factor is now " << _scale_factor;
            rs2::log(RS2_LOG_SEVERITY_INFO, msg.c_str());
        }

        // Reset ImGui state
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        ImGui::GetIO().MouseWheel = _mouse.ui_wheel;
        _mouse.ui_wheel = 0.f;

        ImGui_ImplGlfw_NewFrame(_scale_factor);
    }

    void ux_window::begin_viewport()
    {
        // Rendering
        glViewport(0, 0,
            static_cast<int>(ImGui::GetIO().DisplaySize.x * _scale_factor),
            static_cast<int>(ImGui::GetIO().DisplaySize.y * _scale_factor));
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void ux_window::end_frame()
    {
        if (!_first_frame)
        {
            auto ptr = (internal_automation*)_script.get();
            ptr->on_frame(false);

            ImGui::Render();

            glfwSwapBuffers(_win);
            _mouse.mouse_wheel = 0;
        }
    }

    void ux_window::reset()
    {
        if (_first_load.joinable())
        {
            _keep_alive = false;
            _first_load.join();
            _keep_alive = true;
        }

        _query_devices = true;
        _missing_device = false;
        _hourglass_index = 0;
        _first_frame = true;
        _app_ready = false;
        _splash_timer.reset();
        _dev_stat_message = u8"\uf287 Please connect Intel RealSense device!";

        {
            std::lock_guard<std::mutex> lock(_on_load_message_mtx);
            _on_load_message.clear();
        }
    }
}
