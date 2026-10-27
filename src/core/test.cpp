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


void Test::Init() {
    Console &console = GetModule<Console>();
    Input &input = GetModule<Input>();
    input.CreateAction("toggle_character");
    input.CreateBinding("toggle_character", SDLK_G);

    console.CreateCommand(
        "print",
        [this](std::string output)
        {
            Print(output);
        }
    );

    FileSystem &fs = GetModule<FileSystem>();
    VoxelManager &vm = GetModule<VoxelManager>();

    // create test.pack to use from now on
    //fs.BindFolder("assets");
    //fs.SavePack("test.pack");

    fs.BindPack("test.pack");
    Generator::LoadVoxFile(vm, "assets/castle.vox");
    //GetModule<VoxelSerializer>().SaveWorld();
    //GetModule<VoxelSerializer>().LoadWorld();
}


void Test::Process() {
    Input &input = GetModule<Input>();
    Console &console = GetModule<Console>();
    VoxelRenderer &vr = GetModule<VoxelRenderer>();
    VoxelManager &vm = GetModule<VoxelManager>();

    const float deltaTime = GetModule<DeltaTime>().Get();

    static bool cursorLocked = false;
    static CharacterController character;
    static bool characterMode = true;
    static bool characterInitialized = false;

    if (input.IsPressed("toggle_character") && !ImGui::GetIO().WantCaptureKeyboard)
        characterMode = !characterMode;

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
    static float editDistance = 10.0f;
    static float editInterval = 0.1f;
    static double nextEditTime = 0.0;
    static bool useRaycast = false;
    static float editColor[3] = { 1.0f, 1.0f, 1.0f };
    static int materialType = 0;
    static int materialProperty = 0;

    ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Voxel Editor");
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


    // ============================================================
    // Stats / Performance
    // ============================================================

    static float elapsed = 0.0f;
    static uint32_t frames = 0;
    static int displayedFPS = 0;
    static float displayedFrameTimeMS = 0.0f;

    elapsed += deltaTime;
    frames++;

    if (elapsed >= 0.1f)
    {
        displayedFPS =
            static_cast<int>(
                std::round(
                    frames / elapsed
                )
            );

        displayedFrameTimeMS =
            (elapsed / static_cast<float>(frames)) * 1000.0f;

        elapsed = 0.0f;
        frames = 0;
    }

    ImGui::Begin("Stats");

    ImGui::Text(
        "FPS: %d",
        displayedFPS
    );

    ImGui::Text(
        "Frame Time: %.3f ms",
        displayedFrameTimeMS
    );

    ImGui::Separator();
    ImGui::End();
}
