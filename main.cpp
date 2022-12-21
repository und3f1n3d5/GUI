#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "imgui_internal.h"
#include <SFML/Graphics/Image.hpp>

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#include <vector>
#include <iostream>
#include <dirent.h>
#include <fstream>

#endif

typedef void (*ImGuiMarkerCallback)(const char* file, int line, const char* section, void* user_data);
extern ImGuiMarkerCallback      GImGuiMarkerCallback;
extern void*                        GImGuiMarkerCallbackUserData;
ImGuiMarkerCallback             GImGuiMarkerCallback = NULL;
void*                               GImGuiMarkerCallbackUserData = NULL;
#define IMGUI_DEMO_MARKER(section)  do { if (GImGuiMarkerCallback != NULL) GImGuiMarkerCallback(__FILE__, __LINE__, section, GImGuiMarkerCallbackUserData); } while (0)



// structures for work

struct Point {
    Point(const float d, const float d1, float d2) : x(d), y(d1), scale(d2) {}

    float x, y, scale;
};

struct Picture {
    int image_width = 0;
    int image_height = 0;
    GLuint image_texture = 0;
};

struct State {
    bool done = false;
    bool drawing = false;
    bool selecting = false;
    bool directory_found = false;
    bool file_found = false;
    int pic_i = 0;
    float scale = 1.0;
    std::string file;
    std::string directory;
    int pic_num = 0;
    std::vector<Picture> pictures;
    std::vector<Point> points;
    std::string key_pressed;
};

// Images

bool LoadTextureFromFile(const std::string filename, Picture& pic)
{
    // Load from file
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load(filename.c_str(), &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
        return false;

    // Create a OpenGL texture identifier
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    pic.image_texture = image_texture;
    pic.image_width = image_width;
    pic.image_height = image_height;

    return true;
}

int selected_point = -1;
float R = 3.0;
static void ShowAppCustomRendering(const Picture& pic,
                                   State& st) {

    auto ctx = ImGui::GetCurrentContext();

    //static ImVector<ImVec2> points;
    static ImVec2 scrolling(0.0f, 0.0f);
    static bool opt_enable_grid = true;
    static bool opt_enable_context_menu = true;
    static bool adding_line = false;

    // Using InvisibleButton() as a convenience 1) it will advance the layout cursor and 2) allows us to use IsItemHovered()/IsItemActive()
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
    ImVec2 canvas_sz(pic.image_width * st.scale, pic.image_height * st.scale);
    if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    // Draw border and background color
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    io.FontAllowUserScaling = true;
    //IO.FontAllowScaling

    draw_list->AddImage((void*)(intptr_t)pic.image_texture, canvas_p0, canvas_p1);

    // This will catch our interactions
    ImGui::InvisibleButton("canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool is_hovered = ImGui::IsItemHovered(); // Hovered
    const bool is_active = ImGui::IsItemActive();   // Held
    const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y); // Lock scrolled origin
    const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

    // Add first and second point
    if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && st.drawing) {
        st.points.emplace_back(mouse_pos_in_canvas.x, mouse_pos_in_canvas.y, st.scale);
    }

    if (st.selecting && (selected_point == -1) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        for (int i=0; i < st.points.size(); ++i) {
            if (std::abs(mouse_pos_in_canvas.x - st.points[i].x) <= R && std::abs(mouse_pos_in_canvas.y - st.points[i].y) < R) {
                selected_point = i;
                break;
            }
        }
    }

    if (st.selecting && (selected_point != -1) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        selected_point = -1;
    }

    if (st.selecting && (selected_point != -1) && st.key_pressed == "Backspace") {
        st.points.erase(st.points.begin() + selected_point);
        selected_point = -1;
    }

    if (st.selecting && (selected_point != -1)) {
        st.points[selected_point].x = mouse_pos_in_canvas.x;
        st.points[selected_point].y = mouse_pos_in_canvas.y;
    }

    // Context menu (under default mouse threshold)
    ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

    // Draw grid + all lines in the canvas
    draw_list->PushClipRect(canvas_p0, canvas_p1, true);
    if (opt_enable_grid)
    {
        const float GRID_STEP = 64.0f;
        for (float x = fmodf(scrolling.x, GRID_STEP); x < canvas_sz.x; x += GRID_STEP)
            draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y), ImVec2(canvas_p0.x + x, canvas_p1.y), IM_COL32(200, 200, 200, 40));
        for (float y = fmodf(scrolling.y, GRID_STEP); y < canvas_sz.y; y += GRID_STEP)
            draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y), ImVec2(canvas_p1.x, canvas_p0.y + y), IM_COL32(200, 200, 200, 40));
    }

    /*std::sort(st.points.begin(), st.points.end(), [](Point& a, Point& b) {
        return a.x / a.scale < b.x / b.scale || (a.x / a.scale == b.x / b.scale && a.y / a.scale < b.y / b.scale);
    });*/
    for (int n = 0; n < st.points.size(); n += 1) {
        if (st.selecting && selected_point == n) {
            draw_list->AddCircleFilled(ImVec2(origin.x + st.points[n].x * st.scale / st.points[n].scale,
                                              origin.y + st.points[n].y * st.scale / st.points[n].scale), R,
                                       IM_COL32(255, 0, 0, 255));
        } else {
            draw_list->AddCircleFilled(ImVec2(origin.x + st.points[n].x * st.scale / st.points[n].scale,
                                              origin.y + st.points[n].y * st.scale / st.points[n].scale), R,
                                       IM_COL32(255, 255, 0, 255));
        }
        if (n < st.points.size() - 1) {
            draw_list->AddLine(ImVec2(origin.x + st.points[n].x  * st.scale / st.points[n].scale, origin.y +
                                                                        st.points[n].y * st.scale / st.points[n].scale),
                           ImVec2(origin.x + st.points[n + 1].x * st.scale / st.points[n + 1].scale, origin.y +
                                            st.points[n + 1].y * st.scale / st.points[n + 1].scale), IM_COL32(255, 255, 0, 255),
                           1.0f);
            //draw_list->PopClipRect();
        }
    }

}

