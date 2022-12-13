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

// Images

bool LoadTextureFromFile(const char* filename, Picture& pic)
{
    // Load from file
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
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
static void ShowAppCustomRendering(Picture& pic,
                                   std::vector<Point> &points,
                                   bool drawing,
                                   bool selecting,
                                   float scale,
                                   std::string& key_pressed)
{
    static bool no_titlebar = false;
    static bool no_scrollbar = false;
    static bool no_menu = false;
    static bool no_move = false;
    static bool no_resize = false;
    static bool no_collapse = true;
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
    ImGui::Begin("Draw", &open, window_flags);

    auto ctx = ImGui::GetCurrentContext();

    //static ImVector<ImVec2> points;
    static ImVec2 scrolling(0.0f, 0.0f);
    static bool opt_enable_grid = true;
    static bool opt_enable_context_menu = true;
    static bool adding_line = false;
    // Typically you would use a BeginChild()/EndChild() pair to benefit from a clipping region + own scrolling.
    // Here we demonstrate that this can be replaced by simple offsetting + custom drawing + PushClipRect/PopClipRect() calls.
    // To use a child window instead we could use, e.g:
    //      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));      // Disable padding
    //      ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(50, 50, 50, 255));  // Set a background color
    //      ImGui::BeginChild("canvas", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_NoMove);
    //      ImGui::PopStyleColor();
    //      ImGui::PopStyleVar();
    //      [...]
    //      ImGui::EndChild();

    // Using InvisibleButton() as a convenience 1) it will advance the layout cursor and 2) allows us to use IsItemHovered()/IsItemActive()
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
    ImVec2 canvas_sz(pic.image_width * scale, pic.image_height * scale);
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
    if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && drawing) {
        points.emplace_back(mouse_pos_in_canvas.x, mouse_pos_in_canvas.y, scale);
    }

    if (selecting && (selected_point == -1) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        for (int i=0; i < points.size(); ++i) {
            if (std::abs(mouse_pos_in_canvas.x - points[i].x) <= R && std::abs(mouse_pos_in_canvas.y - points[i].y) < R) {
                selected_point = i;
                break;
            }
        }
    }

    if (selecting && (selected_point != -1) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        selected_point = -1;
    }

    if (selecting && (selected_point != -1) && key_pressed == "Backspace") {
        points.erase(points.begin() + selected_point);
        selected_point = -1;
    }

    if (selecting && (selected_point != -1)) {
        points[selected_point].x = mouse_pos_in_canvas.x;
        points[selected_point].y = mouse_pos_in_canvas.y;
    }

    // Context menu (under default mouse threshold)
    ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
    /*if (opt_enable_context_menu && drag_delta.x == 0.0f && drag_delta.y == 0.0f)
        ImGui::OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);
    if (ImGui::BeginPopup("context"))
    {
        if (ImGui::MenuItem("Remove one", NULL, false, points.size() > 0)) { points.pop_back(); }
        if (ImGui::MenuItem("Remove all", NULL, false, points.size() > 0)) { points.clear(); }
        ImGui::EndPopup();
    }*/

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

    for (int n = 0; n < points.size(); n += 1) {
        draw_list->AddCircleFilled(ImVec2(origin.x + points[n].x * scale / points[n].scale,
                                          origin.y + points[n].y * scale / points[n].scale), R,
                                   IM_COL32(255, 255, 0, 255));
        if (n < points.size() - 1) {
            draw_list->AddLine(ImVec2(origin.x + points[n].x  * scale / points[n].scale, origin.y + points[n].y * scale / points[n].scale),
                           ImVec2(origin.x + points[n + 1].x * scale / points[n + 1].scale, origin.y + points[n + 1].y * scale / points[n + 1].scale), IM_COL32(255, 255, 0, 255),
                           1.0f);
            //draw_list->PopClipRect();
        }
    }
    //std::cout << origin.x << " " << origin.y << std::endl;
    //ImGui::Text("%f %f", origin.x, origin.y);

    ImGui::End();

}

// Files

void OpenPictures(char* folder, std::vector<Picture>& pictures) {
    int pic_num = 0;
    //readdir
    //std::string folder = "/home/dmitrij/CLionProjects/yourbunnywrote/images";
    DIR *dir = opendir(folder);
    std::vector<std::string> pic_names;
    if (dir) {
        struct dirent *ent;
        while((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name.ends_with(".png")) {
                pic_names.emplace_back(ent->d_name);
                ++pic_num;
            }
        }
    }
    else {
        fprintf(stderr, "Error opening directory\n");
    }
    pictures.resize(pic_num);
    for (int i = 0; i < pic_num; ++i) {
        bool ret = LoadTextureFromFile((std::string(folder) + "/" + pic_names[i]).c_str(), pictures[i]);
        IM_ASSERT(ret);
    }
}

