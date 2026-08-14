// src/editor/editor.cpp
#include "editor.h"
#include <imgui.h>
#include <imgui_internal.h>   // needed for DockBuilder* layout API
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include "../core/project.h"
#include "../renderer/rhi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

static Scene* current_scene = nullptr;
static int selected_index = -1;
static GLFWwindow* glfw_window = nullptr;
static PlatformWindow* platform_window = nullptr;

static float viewport_x = 0, viewport_y = 0;
static float viewport_w = 0, viewport_h = 0;
static int mouse_over_viewport = 0;

static char project_root_path[256] = {0};
static bool show_console = true;
static bool show_demo_window = false;

static RHIFramebuffer* viewport_fb = NULL;
static int fb_width = 0, fb_height = 0;

static int object_counter = 0;
static bool first_frame = true;

static char console_text[1024] = "Console output (placeholder)";

// ---------------------------------------------------------------------
// Theming
// ---------------------------------------------------------------------
enum EditorTheme {
    THEME_MIDNIGHT = 0,   // dark blue-black, requested default
    THEME_GRAPHITE,       // neutral charcoal, cyan accent
    THEME_ABYSS,          // near-black, violet accent
    THEME_COUNT
};

static int current_theme = THEME_MIDNIGHT;
static const char* theme_names[THEME_COUNT] = { "Midnight Blue", "Graphite", "Abyss" };

static void apply_theme(int theme) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Shared macOS-flavored shape language: soft rounding, generous spacing, no hard borders.
    style.WindowRounding    = 14.0f;
    style.ChildRounding     = 12.0f;
    style.FrameRounding     = 10.0f;
    style.PopupRounding     = 12.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding      = 10.0f;
    style.TabRounding       = 10.0f;
    style.WindowBorderSize  = 0.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 0.0f;
    style.WindowPadding     = ImVec2(12, 12);
    style.FramePadding      = ImVec2(8, 5);
    style.ItemSpacing       = ImVec2(10, 8);
    style.ItemInnerSpacing  = ImVec2(6, 6);
    style.IndentSpacing     = 18.0f;
    style.ScrollbarSize     = 13.0f;
    style.GrabMinSize       = 10.0f;
    style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
    style.SeparatorTextBorderSize = 1.0f;
#ifdef IMGUI_HAS_DOCK
    style.DockingSeparatorSize = 2.0f;
    style.TabBarBorderSize = 1.0f;
