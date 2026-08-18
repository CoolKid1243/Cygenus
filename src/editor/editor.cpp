#include "editor.h"
#include "../scene/scene.h"
#include "../scripting/script_system.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include "../core/project.h"
#include "../renderer/rhi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include "../platform/win_dirent.h"
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif
#else
#include <dirent.h>
#endif

static EcsWorld* world = nullptr;
static Entity selected_entity = ECS_INVALID_ENTITY;
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

static void add_primitive(const char* primitive) {
    if (!world) return;
    selected_entity = scene_spawn_primitive(world, primitive);
}

// Small right-aligned remove button used by inspector component sections
static bool component_remove_button(const char* id) {
    char label[32];
    snprintf(label, sizeof(label), "Remove##%s", id);
    float w = ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - w);
    return ImGui::SmallButton(label);
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

extern "C" void editor_set_world(EcsWorld* w) {
    world = w;
    selected_entity = ECS_INVALID_ENTITY;
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

// Toggles play mode: scripts in the project folder run while playing
static void toggle_play() {
    script_system_set_playing(!script_system_is_playing());
}

extern "C" RHIFramebuffer* editor_get_framebuffer() {
    return viewport_fb;
}

extern "C" void editor_render() {
    if (!world) return;

    ImGuiStyle& style = ImGui::GetStyle();

    // ---------------- Menu / toolbar ----------------
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", NULL, false, true)) {
                scene_new(world);
                selected_entity = ECS_INVALID_ENTITY;
            }
            if (ImGui::MenuItem("Save Scene")) {
                char path[256];
                project_get_path("scenes/sample.scene", path, sizeof(path));
                scene_save(world, path);
            }
            if (ImGui::MenuItem("Load Scene")) {
                char path[256];
                project_get_path("scenes/sample.scene", path, sizeof(path));
                scene_load(world, path);
                selected_entity = ECS_INVALID_ENTITY;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Add")) {
            if (ImGui::MenuItem("Cube")) add_primitive("cube");
            if (ImGui::MenuItem("Plane")) add_primitive("plane");
            if (ImGui::MenuItem("Sphere")) add_primitive("sphere");
            if (ImGui::MenuItem("Empty Entity")) {
                selected_entity = ecs_create_entity(world, "Entity");
                ecs_add_component(world, selected_entity, COMPONENT_TRANSFORM);
            }
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
            bool playing = script_system_is_playing();
            const char* run_label = playing ? "\xe2\x96\xa0  Stop" : "\xe2\x96\xb6  Run"; // ■  Stop / ▶  Run
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
            if (ImGui::Button(run_label, run_size)) toggle_play();
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
    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs_is_alive(world, i)) continue;
        char label[96];
        snprintf(label, sizeof(label), "%s##%d", world->names[i], i);
        if (ImGui::Selectable(label, (selected_entity == i), 0, ImVec2(0,0))) {
            selected_entity = i;
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
            ImVec2(0,1), ImVec2(1,0), IM_COL32_WHITE, 8.0f); // flip V: GL framebuffers are bottom-up
        ImGui::Dummy(vp_size); // reserve the space we just painted into
    }
    ImGui::End();
    ImGui::PopStyleVar();

    // Inspector
    ImGui::Begin("Inspector");
    draw_panel_grid();
    if (ecs_is_alive(world, selected_entity)) {
        Entity e = selected_entity;

        // Rename field - duplicate names get a (1) suffix on enter
        char name_buf[64];
        strncpy(name_buf, world->names[e], sizeof(name_buf)-1);
        name_buf[sizeof(name_buf)-1] = '\0';
        if (ImGui::InputText("Name", name_buf, sizeof(name_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            ecs_rename_entity(world, e, name_buf);
        }

        // Transform component
        if (ecs_has_component(world, e, COMPONENT_TRANSFORM)) {
            ImGui::SeparatorText("Transform");
            if (component_remove_button("transform")) {
                ecs_remove_component(world, e, COMPONENT_TRANSFORM);
            } else {
                TransformComponent* t = &world->transforms[e];
                float pos[3] = {t->position.x, t->position.y, t->position.z};
                if (ImGui::DragFloat3("Position", pos, 0.05f, -100.0f, 100.0f, "%.2f")) {
                    t->position.x = pos[0]; t->position.y = pos[1]; t->position.z = pos[2];
                    t->dirty = 1;
                }
                float rot[3] = {t->rotation.x, t->rotation.y, t->rotation.z};
                if (ImGui::DragFloat3("Rotation", rot, 0.5f, -360.0f, 360.0f, "%.1f")) {
                    t->rotation.x = rot[0]; t->rotation.y = rot[1]; t->rotation.z = rot[2];
                    t->dirty = 1;
                }
                float sca[3] = {t->scale.x, t->scale.y, t->scale.z};
                if (ImGui::DragFloat3("Scale", sca, 0.05f, 0.01f, 10.0f, "%.2f")) {
                    t->scale.x = sca[0]; t->scale.y = sca[1]; t->scale.z = sca[2];
                    t->dirty = 1;
                }
            }
        }

        // Mesh component
        if (ecs_has_component(world, e, COMPONENT_MESH)) {
            ImGui::SeparatorText("Mesh");
            if (component_remove_button("mesh")) {
                ecs_remove_component(world, e, COMPONENT_MESH);
            } else {
                MeshComponent* m = &world->meshes[e];
                char mesh_path[128];
                strncpy(mesh_path, m->mesh_path, sizeof(mesh_path)-1);
                mesh_path[sizeof(mesh_path)-1] = '\0';
                if (ImGui::InputText("Mesh", mesh_path, sizeof(mesh_path))) {
                    strncpy(m->mesh_path, mesh_path, sizeof(m->mesh_path)-1);
                }
            }
        }

        // Material component
        if (ecs_has_component(world, e, COMPONENT_MATERIAL)) {
            ImGui::SeparatorText("Material");
            if (component_remove_button("material")) {
                ecs_remove_component(world, e, COMPONENT_MATERIAL);
            } else {
                MaterialComponent* mat = &world->materials[e];
                float color[3] = {mat->color[0], mat->color[1], mat->color[2]};
                if (ImGui::ColorEdit3("Colour", color, 0)) {
                    mat->color[0] = color[0]; mat->color[1] = color[1]; mat->color[2] = color[2];
                }
                char tex_path[256];
                strncpy(tex_path, mat->texture_path, sizeof(tex_path)-1);
                tex_path[sizeof(tex_path)-1] = '\0';
                if (ImGui::InputText("Texture", tex_path, sizeof(tex_path))) {
                    strncpy(mat->texture_path, tex_path, sizeof(mat->texture_path)-1);
                }
            }
        }

        // Add component button - lists the components the entity doesn't have yet
        ImGui::Spacing();
        if (ImGui::Button("Add Component", ImVec2(ImGui::GetContentRegionAvail().x, 30))) {
            ImGui::OpenPopup("add_component_popup");
        }
        if (ImGui::BeginPopup("add_component_popup")) {
            if (!ecs_has_component(world, e, COMPONENT_TRANSFORM) && ImGui::MenuItem("Transform")) {
                ecs_add_component(world, e, COMPONENT_TRANSFORM);
            }
            if (!ecs_has_component(world, e, COMPONENT_MESH) && ImGui::MenuItem("Mesh")) {
                ecs_add_component(world, e, COMPONENT_MESH);
                if (!world->meshes[e].mesh_path[0]) {
                    snprintf(world->meshes[e].mesh_path, sizeof(world->meshes[e].mesh_path), "primitive:cube");
                }
            }
            if (!ecs_has_component(world, e, COMPONENT_MATERIAL) && ImGui::MenuItem("Material")) {
                ecs_add_component(world, e, COMPONENT_MATERIAL);
            }
            ImGui::EndPopup();
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.18f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.24f, 0.26f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.42f, 0.12f, 0.14f, 1.0f));
        if (ImGui::Button("Delete Entity", ImVec2(ImGui::GetContentRegionAvail().x, 30))) {
            ecs_destroy_entity(world, e);
            selected_entity = ECS_INVALID_ENTITY;
        }
        ImGui::PopStyleColor(3);
    } else {
        ImGui::TextDisabled("No entity selected");
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