void OpenFile(std::string& file, std::vector<Point>& points) {
    std::ifstream f;
    f.open(file);
    std::string str;
    float x;
    while(f >> x){
        Point point(x, 0, 1);
        f >> point.y;
        f >> point.scale;
        points.push_back(point);
    }
    f.close();
}

void WriteFile(std::string& file, std::vector<Point>& points) {
    std::ofstream f;
    f.open(file);
    for (auto & point : points){
        f << point.x << " " << point.y << " " << point.scale << std::endl;
    }
    f.close();
}



// Menus

static void ShowMenuFile(std::string& key_pessed,
                         std::vector<Point>& points,
                         std::string &filename) {
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_DEMO_MARKER("Menu");
    ImGui::MenuItem("(demo menu)", NULL, false, false);
    if (ImGui::MenuItem("New") || (io.KeyCtrl && key_pessed == "N")) {}
    if (ImGui::MenuItem("Open", "Ctrl+O") || (io.KeyCtrl && key_pessed == "O")) {}
    if (ImGui::BeginMenu("Open Recent") || (io.KeyCtrl && key_pessed == "R"))
    {
        ImGui::MenuItem("fish_hat.c");
        ImGui::MenuItem("fish_hat.inl");
        ImGui::MenuItem("fish_hat.h");
        if (ImGui::BeginMenu("More.."))
        {
            ImGui::MenuItem("Hello");
            ImGui::MenuItem("Sailor");
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Save", "Ctrl+S") || (io.KeyCtrl && key_pessed == "S")) {
        WriteFile(filename, points);
    }
    //if (ImGui::MenuItem("Save As..")) {}

    ImGui::Separator();
    IMGUI_DEMO_MARKER("Examples/Menu/Options");
    if (ImGui::BeginMenu("Options"))
    {
        static bool enabled = true;
        ImGui::MenuItem("Enabled", "", &enabled);
        ImGui::BeginChild("child", ImVec2(0, 60), true);
        for (int i = 0; i < 10; i++)
            ImGui::Text("Scrolling Text %d", i);
        ImGui::EndChild();
        static float f = 0.5f;
        static int n = 0;
        ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
        ImGui::InputFloat("Input", &f, 0.1f);
        ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
        ImGui::EndMenu();
    }

    IMGUI_DEMO_MARKER("Examples/Menu/Colors");
    if (ImGui::BeginMenu("Colors"))
    {
        float sz = ImGui::GetTextLineHeight();
        for (int i = 0; i < ImGuiCol_COUNT; i++)
        {
            const char* name = ImGui::GetStyleColorName((ImGuiCol)i);
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), ImGui::GetColorU32((ImGuiCol)i));
            ImGui::Dummy(ImVec2(sz, sz));
            ImGui::SameLine();
            ImGui::MenuItem(name);
        }
        ImGui::EndMenu();
    }

    // Here we demonstrate appending again to the "Options" menu (which we already created above)
    // Of course in this demo it is a little bit silly that this function calls BeginMenu("Options") twice.
    // In a real code-base using it would make senses to use this feature from very different code locations.
    if (ImGui::BeginMenu("Options")) // <-- Append!
    {
        IMGUI_DEMO_MARKER("Examples/Menu/Append to an existing menu");
        static bool b = true;
        ImGui::Checkbox("SomeOption", &b);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Disabled", false)) // Disabled
    {
        IM_ASSERT(0);
    }
    if (ImGui::MenuItem("Checked", NULL, true)) {}
    if (ImGui::MenuItem("Quit", "Alt+F4")) {}
}

