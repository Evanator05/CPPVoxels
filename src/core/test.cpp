#include "test.h"
#include "gui.h"
#include "input.h"
#include "console.h"
#include "deltatime.h"

#include "modules/voxel/voxelmanager.h"
#include "modules/voxel/voxelserializer.h"
#include "modules/voxelphysics/voxelphysics.h"
#include "modules/voxelrenderer/voxelrenderer.h"
#include "modules/voxelrenderer/testgeneration.h"
#include "filesystem.h"
#include "glm/glm.hpp"

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {
    // Wood, slate and brass: deliberately angular, like a hand-built workbench.
    void ApplyVoxelUIStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 2.0f;
        style.ChildRounding = 1.0f;
        style.FrameRounding = 2.0f;
        style.PopupRounding = 2.0f;
        style.GrabRounding = 1.0f;
        style.ScrollbarRounding = 1.0f;
        style.WindowBorderSize = 0.0f; // The carved wooden border is custom drawn.
        style.FrameBorderSize = 1.0f;
        style.WindowPadding = ImVec2(22.0f, 18.0f);
        style.FramePadding = ImVec2(9.0f, 7.0f);
        style.ItemSpacing = ImVec2(9.0f, 9.0f);
        style.ItemInnerSpacing = ImVec2(7.0f, 5.0f);
        style.IndentSpacing = 16.0f;
        style.GrabMinSize = 13.0f;
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

        ImVec4* c = style.Colors;
        c[ImGuiCol_Text]                 = ImVec4(0.96f, 0.88f, 0.72f, 1.00f);
        c[ImGuiCol_TextDisabled]         = ImVec4(0.72f, 0.62f, 0.46f, 1.00f);
        c[ImGuiCol_WindowBg]             = ImVec4(0.18f, 0.19f, 0.18f, 1.00f);
        c[ImGuiCol_ChildBg]              = ImVec4(0.15f, 0.16f, 0.15f, 1.00f);
        c[ImGuiCol_PopupBg]              = ImVec4(0.16f, 0.15f, 0.12f, 1.00f);
        c[ImGuiCol_Border]               = ImVec4(0.43f, 0.33f, 0.21f, 1.00f);
        c[ImGuiCol_BorderShadow]         = ImVec4(0.04f, 0.03f, 0.02f, 0.00f);
        c[ImGuiCol_FrameBg]              = ImVec4(0.22f, 0.22f, 0.20f, 1.00f);
        c[ImGuiCol_FrameBgHovered]       = ImVec4(0.31f, 0.29f, 0.24f, 1.00f);
        c[ImGuiCol_FrameBgActive]        = ImVec4(0.37f, 0.32f, 0.24f, 1.00f);
        c[ImGuiCol_TitleBg]              = ImVec4(0.29f, 0.17f, 0.10f, 1.00f);
        c[ImGuiCol_TitleBgActive]        = ImVec4(0.36f, 0.23f, 0.13f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.25f, 0.16f, 0.11f, 1.00f);
        c[ImGuiCol_MenuBarBg]            = ImVec4(0.28f, 0.18f, 0.11f, 1.00f);
        c[ImGuiCol_ScrollbarBg]          = ImVec4(0.14f, 0.13f, 0.11f, 1.00f);
        c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.39f, 0.28f, 0.17f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.53f, 0.37f, 0.22f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.65f, 0.46f, 0.26f, 1.00f);
        c[ImGuiCol_CheckMark]            = ImVec4(0.91f, 0.73f, 0.42f, 1.00f);
        c[ImGuiCol_SliderGrab]           = ImVec4(0.64f, 0.46f, 0.25f, 1.00f);
        c[ImGuiCol_SliderGrabActive]     = ImVec4(0.91f, 0.70f, 0.37f, 1.00f);
        c[ImGuiCol_Button]               = ImVec4(0.36f, 0.24f, 0.14f, 1.00f);
        c[ImGuiCol_ButtonHovered]        = ImVec4(0.48f, 0.33f, 0.18f, 1.00f);
        c[ImGuiCol_ButtonActive]         = ImVec4(0.59f, 0.41f, 0.22f, 1.00f);
        c[ImGuiCol_Header]               = ImVec4(0.37f, 0.26f, 0.16f, 1.00f);
        c[ImGuiCol_HeaderHovered]        = ImVec4(0.48f, 0.34f, 0.19f, 1.00f);
        c[ImGuiCol_HeaderActive]         = ImVec4(0.59f, 0.41f, 0.22f, 1.00f);
        c[ImGuiCol_Separator]            = ImVec4(0.52f, 0.40f, 0.25f, 0.85f);
        c[ImGuiCol_SeparatorHovered]     = ImVec4(0.80f, 0.59f, 0.33f, 1.00f);
        c[ImGuiCol_SeparatorActive]      = ImVec4(0.93f, 0.73f, 0.41f, 1.00f);
        c[ImGuiCol_ResizeGrip]           = ImVec4(0.4f, 0.3f, 0.2f, 0.2f);
        c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.6f, 0.4f, 0.2f, 0.6f);
        c[ImGuiCol_ResizeGripActive]     = ImVec4(0.8f, 0.6f, 0.3f, 1.0f);
        c[ImGuiCol_Tab]                  = ImVec4(0.24f, 0.18f, 0.12f, 1.00f);
        c[ImGuiCol_TabHovered]           = ImVec4(0.49f, 0.34f, 0.19f, 1.00f);
        c[ImGuiCol_TabActive]            = ImVec4(0.40f, 0.28f, 0.16f, 1.00f);
        c[ImGuiCol_PlotLines]            = ImVec4(0.91f, 0.72f, 0.41f, 1.00f);
        c[ImGuiCol_PlotHistogram]        = ImVec4(0.91f, 0.72f, 0.41f, 1.00f);
        c[ImGuiCol_TextSelectedBg]       = ImVec4(0.62f, 0.43f, 0.22f, 0.50f);
    }

    // Draw inside the window, but behind widgets. No textures or extra assets required.
    void DrawCraftedEditorFrame() {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetWindowPos();
        const ImVec2 s = ImGui::GetWindowSize();
        const float x = p.x, y = p.y, r = p.x + s.x, b = p.y + s.y;
        const ImU32 woodDark = IM_COL32(47, 28, 17, 255);
        const ImU32 wood = IM_COL32(91, 57, 32, 255);
        const ImU32 woodLight = IM_COL32(130, 88, 46, 255);
        const ImU32 iron = IM_COL32(61, 59, 53, 255);
        const ImU32 brass = IM_COL32(180, 143, 78, 255);

        // Layered stone face: hand-cut slate with a subtle stone grain.
        dl->AddRectFilled(ImVec2(x + 9, y + 33), ImVec2(r - 9, b - 9), IM_COL32(51, 54, 52, 255));
        for (int i = 0; i < 18; ++i) {
            const float yy = y + 54.0f + i * 22.0f;
            if (yy >= b - 13) break;
            const float xx = x + 18.0f + float((i * 71) % 195);
            dl->AddLine(ImVec2(xx, yy), ImVec2(xx + 28 + float(i % 3) * 11, yy - 2), IM_COL32(106, 108, 100, 22), 1.0f);
        }
        // Planks around the stone face and across the header.
        dl->AddRectFilled(ImVec2(x, y), ImVec2(r, y + 36), woodDark);
        dl->AddRectFilled(ImVec2(x + 5, y + 5), ImVec2(r - 5, y + 31), wood);
        dl->AddRectFilled(ImVec2(x + 5, y + 5), ImVec2(r - 5, y + 7), woodLight);
        dl->AddRectFilled(ImVec2(x + 5, y + 28), ImVec2(r - 5, y + 31), woodDark);
        dl->AddRectFilled(ImVec2(x, y + 32), ImVec2(x + 11, b), woodDark);
        dl->AddRectFilled(ImVec2(r - 11, y + 32), ImVec2(r, b), woodDark);
        dl->AddRectFilled(ImVec2(x + 3, y + 36), ImVec2(x + 8, b - 7), wood);
        dl->AddRectFilled(ImVec2(r - 8, y + 36), ImVec2(r - 3, b - 7), wood);
        dl->AddRectFilled(ImVec2(x, b - 11), ImVec2(r, b), woodDark);
        dl->AddRectFilled(ImVec2(x + 5, b - 8), ImVec2(r - 5, b - 4), wood);
        // Uneven wood grain, constrained to the title plank.
        for (int i = 0; i < 9; ++i) {
            float xx = x + 21 + i * 32.0f;
            if (xx > r - 22) break;
            dl->AddLine(ImVec2(xx, y + 10), ImVec2(xx + 18, y + 11 + (i % 3)), IM_COL32(188, 132, 71, 35));
            dl->AddLine(ImVec2(xx + 3, y + 25), ImVec2(xx + 13, y + 24), IM_COL32(20, 12, 7, 45));
        }
        // Riveted iron corner plates.
        for (int side = 0; side < 2; ++side) {
            const float cx = side == 0 ? x + 13 : r - 13;
            for (int end = 0; end < 2; ++end) {
                const float cy = end == 0 ? y + 17 : b - 13;
                dl->AddRectFilled(ImVec2(cx - 7, cy - 7), ImVec2(cx + 7, cy + 7), iron);
                dl->AddRect(ImVec2(cx - 7, cy - 7), ImVec2(cx + 7, cy + 7), IM_COL32(120, 109, 88, 255));
                dl->AddCircleFilled(ImVec2(cx, cy), 3.0f, brass, 8);
                dl->AddCircleFilled(ImVec2(cx - 0.7f, cy - 0.7f), 1.0f, IM_COL32(239, 208, 142, 255), 8);
            }
        }
        // A modest brass rule beneath the wooden header.
        dl->AddLine(ImVec2(x + 22, y + 35), ImVec2(r - 22, y + 35), brass, 1.0f);
    }

    struct CharacterController {
        glm::vec3 position{0.0f}; // Center of the feet in world space.
        glm::vec3 velocity{0.0f};
        float radius = 2.5f;
        float height = 18.0f;
        float eyeHeight = 16.0f;
        float skin = 0.2f;
        float moveSpeed = 50.0f;
        float acceleration = 25.0f;
        float airAcceleration = 6.0f;
        float gravity = 240.0f;
        float jumpSpeed = 80.0f;
        float terminalSpeed = 500.0f;
        float stepHeight = 5.0f;
        float stepSmoothing = 12.0f;
        float cameraOffset = 0.0f;
        bool grounded = false;
    };

    struct CharacterCast {
        float distance;
        bool hit = false;
        float surfaceHeight = 0.0f;
    };

    CharacterCast CastCharacter(VoxelPhysics& physics, const CharacterController& character,
                                glm::vec3 position, int axis, float movement) {
        const float distance = std::abs(movement);
        CharacterCast result{distance};
        if (distance <= 0.0f)
            return result;
        // Horizontal sweeps keep this skin gap in front of a stair. Include it
        // in the support footprint so short frame movements can find its top.
        const float radius = character.radius + (axis == 1 && movement < 0.0f ? character.skin : 0.0f);
        const glm::vec3 lower(-radius, 0.0f, -radius);
        const glm::vec3 upper(radius, character.height, radius);
        glm::vec3 direction(0.0f);
        direction[axis] = movement > 0.0f ? 1.0f : -1.0f;
        const int u = (axis + 1) % 3, v = (axis + 2) % 3;
        // Sample the leading face with gaps smaller than one voxel, so a voxel
        // cannot pass between the rays, even halfway up a wall or at a corner.
        const int samplesU = std::max(1, int(std::ceil((upper[u] - lower[u]) / 0.75f)));
        const int samplesV = std::max(1, int(std::ceil((upper[v] - lower[v]) / 0.75f)));
        for (int i = 0; i <= samplesU; ++i) {
            for (int j = 0; j <= samplesV; ++j) {
                glm::vec3 offset(0.0f);
                offset[axis] = movement > 0.0f ? upper[axis] : lower[axis];
                offset[u] = glm::mix(lower[u], upper[u], float(i) / samplesU);
                offset[v] = glm::mix(lower[v], upper[v], float(j) / samplesV);
                const RayQuery hit = physics.TraceWorld({position + offset, direction}, distance + character.skin);
                if (hit.hit && (axis != 1 || movement > 0.0f || hit.normal.y > 0.0f)) {
                    const float travel = std::max(0.0f, hit.depth - character.skin);
                    if (!result.hit || travel < result.distance)
                        result.surfaceHeight = float(hit.position.y) + (movement < 0.0f ? 1.0f : 0.0f);
                    result.hit = true;
                    result.distance = std::min(result.distance, travel);
                    if (result.distance <= character.skin * 0.01f) {
                        result.distance = 0.0f;
                        return result;
                    }
                }
            }
        }
        return result;
    }

    glm::vec3 SlideCharacter(VoxelPhysics& physics, const CharacterController& character,
                             glm::vec3 position, glm::vec3 motion) {
        const int first = std::abs(motion.x) >= std::abs(motion.z) ? 0 : 2;
        for (int axis : {first, 2 - first}) {
            const CharacterCast cast = CastCharacter(physics, character, position, axis, motion[axis]);
            position[axis] += std::copysign(cast.distance, motion[axis]);
        }
        return position;
    }

    void MoveCharacter(VoxelPhysics& physics, CharacterController& character, glm::vec3 targetVelocity, bool jump, float deltaTime) {
        if (!(deltaTime > 0.0f) || !std::isfinite(deltaTime))
            return;
        deltaTime = std::min(deltaTime, 0.1f);
        targetVelocity.y = 0.0f;
        character.radius = std::max(character.radius, 0.05f);
        character.height = std::max(character.height, 0.2f);
        character.skin = std::clamp(character.skin, 0.001f, std::min(character.radius, character.height) * 0.25f);
        character.eyeHeight = std::clamp(character.eyeHeight, character.skin, character.height - character.skin);
        character.stepHeight = std::clamp(character.stepHeight, 0.0f, character.height - character.skin);

        const float speed = std::max(glm::length(targetVelocity),
            glm::length(glm::vec3(character.velocity.x, 0.0f, character.velocity.z)));
        const int substeps = std::clamp(int(std::ceil(std::max(deltaTime * 60.0f, speed * deltaTime / character.radius))), 1, 16);
        const float dt = deltaTime / substeps;
        for (int iteration = 0; iteration < substeps; ++iteration) {
            if (jump && character.grounded) {
                character.velocity.y = character.jumpSpeed;
                character.grounded = false;
            }
            jump = false;
            const bool groundedMovement = character.grounded;
            const float acceleration = groundedMovement ? character.acceleration : character.airAcceleration;
            const float blend = 1.0f - std::exp(-std::max(acceleration, 0.0f) * dt);
            character.velocity.x = glm::mix(character.velocity.x, targetVelocity.x, blend);
            character.velocity.z = glm::mix(character.velocity.z, targetVelocity.z, blend);
            const glm::vec3 motion(character.velocity.x * dt, 0.0f, character.velocity.z * dt);
            const glm::vec3 start = character.position;
            glm::vec3 position = SlideCharacter(physics, character, start, motion);
            glm::vec3 progress = position - start;

            if (character.stepHeight > 0.0f && glm::dot(progress, progress) < glm::dot(motion, motion)) {
                const CharacterCast ceiling = CastCharacter(physics, character, start, 1, character.stepHeight);
                const float lift = std::min(character.stepHeight, ceiling.distance);
                if (lift > character.skin * 0.25f) {
                    glm::vec3 raised = start + glm::vec3(0.0f, lift, 0.0f);
                    raised = SlideCharacter(physics, character, raised, motion);
                    const CharacterCast floor = CastCharacter(physics, character, raised, 1,
                        -(lift + character.stepHeight));
                    raised.y = floor.surfaceHeight + character.skin;
                    const glm::vec3 stepProgress(raised.x - start.x, 0.0f, raised.z - start.z);
                    const float rise = raised.y - start.y;
                    const float tolerance = character.skin * 0.01f;
                    if (floor.hit && rise > tolerance && rise <= lift + tolerance &&
                        rise <= character.stepHeight + tolerance && glm::dot(stepProgress - progress, motion) > 0.0f) {
                        position = raised;
                        progress = stepProgress;
                        character.cameraOffset -= rise;
                    }
                }
            }
            // Follow a lower surface using the same height limit and visual
            // compensation as an upward step, measured from this move's start.
            if (groundedMovement && character.stepHeight > 0.0f && position.y <= start.y) {
                const CharacterCast floor = CastCharacter(physics, character, position, 1, -character.stepHeight);
                const float landingHeight = floor.surfaceHeight + character.skin;
                const float drop = start.y - landingHeight;
                if (floor.hit && drop > 0.0f && drop <= character.stepHeight + character.skin * 0.01f) {
                    position.y = landingHeight;
                    character.cameraOffset += drop;
                }
            }
            character.position = position;
            if (std::abs(progress.x - motion.x) > 1e-4f)
                character.velocity.x = 0.0f;
            if (std::abs(progress.z - motion.z) > 1e-4f)
                character.velocity.z = 0.0f;

            character.velocity.y = std::max(character.velocity.y - character.gravity * dt, -character.terminalSpeed);
            float vertical = character.velocity.y * dt;
            const CharacterCast verticalCast = CastCharacter(physics, character, character.position, 1, vertical);
            const float travel = std::copysign(verticalCast.distance, vertical);
            character.position.y += travel;
            character.grounded = vertical <= 0.0f && verticalCast.hit;
            if (verticalCast.hit)
                character.velocity.y = 0.0f;
            character.cameraOffset *= std::exp(-std::max(character.stepSmoothing, 0.0f) * dt);
            character.cameraOffset = std::max(character.cameraOffset, character.skin - character.eyeHeight);
            if (character.cameraOffset > 0.0f) {
                const glm::vec3 eye = character.position + glm::vec3(0.0f, character.eyeHeight, 0.0f);
                const RayQuery ceiling = physics.TraceWorld({eye, {0.0f, 1.0f, 0.0f}}, character.cameraOffset + character.skin);
                if (ceiling.hit)
                    character.cameraOffset = std::min(character.cameraOffset, std::max(0.0f, ceiling.depth - character.skin));
            }
        }
    }
}