// Files

void OpenPictures(const std::string folder, State& st) {
    //readdir
    st.pic_num = 0;
    //std::string folder = "/home/dmitrij/CLionProjects/yourbunnywrote/images";
    DIR *dir = opendir(folder.c_str());
    std::vector<std::string> pic_names;
    if (dir) {
        struct dirent *ent;
        while((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name.ends_with(".png")) {
                pic_names.emplace_back(ent->d_name);
                ++st.pic_num;
            }
        }
    }
    else {
        fprintf(stderr, "Error opening directory\n");
    }
    st.pictures.resize(st.pic_num);
    for (int i = 0; i < st.pic_num; ++i) {
        bool ret = LoadTextureFromFile(std::string(folder) + "/" + pic_names[i], st.pictures[i]);
        IM_ASSERT(ret);
    }
}

void OpenFile(const std::string file, State& st) {
    std::ifstream f;
    f.open(file);
    std::string str;
    float x;
    while(f >> x){
        Point point(x, 0, 1);
        f >> point.y;
        f >> point.scale;
        st.points.push_back(point);
    }
    f.close();
}

void WriteFile(std::string& file, State& st) {
    std::ofstream f;
    f.open(file);
    for (auto & point : st.points) {
        f << point.x << " " << point.y << " " << point.scale << std::endl;
    }
    f.close();
}



// Menus