static void ShowAppMainMenuBar(std::string& key_pressed,
                               std::vector<Point>& points,
                               std::string &filename)
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            ShowMenuFile(key_pressed, points, filename);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
            if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "CTRL+X")) {}
            if (ImGui::MenuItem("Copy", "CTRL+C")) {}
            if (ImGui::MenuItem("Paste", "CTRL+V")) {}
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
int main(int, char**)
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

    // Images
    std::vector<Picture> pictures;
    pictures.emplace_back();

    // Points
    std::vector<Point> points;

    // Main loop
    bool done = false;
    bool drawing = false;
    bool selecting = false;
    bool directory_found = false;
    bool file_found = false;
    int pic_i = 0;
    float scale = 1.0;
    bool change_dir = true;
    bool change_file = true;
    bool show_hints = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
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
            std::string key_pressed = "";
            struct funcs { static bool IsLegacyNativeDupe(ImGuiKey key) { return key < 512 && ImGui::GetIO().KeyMap[key] != -1; } }; // Hide Native<>ImGuiKey duplicates when both exists in the array
            const ImGuiKey key_first = (ImGuiKey)0;
            //io.Key
            for (ImGuiKey key = key_first; key < ImGuiKey_COUNT; key = (ImGuiKey)(key + 1)) {
                if (funcs::IsLegacyNativeDupe(key))
                    continue;
                if (ImGui::IsKeyPressed(key)) {
                    key_pressed = std::string(ImGui::GetKeyName(key));
                }
            }

            // Buttons
            if (ImGui::Button("Select") || ((key_pressed == "A") &&io.KeyCtrl)) {
                selecting = !selecting;
                if (selecting)
                    drawing = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("+") || ((key_pressed == "Equal") &&io.KeyCtrl)) {
                scale += 0.1;
            }
            ImGui::SameLine();
            if (ImGui::Button("-") || ((key_pressed == "Minus") &&io.KeyCtrl)) {
                scale -= 0.1;
                scale = std::max(0.1f, scale);
            };
            ImGui::SameLine();
            if (ImGui::Button("Draw") || (io.KeyCtrl && key_pressed == "D")) {
                drawing = !drawing;
                if (drawing)
                    selecting = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Next Picture") || key_pressed == "PageDown") {
                pic_i += 1;
            }
            ImGui::SameLine();
            if (ImGui::Button("Previous picture") || key_pressed == "PageUp") {
                pic_i -= 1;
            }
            ImGui::SameLine();
            if (ImGui::Button("Undo") || (key_pressed == "z" && io.KeyCtrl)) {
                if (!points.empty()) {
                    points.pop_back();
                }
            }
            ImGui::SameLine();
            ImGui::Checkbox("Change input directory", &change_dir);
            ImGui::SameLine();
            ImGui::Checkbox("Change input file", &change_file);
            ImGui::SameLine();
            ImGui::Checkbox("Show hints window", &show_hints);

            // Images
            static char dir_name[256] = "/home/dmitrij/images";
            if (change_dir) {
                ImGui::Text("Directory with pictures: ");
                ImGui::SameLine();
                ImGui::InputText(":", dir_name, IM_ARRAYSIZE(dir_name));
                ImGui::SameLine();
                if (ImGui::Button("Search directory")) {
                    try {
                        OpenPictures(dir_name, pictures);
                        directory_found = true;
                    } catch (...) {
                        directory_found = false;
                    }
                }
            }
            if (pictures.empty()) {
                pictures.emplace_back();
            } else if (pictures.size() > 1 && pictures[0].image_texture == 0) {
                pictures.erase(pictures.begin());
            }
            int pic_num = pictures.size();

            // Points
            static char file_name[256] = "/home/dmitrij/dummy.txt";
            if (change_file) {
                ImGui::Text("File with points: ");
                ImGui::SameLine();

                ImGui::InputText(":", file_name, IM_ARRAYSIZE(file_name));
                ImGui::SameLine();
                if (ImGui::Button("Search file")) {
                    try {
                        if (!std::string(file_name).empty()) {
                            std::string file = file_name;
                            OpenFile(file, points);
                            file_found = true;
                        }
                    } catch (...) {
                        file_found = false;
                    }
                }
            }

            if (!directory_found) {
                ImGui::Text("Directory not found :(");
            }
            if (!file_found) {
                ImGui::Text("File not found :(");
                ImGui::Text("Input the correct file to draw & save points");
            }

            // Show points
            if (key_pressed == "Backspace" && io.KeyCtrl) {
                points.clear();
                selected_point = -1;
            }
            pic_i = (pic_i + pic_num) % pic_num;
            size_t old_sz = points.size();
            if (file_found)
                ShowAppCustomRendering(pictures[pic_i], points, drawing, selecting, scale, key_pressed);
            if (file_found && points.size() != old_sz) {
                std::string file = file_name;
                WriteFile(file, points);
            }
            /*ImGui::Text("Points: ");
            for (auto & point : points) {
                ImGui::Text("(%f, %f)", point.x, point.y);
            }*/
            auto ctx = ImGui::GetCurrentContext();
            //ImGui::Text("%f", ctx->CurrentWindow->FontWindowScale);

            std::string file = file_name;
            ShowAppMainMenuBar(key_pressed, points, file);

            if (show_hints) {
                ShowHintsWindow();
            }

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