void Test::Print(std::string output) {
    Console &console = GetModule<Console>();
    console.Log(output, Console::LogLevel::Info);
}


void printBinary(uint64_t value) {
    for (int i = 63; i >= 0; i--)
        printf("%llu", (value >> i) & 1ULL);

    printf("\n");
}

static bool characterMode = true;
void Test::Init() {
    Console &console = GetModule<Console>();

    console.CreateCommand(
        "print",
        [this](std::string output)
        {
            Print(output);
        }
    );

    console.CreateCommand(
        "noclip",
        [this, &console]() {
            characterMode = !characterMode;
            console.Log("Noclip " + std::string(characterMode ? "off" : "on"), Console::LogLevel::Info);
        }
    );
    FileSystem &fs = GetModule<FileSystem>();
    VoxelManager &vm = GetModule<VoxelManager>();

    // create test.pack to use from now on
    fs.BindFolder("assets");
    Generator::LoadVoxFile(vm, "assets/castle.vox");
}


void Test::Process() {
    Input &input = GetModule<Input>();
    Console &console = GetModule<Console>();
    VoxelRenderer &vr = GetModule<VoxelRenderer>();
    VoxelManager &vm = GetModule<VoxelManager>();

    const float deltaTime = GetModule<DeltaTime>().Get();

    static bool cursorLocked = false;
    static CharacterController character;
    static bool characterInitialized = false;

    if (input.IsPressed("lock_cursor")) {
        cursorLocked = !cursorLocked;
        input.SetMouseLock(cursorLocked);
    }


    float moveSpeed = characterMode ? character.moveSpeed : 50.0f;

    if (input.IsHeld("speedmodifier"))
        moveSpeed *= characterMode ? 1.75f : 5.0f;

    vr.cameraTransform.time += deltaTime;

    static float yaw = 0.0f;
    static float pitch = 0.0f;

    constexpr float lookSpeed = 1.25f;

    if (cursorLocked) {
        const float lookX =
            input.GetDelta("lookright", deltaTime) -
            input.GetDelta("lookleft", deltaTime);

        const float lookY =
            input.GetDelta("lookup", deltaTime) -
            input.GetDelta("lookdown", deltaTime);

        yaw += lookX * lookSpeed;
        pitch += lookY * lookSpeed;
    }

    constexpr float pitchLimit = glm::half_pi<float>() - 0.001f;

    pitch = glm::clamp(pitch, -pitchLimit, pitchLimit);

    glm::vec3 forward;

    forward.x = std::cos(pitch) * std::sin(yaw);
    forward.y = std::sin(pitch);
    forward.z = std::cos(pitch) * std::cos(yaw);

    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    const glm::vec3 right = glm::normalize(glm::cross(worldUp, forward));
    const glm::vec3 up = glm::normalize(glm::cross(forward, right));


    vr.cameraTransform.rotation0 = right;
    vr.cameraTransform.rotation1 = up;
    vr.cameraTransform.rotation2 = forward;

    glm::vec3 movement(
        input.GetStrength("right") - input.GetStrength("left"),
        characterMode ? 0.0f : input.GetStrength("up") - input.GetStrength("down"),
        input.GetStrength("forward") - input.GetStrength("backward")
    );


    const float movementLength =
        glm::length(movement);

    if (movementLength > 1.0f)
        movement /= movementLength;


    const glm::vec3 worldMovement =
        right * movement.x +
        up * movement.y +
        forward * movement.z;


    VoxelPhysics physics(engine);
    if (characterMode) {
        if (!characterInitialized) {
            character.position = vr.cameraTransform.localPos - glm::vec3(0.0f, character.eyeHeight, 0.0f);
            character.velocity = glm::vec3(0.0f);
            character.cameraOffset = 0.0f;
            character.grounded = false;
            const RayQuery ground = physics.TraceWorld({vr.cameraTransform.localPos, {0.0f, -1.0f, 0.0f}}, character.eyeHeight);
            if (ground.hit && ground.normal.y > 0.0f)
                character.position.y = glm::max(character.position.y, vr.cameraTransform.localPos.y - ground.depth + character.skin);
            characterInitialized = true;
        }
        const glm::vec3 groundForward(std::sin(yaw), 0.0f, std::cos(yaw));
        const glm::vec3 targetVelocity = (right * movement.x + groundForward * movement.z) * moveSpeed;
        MoveCharacter(physics, character, targetVelocity, input.IsPressed("up"), deltaTime);
        vr.cameraTransform.localPos = character.position + glm::vec3(0.0f, character.eyeHeight + character.cameraOffset, 0.0f);
    } else {
        characterInitialized = false;
        vr.cameraTransform.localPos += worldMovement * moveSpeed * deltaTime;
    }
    vr.cameraTransform.frame++;

    // Apply once, after the GUI context has been created.
    static bool uiStyleInitialized = false;
    if (!uiStyleInitialized) {
        ApplyVoxelUIStyle();
        uiStyleInitialized = true;
    }

    const ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    const ImVec2 crosshair(screenSize.x * 0.5f, screenSize.y * 0.5f);
    ImDrawList* overlay = ImGui::GetForegroundDrawList();
    overlay->AddLine(ImVec2(crosshair.x - 5.0f, crosshair.y), ImVec2(crosshair.x + 5.0f, crosshair.y), IM_COL32(0, 0, 0, 255), 3.0f);
    overlay->AddLine(ImVec2(crosshair.x, crosshair.y - 5.0f), ImVec2(crosshair.x, crosshair.y + 5.0f), IM_COL32(0, 0, 0, 255), 3.0f);
    overlay->AddLine(ImVec2(crosshair.x - 5.0f, crosshair.y), ImVec2(crosshair.x + 5.0f, crosshair.y), IM_COL32(255, 255, 255, 255));
    overlay->AddLine(ImVec2(crosshair.x, crosshair.y - 5.0f), ImVec2(crosshair.x, crosshair.y + 5.0f), IM_COL32(255, 255, 255, 255));

    enum class EditShape {
        Cube,
        Sphere
    };

    static EditShape editShape = EditShape::Sphere;
    static bool addVoxels = true;
    static int editSize = 8;
    static float editDistance = 80.0f;
    static float editInterval = 0.05f;
    static double nextEditTime = 0.0;
    static bool useRaycast = true;
    static float editColor[3] = { 1.0f, 1.0f, 1.0f };
    static int materialType = 0;
    static int materialProperty = 0;

    // Pin the right/bottom edge to the viewport, including after window resizes.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float uiMargin = 18.0f;
    const ImVec2 editorBottomRight(
        viewport->WorkPos.x + viewport->WorkSize.x - uiMargin,
        viewport->WorkPos.y + viewport->WorkSize.y - uiMargin
    );
    ImGui::SetNextWindowPos(editorBottomRight, ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_Always);
    ImGui::Begin("Voxel Editor", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    DrawCraftedEditorFrame();
    ImGui::SetCursorPosY(9.0f);
    const char* editorTitle = "VOXEL WORKBENCH";
    const float titleWidth = ImGui::CalcTextSize(editorTitle).x;
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - titleWidth) * 0.5f);
    ImGui::TextColored(ImVec4(0.96f, 0.83f, 0.57f, 1.0f), "%s", editorTitle);
    ImGui::SetCursorPosY(48.0f);
    ImGui::TextDisabled("EDIT MODE");
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::RadioButton("Add Voxels", addVoxels))
        addVoxels = true;

    ImGui::SameLine();

    if (ImGui::RadioButton("Delete Voxels", !addVoxels))
        addVoxels = false;
    
    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextDisabled("SHAPE");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::RadioButton("Sphere", editShape == EditShape::Sphere))
        editShape = EditShape::Sphere;
    

    ImGui::SameLine();

    if (ImGui::RadioButton("Cube", editShape == EditShape::Cube))
        editShape = EditShape::Cube;
    
    ImGui::Spacing();
    ImGui::PushItemWidth(-1.0f);
    ImGui::SliderInt(
        "##VoxelSize",
        &editSize,
        1,
        128,
        "Size: %d"
    );

    ImGui::SliderFloat(
        "##EditDistance",
        &editDistance,
        1.0f,
        256.0f,
        useRaycast ? "Max distance: %.1f" : "Distance: %.1f"
    );
    ImGui::Checkbox("Raycast targeting", &useRaycast);
    ImGui::SliderFloat("##EditInterval", &editInterval, 0.01f, 1.0f, "Edit interval: %.2f s", ImGuiSliderFlags_AlwaysClamp);

    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextDisabled("MATERIAL");
    ImGui::Separator();
    ImGui::Spacing();

    if (addVoxels) {
        ImGui::Text("Color");

        ImGui::ColorEdit3(
            "##VoxelColor",
            editColor,
            ImGuiColorEditFlags_NoInputs
        );


        ImGui::Spacing();


        const char *materialTypes[] = {
            "Regular",
            "Emissive"
        };

        ImGui::Text("Material Type");

        ImGui::PushItemWidth(-1.0f);

        ImGui::Combo(
            "##MaterialType",
            &materialType,
            materialTypes,
            IM_ARRAYSIZE(materialTypes)
        );

        ImGui::Spacing();

        ImGui::SliderInt(
            "##MaterialProperty",
            &materialProperty,
            0,
            31,
            "Material Property: %d"
        );

        ImGui::PopItemWidth();
    }
    else
    {
        ImGui::TextDisabled(
            "Material settings are ignored while deleting."
        );
    }

    ImGui::End();

    const bool editDue = input.IsHeld("edit") && ImGui::GetTime() >= nextEditTime;
    const RayQuery hit = useRaycast && editDue ? physics.TraceWorld({vr.cameraTransform.localPos, forward}, editDistance) : RayQuery{};
    if (editDue && (!useRaycast || hit.hit)) {
        nextEditTime = ImGui::GetTime() + editInterval;
        Voxel voxel{};

        voxel.set_solid(addVoxels);


        if (addVoxels)
        {
            voxel.set_rgb(
                static_cast<uint8_t>(
                    glm::clamp(
                        editColor[0],
                        0.0f,
                        1.0f
                    ) * 31.0f
                ),

                static_cast<uint8_t>(
                    glm::clamp(
                        editColor[1],
                        0.0f,
                        1.0f
                    ) * 31.0f
                ),

                static_cast<uint8_t>(
                    glm::clamp(
                        editColor[2],
                        0.0f,
                        1.0f
                    ) * 31.0f
                )
            );


            voxel.set_type(
                materialType == 0
                    ? Voxel::Type::Regular
                    : Voxel::Type::Emissive
            );


            voxel.set_payload(
                static_cast<uint8_t>(
                    materialProperty
                )
            );
        }


        const glm::vec3 editPositionFloat =
            vr.cameraTransform.localPos +
            forward * editDistance;


        const glm::ivec3 editPosition =
            useRaycast ? hit.position :
            glm::ivec3(
                glm::floor(
                    editPositionFloat
                )
            );


        if (editShape == EditShape::Sphere)
        {
            vm.FillSphere(
                editPosition,
                static_cast<float>(editSize),
                voxel
            );
        }
        else
        {
            const glm::ivec3 extent(
                editSize
            );

            vm.FillVoxels(
                editPosition - extent,
                editPosition + extent,
                voxel
            );
        }
    }
}
