#include "editor.h"
#include "../scene/scene.h"
#include "../scripting/script_system.h"
#include "../ecs/ecs.h"
#include "../math/math3d.h"
#include "../input/engine_input.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <GLFW/glfw3.h>
#include "../core/project.h"
#include "../renderer/rhi.h"
#include "../renderer/render_system.h"
#include "../renderer/obj_loader.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <ctype.h>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if defined(_WIN32)
#include "../platform/win_dirent.h"
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif
#else
#include <dirent.h>
#endif

// External ECS functions
extern "C" Entity ecs_get_display_camera(const EcsWorld* world, int tag);
extern "C" void ecs_set_camera_display_tag(EcsWorld* world, Entity e, int tag);

// External scene functions
extern "C" Entity scene_spawn_primitive(EcsWorld* world, const char* primitive);

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

// Game window framebuffer (always visible now)
static RHIFramebuffer* game_fb = NULL;
static int game_fb_width = 0, game_fb_height = 0;

// Gizmo state
static bool gizmo_enabled = true;
static ImGuizmo::OPERATION current_gizmo_operation = ImGuizmo::TRANSLATE; // TRANSLATE, ROTATE, SCALE
static bool gizmo_using = false;

// Camera matrices for gizmos
static float camera_view_matrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
static float camera_projection_matrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

// View gizmo (the little orientation cube, top-right of the viewport, like Unity/Blender)
static float view_gizmo_matrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

static void decompose_view_matrix(const float* view, float* out_eye, float* out_yaw_deg, float* out_pitch_deg) {
    float right[3]       = { view[0], view[4], view[8]  };
    float up[3]          = { view[1], view[5], view[9]  };
    float neg_forward[3] = { view[2], view[6], view[10] };
    float forward[3]     = { -neg_forward[0], -neg_forward[1], -neg_forward[2] };

    float t[3] = { view[12], view[13], view[14] };

    // Inverse of an orthonormal look-at matrix: eye = -right*t.x - up*t.y + forward*t.z
    for (int i = 0; i < 3; i++) {
        out_eye[i] = -right[i] * t[0] - up[i] * t[1] + forward[i] * t[2];
    }

    *out_pitch_deg = asinf(forward[1]) * (180.0f / (float)M_PI);
    *out_yaw_deg = atan2f(forward[2], forward[0]) * (180.0f / (float)M_PI);
}

// Key state tracking for hotkeys
static bool key_q_pressed = false;
static bool key_w_pressed = false;
static bool key_e_pressed = false;
static bool key_r_pressed = false;
static bool key_t_pressed = false;
static bool key_s_pressed = false;

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
            ImGui::DockBuilderDockWindow("Game", main_id); // Game window in same area as viewport
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
            ImGui::DockBuilderDockWindow("Game", main_id);
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
            ImGui::DockBuilderDockWindow("Game", main_id);
            break;
        }
    }

    ImGui::DockBuilderFinish(dockspace_id);
}

// ---------------------------------------------------------------------

static void add_primitive(const char* primitive) {
    if (!world) return;
    selected_entity = scene_spawn_primitive(world, primitive);
    
    // Auto-set camera as display camera if it's the first one
    if (strcmp(primitive, "camera") == 0) {
        if (ecs_get_display_camera(world, 1) == ECS_INVALID_ENTITY) {
            ecs_set_camera_display_tag(world, selected_entity, 1);
        }
    }
}

// Small right-aligned remove button used by inspector component sections
static bool component_remove_button(const char* id) {
    char label[32];
    snprintf(label, sizeof(label), "Remove##%s", id);
    float w = ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - w);
    return ImGui::SmallButton(label);
}

static bool has_extension_ci(const char* name, const char* ext) {
    size_t name_len = strlen(name);
    size_t ext_len = strlen(ext);
    if (name_len < ext_len) return false;
    const char* tail = name + (name_len - ext_len);
    for (size_t i = 0; i < ext_len; i++) {
        if (tolower((unsigned char)tail[i]) != tolower((unsigned char)ext[i])) return false;
    }
    return true;
}