#endif

    struct Palette {
        ImVec4 bg, panel, raised, accent, accent_hi, accent_active, text, text_dim, border;
    } p;

    switch (theme) {
        default:
        case THEME_MIDNIGHT:
            p = { ImVec4(0.047f,0.055f,0.086f,1.0f), ImVec4(0.075f,0.086f,0.129f,1.0f),
                  ImVec4(0.106f,0.125f,0.176f,1.0f), ImVec4(0.25f,0.45f,0.95f,1.0f),
                  ImVec4(0.38f,0.58f,1.00f,1.0f), ImVec4(0.20f,0.38f,0.85f,1.0f),
                  ImVec4(0.90f,0.92f,0.96f,1.0f), ImVec4(0.52f,0.56f,0.64f,1.0f),
                  ImVec4(0.15f,0.17f,0.23f,1.0f) };
            break;
        case THEME_GRAPHITE:
            p = { ImVec4(0.078f,0.078f,0.086f,1.0f), ImVec4(0.110f,0.110f,0.122f,1.0f),
                  ImVec4(0.150f,0.150f,0.165f,1.0f), ImVec4(0.20f,0.70f,0.75f,1.0f),
                  ImVec4(0.32f,0.85f,0.88f,1.0f), ImVec4(0.16f,0.58f,0.62f,1.0f),
                  ImVec4(0.92f,0.92f,0.93f,1.0f), ImVec4(0.55f,0.55f,0.58f,1.0f),
                  ImVec4(0.19f,0.19f,0.21f,1.0f) };
            break;
        case THEME_ABYSS:
            p = { ImVec4(0.035f,0.03f,0.05f,1.0f), ImVec4(0.065f,0.058f,0.09f,1.0f),
                  ImVec4(0.10f,0.09f,0.135f,1.0f), ImVec4(0.55f,0.35f,0.95f,1.0f),
                  ImVec4(0.66f,0.48f,1.00f,1.0f), ImVec4(0.45f,0.28f,0.80f,1.0f),
                  ImVec4(0.91f,0.90f,0.96f,1.0f), ImVec4(0.55f,0.52f,0.62f,1.0f),
                  ImVec4(0.14f,0.12f,0.19f,1.0f) };
            break;
    }

    colors[ImGuiCol_Text]                  = p.text;
    colors[ImGuiCol_TextDisabled]          = p.text_dim;
    colors[ImGuiCol_WindowBg]              = p.panel;
    colors[ImGuiCol_ChildBg]               = ImVec4(0,0,0,0);
    colors[ImGuiCol_PopupBg]               = p.panel;
    colors[ImGuiCol_Border]                = p.border;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0,0,0,0);
    colors[ImGuiCol_FrameBg]               = p.raised;
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(p.raised.x+0.03f,p.raised.y+0.03f,p.raised.z+0.04f,1.0f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(p.raised.x+0.05f,p.raised.y+0.05f,p.raised.z+0.06f,1.0f);
    colors[ImGuiCol_TitleBg]               = p.bg;
    colors[ImGuiCol_TitleBgActive]         = p.bg;
    colors[ImGuiCol_TitleBgCollapsed]      = p.bg;
    colors[ImGuiCol_MenuBarBg]             = p.bg;
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0,0,0,0);
    colors[ImGuiCol_ScrollbarGrab]         = p.raised;
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(p.raised.x+0.05f,p.raised.y+0.05f,p.raised.z+0.06f,1.0f);
    colors[ImGuiCol_ScrollbarGrabActive]   = p.accent_active;
    colors[ImGuiCol_CheckMark]             = p.accent_hi;
    colors[ImGuiCol_SliderGrab]            = p.accent;
    colors[ImGuiCol_SliderGrabActive]      = p.accent_hi;
    colors[ImGuiCol_Button]                = p.raised;
    colors[ImGuiCol_ButtonHovered]         = ImVec4(p.raised.x+0.05f,p.raised.y+0.05f,p.raised.z+0.06f,1.0f);
    colors[ImGuiCol_ButtonActive]          = p.accent_active;
    colors[ImGuiCol_Header]                = ImVec4(p.raised.x+0.03f,p.raised.y+0.03f,p.raised.z+0.04f,1.0f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(p.raised.x+0.07f,p.raised.y+0.07f,p.raised.z+0.08f,1.0f);
    colors[ImGuiCol_HeaderActive]          = p.accent_active;
    colors[ImGuiCol_Separator]             = p.border;
    colors[ImGuiCol_SeparatorHovered]      = p.accent;
    colors[ImGuiCol_SeparatorActive]       = p.accent_hi;
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0,0,0,0);
    colors[ImGuiCol_ResizeGripHovered]     = p.accent_active;
    colors[ImGuiCol_ResizeGripActive]      = p.accent_hi;
    colors[ImGuiCol_Tab]                   = p.bg;
    colors[ImGuiCol_TabHovered]            = ImVec4(p.raised.x+0.05f,p.raised.y+0.05f,p.raised.z+0.06f,1.0f);
    colors[ImGuiCol_TabActive]             = p.raised;
    colors[ImGuiCol_TabUnfocused]          = p.bg;
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(p.bg.x+0.02f,p.bg.y+0.02f,p.bg.z+0.03f,1.0f);
#ifdef IMGUI_HAS_DOCK
    colors[ImGuiCol_DockingPreview]        = ImVec4(p.accent.x,p.accent.y,p.accent.z,0.35f);
    colors[ImGuiCol_DockingEmptyBg]        = p.bg;
