#include "test.h"
#include "gui.h"
#include "input.h"
#include "console.h"
#include "deltatime.h"

#include "modules/voxel/voxelmanager.h"
#include "modules/voxelrenderer/voxelrenderer.h"

#include "glm/glm.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>


void Test::Print(std::string output)
{
    Console &console = GetModule<Console>();
    console.Log(output, Console::LogLevel::Info);
}


void printBinary(uint64_t value)
{
    for (int i = 63; i >= 0; i--)
        printf("%llu", (value >> i) & 1ULL);

    printf("\n");
}


void Test::Init()
{
    Console &console = GetModule<Console>();

    console.CreateCommand(
        "print",
        [this](std::string output)
        {
            Print(output);
        }
    );
}


void Test::Process()
{
    Input &input = GetModule<Input>();
    Console &console = GetModule<Console>();
    VoxelRenderer &vr = GetModule<VoxelRenderer>();
    VoxelManager &vm = GetModule<VoxelManager>();

    const float deltaTime = GetModule<DeltaTime>().Get();

    // ============================================================
    // Cursor lock
    // ============================================================

    static bool cursorLocked = false;

    if (input.IsPressed("lock_cursor"))
    {
        cursorLocked = !cursorLocked;
        input.SetMouseLock(cursorLocked);
    }


    // ============================================================
    // Camera
    // ============================================================

    float moveSpeed = 50.0f;

    if (input.IsHeld("speedmodifier"))
        moveSpeed *= 5.0f;

    vr.cameraTransform.time += deltaTime;


    // ------------------------------------------------------------
    // Camera rotation
    // ------------------------------------------------------------

    static float yaw = 0.0f;
    static float pitch = 0.0f;

    constexpr float lookSpeed = 1.25f;

    if (cursorLocked)
    {
        const float lookX =
            input.GetDelta("lookright", deltaTime) -
            input.GetDelta("lookleft", deltaTime);

        const float lookY =
            input.GetDelta("lookup", deltaTime) -
            input.GetDelta("lookdown", deltaTime);

        yaw += lookX * lookSpeed;
        pitch += lookY * lookSpeed;
    }

    constexpr float pitchLimit =
        glm::half_pi<float>() - 0.001f;

    pitch = glm::clamp(
        pitch,
        -pitchLimit,
        pitchLimit
    );


    glm::vec3 forward;

    forward.x = std::cos(pitch) * std::sin(yaw);
    forward.y = std::sin(pitch);
    forward.z = std::cos(pitch) * std::cos(yaw);


    const glm::vec3 worldUp(
        0.0f,
        1.0f,
        0.0f
    );

    const glm::vec3 right =
        glm::normalize(
            glm::cross(worldUp, forward)
        );

    const glm::vec3 up =
        glm::normalize(
            glm::cross(forward, right)
        );


    vr.cameraTransform.rotation0 = right;
    vr.cameraTransform.rotation1 = up;
    vr.cameraTransform.rotation2 = forward;


    // ------------------------------------------------------------
    // Camera movement
    // ------------------------------------------------------------

    glm::vec3 movement(
        input.GetStrength("right") -
            input.GetStrength("left"),

        input.GetStrength("up") -
            input.GetStrength("down"),

        input.GetStrength("forward") -
            input.GetStrength("backward")
    );


    const float movementLength =
        glm::length(movement);

    if (movementLength > 1.0f)
        movement /= movementLength;


    const glm::vec3 worldMovement =
        right * movement.x +
        up * movement.y +
        forward * movement.z;


    vr.cameraTransform.localPos +=
        worldMovement *
        moveSpeed *
        deltaTime;

    vr.cameraTransform.frame++;


    // ============================================================
    // Voxel editor state
    // ============================================================

    enum class EditShape
    {
        Cube,
        Sphere
    };


    static EditShape editShape =
        EditShape::Sphere;

    static bool addVoxels = true;

    static int editSize = 8;

    static float editDistance =
        10.0f;

    static float editColor[3] =
    {
        1.0f,
        1.0f,
        1.0f
    };

    static int materialType = 0;
    static int materialProperty = 0;


    // ============================================================
    // Voxel editor UI
    // ============================================================

    ImGui::SetNextWindowSize(
        ImVec2(340.0f, 0.0f),
        ImGuiCond_FirstUseEver
    );

    ImGui::Begin("Voxel Editor");


    // ------------------------------------------------------------
    // Edit mode
    // ------------------------------------------------------------

    ImGui::TextDisabled("EDIT MODE");
    ImGui::Separator();
    ImGui::Spacing();


    if (ImGui::RadioButton(
            "Add Voxels",
            addVoxels))
    {
        addVoxels = true;
    }

    ImGui::SameLine();

    if (ImGui::RadioButton(
            "Delete Voxels",
            !addVoxels))
    {
        addVoxels = false;
    }


    ImGui::Spacing();
    ImGui::Spacing();


    // ------------------------------------------------------------
    // Shape
    // ------------------------------------------------------------

    ImGui::TextDisabled("SHAPE");
    ImGui::Separator();
    ImGui::Spacing();


    if (ImGui::RadioButton(
            "Sphere",
            editShape == EditShape::Sphere))
    {
        editShape = EditShape::Sphere;
    }

    ImGui::SameLine();

    if (ImGui::RadioButton(
            "Cube",
            editShape == EditShape::Cube))
    {
        editShape = EditShape::Cube;
    }


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
        "Distance: %.1f"
    );

    ImGui::PopItemWidth();


    ImGui::Spacing();
    ImGui::Spacing();


    // ------------------------------------------------------------
    // Material
    // ------------------------------------------------------------

    ImGui::TextDisabled("MATERIAL");
    ImGui::Separator();
    ImGui::Spacing();


    if (addVoxels)
    {
        ImGui::Text("Color");

        ImGui::ColorEdit3(
            "##VoxelColor",
            editColor,
            ImGuiColorEditFlags_NoInputs
        );


        ImGui::Spacing();


        const char *materialTypes[] =
        {
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


    ImGui::Spacing();
    ImGui::Spacing();


    // ------------------------------------------------------------
    // Status
    // ------------------------------------------------------------

    ImGui::TextDisabled("STATUS");
    ImGui::Separator();
    ImGui::Spacing();


    ImGui::Text(
        "Operation: %s",
        addVoxels ? "Add" : "Delete"
    );

    ImGui::Text(
        "Shape: %s",
        editShape == EditShape::Sphere
            ? "Sphere"
            : "Cube"
    );

    ImGui::Text(
        "Cursor: %s",
        cursorLocked
            ? "Locked"
            : "Unlocked"
    );


    ImGui::End();


    // ============================================================
    // Perform voxel edit
    // ============================================================

    if (input.IsHeld("edit"))
    {
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
    // Stats / FPS
    // ============================================================

    static float elapsed = 0.0f;
    static uint32_t frames = 0;
    static int displayedFPS = 0;

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

        elapsed = 0.0f;
        frames = 0;
    }


    ImGui::Begin("Stats");

    ImGui::Text(
        "FPS: %d",
        displayedFPS
    );

    ImGui::Separator();

    ImGui::Text(
        "Position: %.2f  %.2f  %.2f",
        vr.cameraTransform.localPos.x,
        vr.cameraTransform.localPos.y,
        vr.cameraTransform.localPos.z
    );

    ImGui::Text(
        "Frame: %d",
        vr.cameraTransform.frame
    );

    ImGui::Text(
        "Cursor Locked: %s",
        cursorLocked
            ? "Yes"
            : "No"
    );

    ImGui::End();
}