// Imports every mesh in an FBX file as a parented hierarchy of entities: the
// first mesh in the file becomes a root entity, and every other mesh in the
// file is created as a child underneath it (matching how Unity flattens a
// multi-object FBX import under one entity when it's dragged into a scene).
static void import_fbx_to_scene(const char* full_path, const char* relative_path) {
    if (!world) return;

    FbxImportResult result = obj_load_fbx_multi(full_path);
    if (result.count == 0) return; // obj_load_fbx_multi already printed why

    std::vector<Entity> created(result.count, ECS_INVALID_ENTITY);

    for (int i = 0; i < result.count; i++) {
        FbxMeshNode* n = &result.nodes[i];

        Entity e = ecs_create_entity(world, n->name);
        created[i] = e;

        ecs_add_component(world, e, COMPONENT_TRANSFORM);
        TransformComponent* t = &world->transforms[e];
        t->position = n->position;
        t->rotation = n->rotation;
        t->scale = n->scale;
        t->dirty = 1;

        ecs_add_component(world, e, COMPONENT_MESH);
        // Tag with the file + node name rather than the plain file path, so
        // render_system_sync() won't confuse this sub-mesh with any other
        // entity pointing at the same file, and won't try to re-load it
        // itself (it only knows how to return one mesh per path).
        snprintf(world->meshes[e].mesh_path, sizeof(world->meshes[e].mesh_path),
            "%s#%s", relative_path, n->name);
        render_system_assign_mesh(e, n->mesh, world->meshes[e].mesh_path);

        if (n->parent_index >= 0) {
            ecs_set_parent(world, e, created[n->parent_index]);
        }
    }

    selected_entity = created[0];
    obj_free_fbx_multi(&result);
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

            if (has_extension_ci(entry->d_name, ".fbx")) {
                char popup_id[560];
                snprintf(popup_id, sizeof(popup_id), "file_ctx_%s", full_path);
                if (ImGui::BeginPopupContextItem(popup_id)) {
                    if (ImGui::MenuItem("Import into Scene")) {
                        // full_path is relative to cwd (e.g.
                        // "projects/sample_project/assets/models/x.fbx");
                        // strip the project root prefix to get the path
                        // form MeshComponent.mesh_path expects (relative to
                        // the project, e.g. "assets/models/x.fbx").
                        const char* relative = full_path;
                        size_t root_len = strlen(project_root_path);
                        if (strncmp(full_path, project_root_path, root_len) == 0 && full_path[root_len] == '/') {
                            relative = full_path + root_len + 1;
                        }
                        import_fbx_to_scene(full_path, relative);
                    }
                    ImGui::EndPopup();
                }
            }
        }
    }
    closedir(dir);
}