#endif
    colors[ImGuiCol_PlotLines]             = p.accent;
    colors[ImGuiCol_PlotLinesHovered]      = p.accent_hi;
    colors[ImGuiCol_PlotHistogram]         = p.accent;
    colors[ImGuiCol_PlotHistogramHovered]  = p.accent_hi;
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(p.accent.x,p.accent.y,p.accent.z,0.35f);
    colors[ImGuiCol_DragDropTarget]        = p.accent_hi;
    colors[ImGuiCol_NavHighlight]          = p.accent;
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1,1,1,0.7f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.2f,0.2f,0.2f,0.4f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(p.bg.x,p.bg.y,p.bg.z,0.55f);

    current_theme = theme;
}

// ---------------------------------------------------------------------
// Subtle background grid drawn behind panel contents
// ---------------------------------------------------------------------
static bool show_panel_grid = true;

static void draw_panel_grid(float spacing = 22.0f) {
    if (!show_panel_grid) return;
    ImVec2 win_pos  = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 line_col = IM_COL32(255, 255, 255, 9); // very light, just a texture hint
    float start_x = fmodf(win_pos.x, spacing);
    float start_y = fmodf(win_pos.y, spacing);
    for (float x = start_x; x < win_size.x; x += spacing)
        dl->AddLine(ImVec2(win_pos.x + x, win_pos.y), ImVec2(win_pos.x + x, win_pos.y + win_size.y), line_col);
    for (float y = start_y; y < win_size.y; y += spacing)
        dl->AddLine(ImVec2(win_pos.x, win_pos.y + y), ImVec2(win_pos.x + win_size.x, win_pos.y + y), line_col);
}

// ---------------------------------------------------------------------
// Layout presets (Unity/Unreal-style, built with the DockBuilder API)
// ---------------------------------------------------------------------
enum LayoutPreset {
    LAYOUT_DEFAULT = 0,   // classic: hierarchy left, inspector right, project+console bottom
    LAYOUT_WIDE_VIEWPORT, // narrow side columns, tall viewport, best for level dressing
    LAYOUT_TWO_COLUMN,    // everything stacked left, huge viewport right (streaming/capture friendly)
    LAYOUT_COUNT
};
static const char* layout_names[LAYOUT_COUNT] = { "Default", "Wide Viewport", "Two Column" };
static int pending_layout = LAYOUT_DEFAULT; // applied on first frame, then only on explicit request

static void apply_layout_preset(ImGuiID dockspace_id, int preset) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID main_id = dockspace_id;

    switch (preset) {
        default:
        case LAYOUT_DEFAULT: {
            ImGuiID left   = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Left,  0.20f, nullptr, &main_id);
            ImGuiID right  = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Right, 0.26f, nullptr, &main_id);
            ImGuiID bottom = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Down,  0.26f, nullptr, &main_id);

            ImGui::DockBuilderDockWindow("Hierarchy", left);
            ImGui::DockBuilderDockWindow("Inspector", right);
            ImGui::DockBuilderDockWindow("Project", bottom);
            ImGui::DockBuilderDockWindow("Console", bottom);
            ImGui::DockBuilderDockWindow("Viewport", main_id);
            break;
        }
        case LAYOUT_WIDE_VIEWPORT: {
            ImGuiID left   = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Left,  0.14f, nullptr, &main_id);
            ImGuiID right  = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Right, 0.16f, nullptr, &main_id);
            ImGuiID bottom = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Down,  0.18f, nullptr, &main_id);

            ImGui::DockBuilderDockWindow("Hierarchy", left);
            ImGui::DockBuilderDockWindow("Inspector", right);
            ImGui::DockBuilderDockWindow("Project", bottom);
            ImGui::DockBuilderDockWindow("Console", bottom);
            ImGui::DockBuilderDockWindow("Viewport", main_id);
            break;
        }
        case LAYOUT_TWO_COLUMN: {
            ImGuiID left = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Left, 0.24f, nullptr, &main_id);
            ImGuiID left_top, left_bottom;
            left_top    = ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.5f, nullptr, &left_bottom);

            ImGui::DockBuilderDockWindow("Hierarchy", left_top);
            ImGui::DockBuilderDockWindow("Project", left_bottom);
            ImGui::DockBuilderDockWindow("Inspector", left_bottom);
            ImGui::DockBuilderDockWindow("Console", left_bottom);
            ImGui::DockBuilderDockWindow("Viewport", main_id);
            break;
        }
    }

    ImGui::DockBuilderFinish(dockspace_id);
}