static void ShowAppMainMenuBar(State &st)
{
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Input directory"))
        {
            ImGui::BeginChild("child", ImVec2(500, 60), false);
            static char dir_name[256] = "/home/dmitrij/images";
            ImGui::Text("Directory with pictures: ");
            ImGui::SameLine();
            ImGui::InputText(":", dir_name, IM_ARRAYSIZE(dir_name));
            if (ImGui::Button("Search directory")) {
                try {
                    st.directory = dir_name;
                    OpenPictures(st.directory, st);
                    st.directory_found = true;
                } catch (...) {
                    st.directory_found = false;
                }
            }
            if (st.pictures.empty()) {
                st.pictures.emplace_back();
            } else if (st.pictures.size() > 1 && st.pictures[0].image_texture == 0) {
                st.pictures.erase(st.pictures.begin());
            }
            ImGui::EndChild();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Input file")) {
            ImGui::BeginChild("child", ImVec2(500, 60), false);
            static char file_name[256] = "/home/dmitrij/dummy.txt";
            ImGui::Text("File with points: ");
            ImGui::SameLine();
            ImGui::InputText(":", file_name, IM_ARRAYSIZE(file_name));
            if (ImGui::Button("Search file")) {
                try {
                    if (!std::string(file_name).empty()) {
                        st.file = file_name;
                        OpenFile(st.file, st);
                        st.file_found = true;
                    }
                } catch (...) {
                    st.file_found = false;
                }
            }
            ImGui::EndChild();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            // Buttons
            if (ImGui::MenuItem("Select", "Ctrl+A") || ((st.key_pressed == "A") &&io.KeyCtrl)) {
                st.selecting = !st.selecting;
                st.key_pressed = "";
                if (st.selecting)
                    st.drawing = false;
            }
            if (ImGui::MenuItem("Zoom in", "Ctrl+") || ((st.key_pressed == "Equal") &&io.KeyCtrl)) {
                st.scale += 0.1;
                st.key_pressed = "";
            }
            if (ImGui::MenuItem("Zoom out", "Ctrl-") || ((st.key_pressed == "Minus") &&io.KeyCtrl)) {
                st.scale -= 0.1;
                st.scale = std::max(0.1f, st.scale);
            };
            if (ImGui::MenuItem("Draw", "Ctrl+D") || (io.KeyCtrl && st.key_pressed == "D")) {
                st.drawing = !st.drawing;
                if (st.drawing)
                    st.selecting = false;
            }
            if (ImGui::MenuItem("Next Picture", "PgDn") || st.key_pressed == "PageDown") {
                st.pic_i += 1;
                st.key_pressed = "";
            }
            if (ImGui::MenuItem("Previous picture", "PgUp") || st.key_pressed == "PageUp") {
                st.pic_i -= 1;
                st.key_pressed = "";
            }
            if (ImGui::MenuItem("Save", "Ctrl+S") || (io.KeyCtrl && st.key_pressed == "S")) {
                WriteFile(st.file, st);
                st.key_pressed = "";
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

//



static void AddButton(ImGuiIO& io) {
    static int pressed_count = 0;
    ImTextureID my_tex_id = io.Fonts->TexID;
    float my_tex_w = (float)io.Fonts->TexWidth;
    float my_tex_h = (float)io.Fonts->TexHeight;
    ImGui::PushID(0);
    ImVec2 size = ImVec2(32.0f, 32.0f);                         // Size of the image we want to make visible
    ImVec2 uv0 = ImVec2(0.0f, 0.0f);                            // UV coordinates for lower-left
    ImVec2 uv1 = ImVec2(32.0f / my_tex_w, 32.0f / my_tex_h);    // UV coordinates for (32,32) in our texture
    ImVec4 bg_col = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);             // Black background
    ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);           // No tint
    if (ImGui::ImageButton("Button", my_tex_id, size, uv0, uv1, bg_col, tint_col))
        pressed_count += 1;
    ImGui::PopID();
    ImGui::SameLine();

    ImGui::NewLine();
    ImGui::Text("Pressed %d times.", pressed_count);
}

void ShowHintsWindow() {
    static bool no_titlebar = false;
    static bool no_scrollbar = false;
    static bool no_menu = false;
    static bool no_move = false;
    static bool no_resize = false;
    static bool no_collapse = false;
    static bool no_close = false;
    static bool no_nav = false;
    static bool no_background = false;
    static bool no_bring_to_front = false;
    static bool unsaved_document = false;
    bool open = false;

    ImGuiWindowFlags window_flags = 0;
    if (no_titlebar)        window_flags |= ImGuiWindowFlags_NoTitleBar;
    if (no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
    if (!no_menu)           window_flags |= ImGuiWindowFlags_MenuBar;
    if (no_move)            window_flags |= ImGuiWindowFlags_NoMove;
    if (no_resize)          window_flags |= ImGuiWindowFlags_NoResize;
    if (no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
    if (no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
    if (no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
    if (no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (unsaved_document)   window_flags |= ImGuiWindowFlags_UnsavedDocument;

    //todo
    ImGui::Begin("Hints", &open, window_flags);
    ImGui::Text("Ctrl+D - enable/disable drawing points");
    ImGui::Text("Ctrl+Z - Undo");
    ImGui::Text("Ctrl+S - Save points");
    ImGui::Text("Ctrl+ - Zoom in");
    ImGui::Text("Ctrl- - Zoom out");
    ImGui::Text("PgUp - Next picture");
    ImGui::Text("PgDn - Previous picture");
    ImGui::Text("Ctrl+Backspace - Delete all points");
    ImGui::End();
}

// Main code
int main()
{
    auto c = GL_RESCALE_NORMAL;
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to the latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL3 example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGuiStyle& st = ImGui::GetStyle();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Dear ImGui style
    //ImGui::StyleColorsDark();
    ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Our state
    bool show_demo_window = false;
    bool show_another_window = false;
    bool show_child = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    State state;
    state.pictures.emplace_back();
    while (!state.done) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                state.done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                state.done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        /*if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);*/

        // Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static bool no_titlebar = false;
            static bool no_scrollbar = false;
            static bool no_menu = false;
            static bool no_move = false;
            static bool no_resize = false;
            static bool no_collapse = false;
            static bool no_close = false;
            static bool no_nav = false;
            static bool no_background = false;
            static bool no_bring_to_front = false;
            static bool unsaved_document = false;
            bool open = false;

            ImGuiWindowFlags window_flags = 0;
            if (no_titlebar)        window_flags |= ImGuiWindowFlags_NoTitleBar;
            if (no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
            if (!no_menu)           window_flags |= ImGuiWindowFlags_MenuBar;
            if (no_move)            window_flags |= ImGuiWindowFlags_NoMove;
            if (no_resize)          window_flags |= ImGuiWindowFlags_NoResize;
            if (no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
            if (no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
            if (no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
            if (no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
            if (unsaved_document)   window_flags |= ImGuiWindowFlags_UnsavedDocument;


            bool* p_open = &open;
            ImGui::Begin("Settings", &open, window_flags);// Create a window called "Hello, world!" and append into it.

            // Keys from keyboard
            struct funcs { static bool IsLegacyNativeDupe(ImGuiKey key) { return key < 512 && ImGui::GetIO().KeyMap[key] != -1; } }; // Hide Native<>ImGuiKey duplicates when both exists in the array
            const ImGuiKey key_first = (ImGuiKey)0;
            for (ImGuiKey key = key_first; key < ImGuiKey_COUNT; key = (ImGuiKey)(key + 1)) {
                if (funcs::IsLegacyNativeDupe(key))
                    continue;
                if (ImGui::IsKeyPressed(key)) {
                    state.key_pressed = std::string(ImGui::GetKeyName(key));
                }
            }

            ShowAppMainMenuBar(state);


            if (state.pictures.empty()) {
                state.pictures.emplace_back();
            } else if (state.pictures.size() > 1 && state.pictures[0].image_texture == 0) {
                state.pictures.erase(state.pictures.begin());
            }
            state.pic_num = state.pictures.size();

            if (!state.directory_found) {
                ImGui::Text("Directory not found :(");
            }
            if (!state.file_found) {
                ImGui::Text("File not found :(");
                ImGui::Text("Input the correct file to draw & save points");
            }

            // Show points
            if (state.key_pressed == "Backspace" && io.KeyCtrl) {
                state.points.clear();
                selected_point = -1;
                state.key_pressed = "";
            }
            if (io.KeyCtrl && state.key_pressed == "S") {
                WriteFile(state.file, state);
                state.key_pressed = "";
            }
            if (state.key_pressed == "A" && io.KeyCtrl) {
                state.selecting = !state.selecting;
                if (state.selecting)
                    state.drawing = false;
                state.key_pressed = "";
            }
            if (state.key_pressed == "Equal" && io.KeyCtrl) {
                state.scale += 0.1;
                state.key_pressed = "";
            }
            if (state.key_pressed == "Minus" && io.KeyCtrl) {
                state.scale -= 0.1;
                state.scale = std::max(0.1f, state.scale);
                state.key_pressed = "";
            }
            if (io.KeyCtrl && state.key_pressed == "D") {
                state.drawing = !state.drawing;
                if (state.drawing)
                    state.selecting = false;
            }
            if (state.key_pressed == "PageDown") {
                state.pic_i += 1;
                state.key_pressed = "";
            }
            if (state.key_pressed == "PageUp") {
                state.pic_i -= 1;
                state.key_pressed = "";
            }
            state.pic_i = (state.pic_i + state.pic_num) % state.pic_num;
            size_t old_sz = state.points.size();

            ShowAppCustomRendering(state.pictures[state.pic_i], state);

            if (state.file_found && state.points.size() != old_sz) {
                WriteFile(state.file, state);
            }
            auto ctx = ImGui::GetCurrentContext();
            //ImGui::Text("%f", ctx->CurrentWindow->FontWindowScale);


            ImGui::End();

            //ImGuiWindow
            //ImGuiContext& g = *GImGui;
        }

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