// Renders one entity and its children as a tree, with drag-and-drop
// reparenting: drag any row onto another to make it a child, or onto the
// empty space below the tree to send it back to the top level.
static void draw_hierarchy_node(Entity e) {
    char label[96];
    snprintf(label, sizeof(label), "%s##%d", world->names[e], e);

    TransformComponent* t = &world->transforms[e];
    bool has_children = t->child_count > 0;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
        | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    if (!has_children) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (selected_entity == e) flags |= ImGuiTreeNodeFlags_Selected;

    bool open = ImGui::TreeNodeEx(label, flags);

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        selected_entity = e;
    }

    // Drag source: pick this entity up to reparent it elsewhere.
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &e, sizeof(Entity));
        ImGui::Text("%s", world->names[e]);
        ImGui::EndDragDropSource();
    }

    // Drop target: reparent whatever's being dragged under this entity.
    // Refuses drops that would create a cycle (dropping an entity onto
    // itself or onto one of its own descendants).
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
            Entity dragged = *(const Entity*)payload->Data;
            if (ecs_is_alive(world, dragged) && !ecs_is_ancestor(world, dragged, e)) {
                ecs_set_parent(world, dragged, e);
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Right-click context menu for hierarchy items
    if (ImGui::BeginPopupContextItem(label)) {
        selected_entity = e; // Select the right-clicked entity
        if (ImGui::BeginMenu("Add Component")) {
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
            if (!ecs_has_component(world, e, COMPONENT_CAMERA) && ImGui::MenuItem("Camera")) {
                ecs_add_component(world, e, COMPONENT_CAMERA);
                if (ecs_get_display_camera(world, 1) == ECS_INVALID_ENTITY) {
                    ecs_set_camera_display_tag(world, e, 1);
                }
            }
            if (!ecs_has_component(world, e, COMPONENT_LIGHT) && ImGui::MenuItem("Light")) {
                ecs_add_component(world, e, COMPONENT_LIGHT);
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Duplicate")) {
            Entity new_entity = ecs_create_entity(world, world->names[e]);
            // Copy components
            if (ecs_has_component(world, e, COMPONENT_TRANSFORM)) {
                ecs_add_component(world, new_entity, COMPONENT_TRANSFORM);
                world->transforms[new_entity] = world->transforms[e];
                // The copy above also copied the source entity's parent/
                // children bookkeeping, which still points at the ORIGINAL
                // entity's relationships, not the new one's - clear it and
                // reparent properly instead of ending up with two entities
                // both claiming the same children.
                world->transforms[new_entity].parent = ECS_INVALID_ENTITY;
                world->transforms[new_entity].child_count = 0;
                if (world->transforms[e].parent != ECS_INVALID_ENTITY) {
                    ecs_set_parent(world, new_entity, world->transforms[e].parent);
                }
            }
            if (ecs_has_component(world, e, COMPONENT_MESH)) {
                ecs_add_component(world, new_entity, COMPONENT_MESH);
                world->meshes[new_entity] = world->meshes[e];
            }
            if (ecs_has_component(world, e, COMPONENT_MATERIAL)) {
                ecs_add_component(world, new_entity, COMPONENT_MATERIAL);
                world->materials[new_entity] = world->materials[e];
            }
            if (ecs_has_component(world, e, COMPONENT_CAMERA)) {
                ecs_add_component(world, new_entity, COMPONENT_CAMERA);
                world->cameras[new_entity] = world->cameras[e];
            }
            if (ecs_has_component(world, e, COMPONENT_LIGHT)) {
                ecs_add_component(world, new_entity, COMPONENT_LIGHT);
                world->lights[new_entity] = world->lights[e];
            }
            selected_entity = new_entity;
        }
        if (ImGui::MenuItem("Delete")) {
            ecs_destroy_entity(world, e);
            selected_entity = ECS_INVALID_ENTITY;
        }
        ImGui::EndPopup();
    }

    if (has_children && open) {
        // Snapshot the child list before recursing - accepting a drag-drop
        // reparent anywhere below could mutate this entity's children array
        // (and child_count) mid-traversal otherwise.
        int child_count = t->child_count;
        Entity children[ECS_MAX_CHILDREN];
        memcpy(children, t->children, sizeof(Entity) * child_count);
        for (int c = 0; c < child_count; c++) {
            draw_hierarchy_node(children[c]);
        }
        ImGui::TreePop();
    }
}

extern "C" void editor_init(PlatformWindow* window) {
    platform_window = window;
    glfw_window = (GLFWwindow*)platform_get_gl_context(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // NOTE: ImGuiConfigFlags_ViewportsEnable (multi-OS-window docking) is deliberately
    // left off. It requires per-viewport DPI scale info from the platform backend,
    // and on some GPU/monitor combinations that comes back invalid, which trips
    // Dear ImGui's internal "g.CurrentDpiScale > 0.0f && g.CurrentDpiScale < 99.0f"
    // assertion in imgui.cpp and crashes on the very first frame. Docking windows
    // within the single game window (already enabled above) is unaffected.
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
    ImGuizmo::BeginFrame();
    
    // Handle gizmo hotkeys (Unity-style) - only on key press, not hold
    // Note: Q is used for camera movement down, so we use different keys for gizmos
    // While right-click camera fly-through is active, WASD/QE drive the camera
    // instead, so skip gizmo hotkey switching entirely to avoid W/S also
    // flipping the gizmo mode mid-flight.
    bool camera_controlling = ImGui::IsMouseDown(ImGuiMouseButton_Right);

    bool w_current = ImGui::IsKeyDown(ImGuiKey_W);
    bool e_current = ImGui::IsKeyDown(ImGuiKey_E);
    bool r_current = ImGui::IsKeyDown(ImGuiKey_R);
    bool t_current = ImGui::IsKeyDown(ImGuiKey_T);
    bool s_current = ImGui::IsKeyDown(ImGuiKey_S); // Use S for scale instead of Q

    if (!camera_controlling) {
        if (w_current && !key_w_pressed) current_gizmo_operation = ImGuizmo::TRANSLATE;
        if (e_current && !key_e_pressed) current_gizmo_operation = ImGuizmo::ROTATE;
        if (r_current && !key_r_pressed) current_gizmo_operation = ImGuizmo::SCALE;
        if (t_current && !key_t_pressed) current_gizmo_operation = ImGuizmo::TRANSLATE;
        if (s_current && !key_s_pressed) current_gizmo_operation = ImGuizmo::SCALE;
    }

    key_w_pressed = w_current;
    key_e_pressed = e_current;
    key_r_pressed = r_current;
    key_t_pressed = t_current;
    key_s_pressed = s_current;
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

extern "C" RHIFramebuffer* editor_get_game_framebuffer() {
    return game_fb;
}

extern "C" void editor_get_game_framebuffer_size(int* width, int* height) {
    *width = game_fb_width;
    *height = game_fb_height;
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
            if (ImGui::MenuItem("Camera")) add_primitive("camera");
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
            const char* run_label = playing ? "\xe2\x96\xa0  Stop" : "\xe2\x96\xb6  Run";
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
        if (world->transforms[i].parent != ECS_INVALID_ENTITY) continue; // drawn as someone's child instead
        draw_hierarchy_node(i);
    }

    // Drop target covering the empty space below the tree - drag an entity
    // here to unparent it back to the top level.
    ImGui::Dummy(ImGui::GetContentRegionAvail());
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
            Entity dragged = *(const Entity*)payload->Data;
            if (ecs_is_alive(world, dragged)) {
                ecs_set_parent(world, dragged, ECS_INVALID_ENTITY);
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Right-click context menu for empty hierarchy space
    if (ImGui::BeginPopupContextWindow("HierarchyContext")) {
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Empty Entity")) {
                selected_entity = ecs_create_entity(world, "Entity");
                ecs_add_component(world, selected_entity, COMPONENT_TRANSFORM);
            }
            if (ImGui::MenuItem("Cube")) {
                selected_entity = scene_spawn_primitive(world, "cube");
            }
            if (ImGui::MenuItem("Sphere")) {
                selected_entity = scene_spawn_primitive(world, "sphere");
            }
            if (ImGui::MenuItem("Plane")) {
                selected_entity = scene_spawn_primitive(world, "plane");
            }
            if (ImGui::MenuItem("Camera")) {
                selected_entity = scene_spawn_primitive(world, "camera");
                // Auto-set as main camera if it's the first one
                if (ecs_get_display_camera(world, 1) == ECS_INVALID_ENTITY) {
                    ecs_set_camera_display_tag(world, selected_entity, 1);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
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
        
        // Render gizmo for selected entity if enabled and has transform
        if (gizmo_enabled && ecs_is_alive(world, selected_entity) && 
            ecs_has_component(world, selected_entity, COMPONENT_TRANSFORM)) {
            
            // Set up ImGuizmo
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(img_pos.x, img_pos.y, vp_size.x, vp_size.y);
            
            // Get entity transform matrix
            TransformComponent* t = &world->transforms[selected_entity];
            float model_matrix[16];
            mat4_compose_trs(model_matrix, t->position, t->rotation, t->scale);
            
            // Store original matrix for delta calculation
            float original_matrix[16];
            memcpy(original_matrix, model_matrix, sizeof(original_matrix));
            
            // Manipulate the gizmo
            gizmo_using = ImGuizmo::Manipulate(
                camera_view_matrix, 
                camera_projection_matrix, 
                current_gizmo_operation, 
                ImGuizmo::LOCAL, 
                model_matrix
            );
            
            // Apply the modified transform back to the entity
            if (gizmo_using) {
                // Use ImGuizmo's decomposition to extract proper TRS components
                float translation[3], rotation[3], scale[3];
                ImGuizmo::DecomposeMatrixToComponents(model_matrix, translation, rotation, scale);
                
                // Update the entity's transform
                t->position.x = translation[0];
                t->position.y = translation[1];
                t->position.z = translation[2];
                
                t->rotation.x = rotation[0];
                t->rotation.y = rotation[1];
                t->rotation.z = rotation[2];
                
                t->scale.x = scale[0];
                t->scale.y = scale[1];
                t->scale.z = scale[2];
                
                t->dirty = 1;
            }
        }
        
        // Gizmo toggle button (top left, like Unity)
        ImVec2 button_pos = ImVec2(img_pos.x + 10, img_pos.y + 10);
        ImGui::SetCursorScreenPos(button_pos);
        
        const char* gizmo_modes[] = {"None", "Translate", "Rotate", "Scale"};
        int mode_index = 0;
        if (current_gizmo_operation == ImGuizmo::TRANSLATE) mode_index = 1;
        else if (current_gizmo_operation == ImGuizmo::ROTATE) mode_index = 2;
        else if (current_gizmo_operation == ImGuizmo::SCALE) mode_index = 3;
        
        if (ImGui::Button(gizmo_modes[mode_index])) {
            // Cycle through modes: None -> Translate -> Rotate -> Scale -> None
            if (current_gizmo_operation == ImGuizmo::TRANSLATE) current_gizmo_operation = ImGuizmo::ROTATE;
            else if (current_gizmo_operation == ImGuizmo::ROTATE) current_gizmo_operation = ImGuizmo::SCALE;
            else if (current_gizmo_operation == ImGuizmo::SCALE) current_gizmo_operation = ImGuizmo::TRANSLATE; // Skip None for now
        }
        
        {
            const float gizmo_size = 96.0f;
            const float margin = 10.0f;
            ImVec2 gizmo_pos = ImVec2(img_pos.x + vp_size.x - gizmo_size - margin, img_pos.y + margin);

            memcpy(view_gizmo_matrix, camera_view_matrix, sizeof(view_gizmo_matrix));

            ImGuizmo::ViewManipulate(
                view_gizmo_matrix,
                1.0f,
                gizmo_pos,
                ImVec2(gizmo_size, gizmo_size),
                0x10101010
            );

            if (ImGuizmo::IsUsingViewManipulate()) {
                float eye[3], yaw_deg, pitch_deg;
                decompose_view_matrix(view_gizmo_matrix, eye, &yaw_deg, &pitch_deg);

                if (pitch_deg > 89.0f) pitch_deg = 89.0f;
                if (pitch_deg < -89.0f) pitch_deg = -89.0f;

                engine_input_set_position(eye[0], eye[1], eye[2]);
                engine_input_set_yaw_pitch(yaw_deg, pitch_deg);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();

    // Game Window (always visible, shows real-time camera preview)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Game");
    
    ImVec2 game_size = ImGui::GetContentRegionAvail();
    int new_game_fb_w = (int)game_size.x;
    int new_game_fb_h = (int)game_size.y;
    if (new_game_fb_w < 1) new_game_fb_w = 1;
    if (new_game_fb_h < 1) new_game_fb_h = 1;
    
    if (!game_fb || new_game_fb_w != game_fb_width || new_game_fb_h != game_fb_height) {
        if (game_fb) {
            rhi_framebuffer_destroy(game_fb);
            game_fb = NULL;
        }
        game_fb = rhi_framebuffer_create(new_game_fb_w, new_game_fb_h);
        game_fb_width = new_game_fb_w;
        game_fb_height = new_game_fb_h;
    }

    Entity main_camera = ecs_get_display_camera(world, 1);
    bool has_camera = ecs_is_alive(world, main_camera);

    if (has_camera) {
        // Always show camera view (real-time preview)
        if (game_fb) {
            unsigned int tex_id = rhi_framebuffer_get_texture_id(game_fb);
            ImVec2 img_pos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddImageRounded(
                (ImTextureID)(uintptr_t)tex_id,
                img_pos, ImVec2(img_pos.x + game_size.x, img_pos.y + game_size.y),
                ImVec2(0,1), ImVec2(1,0), IM_COL32_WHITE, 8.0f);
            ImGui::Dummy(game_size);
            
            // Show mode indicator overlay
            bool is_playing = script_system_is_playing();
            ImVec2 overlay_pos = ImVec2(img_pos.x + 10, img_pos.y + 10);
            ImGui::SetCursorScreenPos(overlay_pos);
            if (is_playing) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "● PLAYING");
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "○ EDIT MODE");
            }
        }
    } else {
        // Show "No Camera" message
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 center_pos = ImVec2(
            window_pos.x + window_size.x * 0.5f,
            window_pos.y + window_size.y * 0.5f
        );
        
        ImVec2 text_size = ImGui::CalcTextSize("No Camera in Scene");
        ImVec2 start_pos = ImVec2(
            center_pos.x - text_size.x * 0.5f,
            center_pos.y - text_size.y * 0.5f
        );
        ImGui::SetCursorScreenPos(start_pos);
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No Camera in Scene");
        start_pos.y += text_size.y + 10;
        ImGui::SetCursorScreenPos(start_pos);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Add a Camera component to an entity");
        start_pos.y += text_size.y + 10;
        ImGui::SetCursorScreenPos(start_pos);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "and set it as Display Camera (Tag 1)");
        
        ImGui::PopStyleVar();
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
                if (ImGui::DragFloat3("Scale", sca, 0.05f, 0.01f, 10000.0f, "%.2f")) {
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

        // Camera component
        if (ecs_has_component(world, e, COMPONENT_CAMERA)) {
            ImGui::SeparatorText("Camera");
            if (component_remove_button("camera")) {
                ecs_remove_component(world, e, COMPONENT_CAMERA);
            } else {
                CameraComponent* cam = &world->cameras[e];
                
                // Display tag dropdown
                const char* tag_items[] = {"None", "Display 1", "Display 2", "Display 3", "Display 4", "Display 5"};
                int current_tag = cam->display_tag;
                if (current_tag < 0) current_tag = 0;
                if (current_tag > 5) current_tag = 5;
                
                if (ImGui::Combo("Display Tag", &current_tag, tag_items, 6)) {
                    ecs_set_camera_display_tag(world, e, current_tag);
                }
                
                // FOV slider
                float fov = cam->fov;
                if (ImGui::SliderFloat("FOV", &fov, 30.0f, 120.0f, "%.1f")) {
                    cam->fov = fov;
                }
                
                // Near/Far planes
                float near_p = cam->near_plane;
                float far_p = cam->far_plane;
                if (ImGui::DragFloat("Near Plane", &near_p, 0.01f, 0.01f, 10.0f, "%.2f")) {
                    cam->near_plane = near_p;
                }
                if (ImGui::DragFloat("Far Plane", &far_p, 1.0f, 10.0f, 1000.0f, "%.1f")) {
                    cam->far_plane = far_p;
                }
            }
        }

        // Light component
        if (ecs_has_component(world, e, COMPONENT_LIGHT)) {
            ImGui::SeparatorText("Light");
            if (component_remove_button("light")) {
                ecs_remove_component(world, e, COMPONENT_LIGHT);
            } else {
                LightComponent* light = &world->lights[e];

                float color[3] = {light->color[0], light->color[1], light->color[2]};
                if (ImGui::ColorEdit3("Colour##light", color, 0)) {
                    light->color[0] = color[0]; light->color[1] = color[1]; light->color[2] = color[2];
                }

                float intensity = light->intensity;
                if (ImGui::DragFloat("Intensity", &intensity, 0.05f, 0.0f, 20.0f, "%.2f")) {
                    light->intensity = intensity;
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
            if (!ecs_has_component(world, e, COMPONENT_CAMERA) && ImGui::MenuItem("Camera")) {
                ecs_add_component(world, e, COMPONENT_CAMERA);
                // Automatically set as display camera if it's the first one
                if (ecs_get_display_camera(world, 1) == ECS_INVALID_ENTITY) {
                    ecs_set_camera_display_tag(world, e, 1);
                }
            }
            if (!ecs_has_component(world, e, COMPONENT_LIGHT) && ImGui::MenuItem("Light")) {
                ecs_add_component(world, e, COMPONENT_LIGHT);
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

extern "C" void editor_set_camera_matrices(const float* view, const float* projection) {
    memcpy(camera_view_matrix, view, sizeof(camera_view_matrix));
    memcpy(camera_projection_matrix, projection, sizeof(camera_projection_matrix));
}

extern "C" void editor_get_camera_matrices(float* view, float* projection) {
    memcpy(view, camera_view_matrix, sizeof(camera_view_matrix));
    memcpy(projection, camera_projection_matrix, sizeof(camera_projection_matrix));
}