// ---------------------------------------------------------------------

static void add_primitive(const char* mesh_path) {
    if (!current_scene) return;
    SceneObject obj;
    memset(&obj, 0, sizeof(obj));
    obj.position = (Vec3){0,0,0};
    obj.rotation = (Vec3){0,0,0};
    obj.scale = (Vec3){1,1,1};
    obj.name[0] = '\0';
    strncpy(obj.mesh_path, mesh_path, sizeof(obj.mesh_path)-1);
    strncpy(obj.texture_path, "none", sizeof(obj.texture_path)-1);
    obj.tint[0] = 1.0f; obj.tint[1] = 1.0f; obj.tint[2] = 1.0f;
    scene_add_object(current_scene, obj);
    selected_index = current_scene->object_count - 1;
}

static void list_directory(const char* path, const char* prefix) {
    DIR* dir = opendir(path);
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        struct stat st;
        stat(full_path, &st);
        if (S_ISDIR(st.st_mode)) {
            char label[300];
            snprintf(label, sizeof(label), "\xf0\x9f\x93\x81 %s", entry->d_name); // 📁
            if (ImGui::TreeNode(label)) {
                list_directory(full_path, "");
                ImGui::TreePop();
            }
        } else {
            ImGui::Text("\xf0\x9f\x93\x84 %s", entry->d_name); // 📄
        }
    }
    closedir(dir);
}

extern "C" void editor_init(PlatformWindow* window) {
    platform_window = window;
    glfw_window = (GLFWwindow*)platform_get_gl_context(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true; // avoids accidental drags, feels more "app-like"

    ImGui::StyleColorsDark();
    apply_theme(THEME_MIDNIGHT);

    // Round the outer corners of secondary OS-level viewport windows to match.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 14.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(glfw_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    project_get_path("", project_root_path, sizeof(project_root_path));
    size_t len = strlen(project_root_path);
    if (len > 0 && project_root_path[len-1] == '/')
        project_root_path[len-1] = '\0';

    first_frame = true;
    printf("Editor initialized with Docking\n");
}

extern "C" void editor_shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

extern "C" void editor_set_scene(Scene* scene) {
    current_scene = scene;
    selected_index = -1;
    object_counter = 0;
}

extern "C" void editor_new_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

extern "C" void editor_get_viewport_rect(float* x, float* y, float* w, float* h) {
    *x = viewport_x; *y = viewport_y; *w = viewport_w; *h = viewport_h;
}

extern "C" int editor_is_mouse_over_viewport() {
    return mouse_over_viewport;
}

extern "C" void editor_run_game() {
    char scene_path[256];
    project_get_path("scenes/sample.scene", scene_path, sizeof(scene_path));
    if (!scene_save(current_scene, scene_path)) {
        printf("Failed to save scene before running!\n");
        return;
    }
    char game_exe_path[256];
    project_get_path("game_host", game_exe_path, sizeof(game_exe_path));
    char src_path[512];
    snprintf(src_path, sizeof(src_path), "./build/Release/game_host");
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\" && chmod +x \"%s\"", src_path, game_exe_path, game_exe_path);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" &", game_exe_path, scene_path);
    system(cmd);
}

extern "C" RHIFramebuffer* editor_get_framebuffer() {
    return viewport_fb;
}

extern "C" void editor_render() {
    if (!current_scene) return;

    ImGuiStyle& style = ImGui::GetStyle();

    // ---------------- Menu / toolbar ----------------
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", NULL, false, true)) {
                scene_create(current_scene);
                selected_index = -1;
                object_counter = 0;
            }
            if (ImGui::MenuItem("Save Scene")) {
                char path[256];
                project_get_path("scenes/sample.scene", path, sizeof(path));
                scene_save(current_scene, path);
            }
            if (ImGui::MenuItem("Load Scene")) {
                char path[256];
                project_get_path("scenes/sample.scene", path, sizeof(path));
                scene_load(current_scene, path);
                selected_index = -1;
                object_counter = 0;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Add")) {
            if (ImGui::MenuItem("Cube")) add_primitive("primitive:cube");
            if (ImGui::MenuItem("Plane")) add_primitive("primitive:plane");
            if (ImGui::MenuItem("Sphere")) add_primitive("primitive:sphere");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Console", NULL, &show_console);
            ImGui::MenuItem("Panel Grid", NULL, &show_panel_grid);
            ImGui::Separator();
            if (ImGui::BeginMenu("Layout")) {
                for (int i = 0; i < LAYOUT_COUNT; i++) {
                    if (ImGui::MenuItem(layout_names[i])) pending_layout = i;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Theme")) {
                for (int i = 0; i < THEME_COUNT; i++) {
                    bool selected = (current_theme == i);
                    if (ImGui::MenuItem(theme_names[i], NULL, selected)) apply_theme(i);
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo", NULL, &show_demo_window);
            ImGui::EndMenu();
        }

        // Single, centered Run button — neutral pill with an accent-colored icon,
        // closer to BeamNG/Unreal's understated toolbar buttons than a loud game-UI green.
        {
            const char* run_label = "\xe2\x96\xb6  Run"; // ▶  Run
            float row_height = ImGui::GetFrameHeight(); // matches the menu bar row exactly, avoids clipping
            ImVec2 run_size = ImVec2(84, row_height);
            float bar_width = ImGui::GetWindowWidth();
            ImGui::SetCursorPosX((bar_width - run_size.x) * 0.5f);

            ImGuiStyle& s = ImGui::GetStyle();
            ImVec4 accent = s.Colors[ImGuiCol_SliderGrabActive];

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, row_height * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Button, s.Colors[ImGuiCol_FrameBg]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, s.Colors[ImGuiCol_FrameBgHovered]);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, s.Colors[ImGuiCol_FrameBgActive]);
            ImGui::PushStyleColor(ImGuiCol_Text, accent);
            if (ImGui::Button(run_label, run_size)) editor_run_game();
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
        }

        ImGui::EndMainMenuBar();
    }

    // ---------------- Dockspace ----------------
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    ImGuiWindowFlags dockspace_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                    ImGuiWindowFlags_NoBackground;
    ImGui::Begin("Dockspace", nullptr, dockspace_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0,0), ImGuiDockNodeFlags_PassthruCentralNode);

    if (first_frame) {
        apply_layout_preset(dockspace_id, LAYOUT_DEFAULT);
        first_frame = false;
    }
    if (pending_layout >= 0 && !first_frame) {
        apply_layout_preset(dockspace_id, pending_layout);
        pending_layout = -1;
    }

    // ---- Windows ----
    ImGui::Begin("Hierarchy");
    draw_panel_grid();
    for (int i = 0; i < current_scene->object_count; i++) {
        char label[64];
        if (strncmp(current_scene->objects[i].mesh_path, "primitive:", 10) == 0) {
            snprintf(label, sizeof(label), "%s##%d", current_scene->objects[i].mesh_path + 10, i);
        } else {
            snprintf(label, sizeof(label), "Object %d##%d", i, i);
        }
        if (ImGui::Selectable(label, (selected_index == i), 0, ImVec2(0,0))) {
            selected_index = i;
        }
    }
    ImGui::End();

    // Viewport
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport");
    ImVec2 vp_size = ImGui::GetContentRegionAvail();
    viewport_x = ImGui::GetWindowPos().x;
    viewport_y = ImGui::GetWindowPos().y;
    viewport_w = vp_size.x;
    viewport_h = vp_size.y;
    mouse_over_viewport = ImGui::IsWindowHovered(ImGuiHoveredFlags_None);

    int new_fb_w = (int)vp_size.x;
    int new_fb_h = (int)vp_size.y;
    if (new_fb_w < 1) new_fb_w = 1;
    if (new_fb_h < 1) new_fb_h = 1;
    if (!viewport_fb || new_fb_w != fb_width || new_fb_h != fb_height) {
        if (viewport_fb) {
            rhi_framebuffer_destroy(viewport_fb);
            viewport_fb = NULL;
        }
        viewport_fb = rhi_framebuffer_create(new_fb_w, new_fb_h);
        fb_width = new_fb_w;
        fb_height = new_fb_h;
    }

    if (viewport_fb) {
        unsigned int tex_id = rhi_framebuffer_get_texture_id(viewport_fb);
        ImVec2 img_pos = ImGui::GetCursorScreenPos();
        // Slightly rounded corners on the render image to match the panel style.
        ImGui::GetWindowDrawList()->AddImageRounded(
            (ImTextureID)(uintptr_t)tex_id,
            img_pos, ImVec2(img_pos.x + vp_size.x, img_pos.y + vp_size.y),
            ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE, 8.0f);
        ImGui::Dummy(vp_size); // reserve the space we just painted into
    }
    ImGui::End();
    ImGui::PopStyleVar();

    // Inspector
    ImGui::Begin("Inspector");
    draw_panel_grid();
    if (selected_index >= 0 && selected_index < current_scene->object_count) {
        SceneObject* obj = &current_scene->objects[selected_index];

        ImGui::SeparatorText("Transform");
        float pos[3] = {obj->position.x, obj->position.y, obj->position.z};
        if (ImGui::DragFloat3("Position", pos, 0.05f, -100.0f, 100.0f, "%.2f")) {
            obj->position.x = pos[0]; obj->position.y = pos[1]; obj->position.z = pos[2];
        }
        float rot[3] = {obj->rotation.x, obj->rotation.y, obj->rotation.z};
        if (ImGui::DragFloat3("Rotation", rot, 0.5f, -360.0f, 360.0f, "%.1f")) {
            obj->rotation.x = rot[0]; obj->rotation.y = rot[1]; obj->rotation.z = rot[2];
        }
        float sca[3] = {obj->scale.x, obj->scale.y, obj->scale.z};
        if (ImGui::DragFloat3("Scale", sca, 0.05f, 0.01f, 10.0f, "%.2f")) {
            obj->scale.x = sca[0]; obj->scale.y = sca[1]; obj->scale.z = sca[2];
        }

        ImGui::SeparatorText("Rendering");
        char mesh_path[128];
        strncpy(mesh_path, obj->mesh_path, sizeof(mesh_path)-1);
        if (ImGui::InputText("Mesh", mesh_path, sizeof(mesh_path))) {
            strncpy(obj->mesh_path, mesh_path, sizeof(obj->mesh_path)-1);
        }
        char tex_path[256];
        strncpy(tex_path, obj->texture_path, sizeof(tex_path)-1);
        if (ImGui::InputText("Texture", tex_path, sizeof(tex_path))) {
            strncpy(obj->texture_path, tex_path, sizeof(obj->texture_path)-1);
        }
        float tint[3] = {obj->tint[0], obj->tint[1], obj->tint[2]};
        if (ImGui::ColorEdit3("Tint", tint, 0)) {
            obj->tint[0] = tint[0]; obj->tint[1] = tint[1]; obj->tint[2] = tint[2];
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.18f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.24f, 0.26f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.42f, 0.12f, 0.14f, 1.0f));
        if (ImGui::Button("Delete Object", ImVec2(ImGui::GetContentRegionAvail().x, 30))) {
            for (int j = selected_index; j < current_scene->object_count - 1; j++) {
                current_scene->objects[j] = current_scene->objects[j+1];
            }
            current_scene->object_count--;
            selected_index = -1;
        }
        ImGui::PopStyleColor(3);
    } else {
        ImGui::TextDisabled("No object selected");
    }
    ImGui::End();

    // Project
    ImGui::Begin("Project");
    draw_panel_grid();
    ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1.0f), "%s", project_root_path);
    ImGui::Separator();
    list_directory(project_root_path, "");
    ImGui::End();

    // Console
    if (show_console) {
        ImGui::Begin("Console", &show_console);
        draw_panel_grid();
        ImGui::TextWrapped("%s", console_text);
        ImGui::End();
    }

    // Demo
    if (show_demo_window) {
        ImGui::ShowDemoWindow(&show_demo_window);
    }

    ImGui::End(); // Dockspace

    // Render ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and render additional viewports
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
}

extern "C" void editor_set_console_text(const char* text) {
    snprintf(console_text, sizeof(console_text), "%s", text);
}