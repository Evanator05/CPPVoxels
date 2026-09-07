/*
    opengametools vox file reader/writer/merger - v0.997 - MIT license - Justin Paver, Oct 2019

    This is a single-header-file library that provides easy-to-use
    support for reading MagicaVoxel .vox files into structures that
    are easy to dereference and extract information from. It also
    supports writing back out to .vox file from those structures.

    Please see the MIT license information at the end of this file.

    Also, please consider sharing any improvements you make.

    For more information and more tools, visit:
      https://github.com/jpaver/opengametools

    HOW TO COMPILE THIS LIBRARY

    1.  To compile this library, do this in *one* C++ file:
        #define OGT_VOX_IMPLEMENTATION
        #include "ogt_vox.h"

    2. From any other module, it is sufficient to just #include this as usual:
        #include "ogt_vox.h"

    HOW TO READ A VOX SCENE (See demo_vox.cpp)

    1. load a .vox file off disk into a memory buffer.

    2. construct a scene from the memory buffer:
       ogt_vox_scene* scene = ogt_vox_read_scene(buffer, buffer_size);

    3. use the scene members to extract the information you need. eg.
       printf("# of layers: %u\n", scene->num_layers );

    4. destroy the scene:
       ogt_vox_destroy_scene(scene);

    HOW TO MERGE MULTIPLE VOX SCENES (See merge_vox.cpp)

    1. construct multiple scenes from files you want to merge.

        // read buffer1/buffer_size1 from "test1.vox"
        // read buffer2/buffer_size2 from "test2.vox"
        // read buffer3/buffer_size3 from "test3.vox"
        ogt_vox_scene* scene1 = ogt_vox_read_scene(buffer1, buffer_size1);
        ogt_vox_scene* scene2 = ogt_vox_read_scene(buffer2, buffer_size2);
        ogt_vox_scene* scene3 = ogt_vox_read_scene(buffer3, buffer_size3);

    2. construct a merged scene

        const ogt_vox_scene* scenes[] = {scene1, scene2, scene3};
        ogt_vox_scene* merged_scene = ogt_vox_merge_scenes(scenes, 3, NULL, 0);

    3. save out the merged scene

        uint8_t* out_buffer = ogt_vox_write_scene(merged_scene, &out_buffer_size);
        // save out_buffer to disk as a .vox file (it has length out_buffer_size)

    4. destroy the merged scene:

        ogt_vox_destroy_scene(merged_scene);

    EXPLANATION OF SCENE ELEMENTS:

    A ogt_vox_scene comprises primarily a set of instances, models, layers and a palette.

    A ogt_vox_palette contains a set of 256 colors that is used for the scene.
    Each color is represented by a 4-tuple called an ogt_vox_rgba which contains red,
    green, blue and alpha values for the color.

    A ogt_vox_model is a 3-dimensional grid of voxels, where each of those voxels
    is represented by an 8-bit color index. Voxels are arranged in order of increasing
    X then increasing Y then increasing Z.

    Given the x,y,z values for a voxel within the model dimensions, the voxels index
    in the grid can be obtained as follows:

        voxel_index = x + (y * model->size_x) + (z * model->size_x * model->size_y)

    The index is only valid if the coordinate x,y,z satisfy the following conditions:
            0 <= x < model->size_x -AND-
            0 <= y < model->size_y -AND-
            0 <= z < model->size_z

    A voxels color index can be obtained as follows:

        uint8_t color_index = model->voxel_data[voxel_index];

    If color_index == 0, the voxel is not solid and can be skipped,
    If color_index != 0, the voxel is solid and can be used to lookup the color in the palette:

        ogt_vox_rgba color = scene->palette.color[ color_index]

    A ogt_vox_instance is an individual placement of a voxel model within the scene. Each
    instance has a transform that determines its position and orientation within the scene,
    but it also has an index that specifies which model the instance uses for its shape. It
    is expected that there is a many-to-one mapping of instances to models.
    Instances can overlap. For file version 200 (or perhaps higher) lower instance numbers are
    more important and would overwrite existing voxels on merging, for version < 200 the higher
    numbers are more important.

    An ogt_vox_layer is used to conceptually group instances. Each instance indexes the
    layer that it belongs to, but the layer itself has its own name and hidden/shown state.

    EXPLANATION OF MERGED SCENES:

    A merged scene contains all the models and all the scene instances from
    each of the scenes that were passed into it.

    The merged scene will have a combined palette of all the source scene
    palettes by trying to match existing colors exactly, and falling back
    to an RGB-distance matched color when all 256 colors in the merged
    scene palette has been allocated.

    You can explicitly control up to 255 merge palette colors by providing
    those colors to ogt_vox_merge_scenes in the required_colors parameters eg.

        const ogt_vox_palette palette;  // load this via .vox or procedurally or whatever
        const ogt_vox_scene* scenes[] = {scene1, scene2, scene3};
        // palette.color[0] is always the empty color which is why we pass 255 colors starting from index 1 only:
        ogt_vox_scene* merged_scene = ogt_vox_merge_scenes(scenes, 3, &palette.color[1], 255);

    EXPLANATION OF MODEL PIVOTS

    If a voxel model grid has dimension size.xyz in terms of number of voxels, the centre pivot
    for that model is located at floor( size.xyz / 2).

    eg. for a 3x4x1 voxel model, the pivot would be at (1,2,0), or the X in the below ascii art.

           4 +-----+-----+-----+
             |  .  |  .  |  .  |
           3 +-----+-----+-----+
             |  .  |  .  |  .  |
           2 +-----X-----+-----+
             |  .  |  .  |  .  |
           1 +-----+-----+-----+
             |  .  |  .  |  .  |
           0 +-----+-----+-----+
             0     1     2     3

     An example model in this grid form factor might look like this:

           4 +-----+-----+-----+
             |  .  |  .  |  .  |
           3 +-----+-----+-----+
                   |  .  |
           2       X-----+
                   |  .  |
           1       +-----+
                   |  .  |
           0       +-----+
             0     1     2     3

     If you were to generate a mesh from this, clearly each vertex and each face would be on an integer
     coordinate eg. 1, 2, 3 etc. while the centre of each grid location (ie. the . in the above diagram)
     will be on a coordinate that is halfway between integer coordinates. eg. 1.5, 2.5, 3.5 etc.

     To ensure your mesh is properly centered such that instance transforms are correctly applied, you
     want the pivot to be treated as if it were (0,0,0) in model space. To achieve this, simply
     subtract the pivot from any geometry that is generated (eg. vertices in a mesh).

     For the 3x4x1 voxel model above, doing this would look like this:

           2 +-----+-----+-----+
             |  .  |  .  |  .  |
           1 +-----+-----+-----+
                   |  .  |
           0       X-----+
                   |  .  |
          -1       +-----+
                   |  .  |
          -2       +-----+
            -1     0     1     2

    To replace asserts within this library with your own implementation, simply #define ogt_assert before defining your implementation
    eg.
        #include "my_assert.h"
        #define ogt_assert(condition, message_str)    my_assert(condition, message_str)
        #define ogt_assert_warn(condition, message_str)      my_assert(condition, message_str)

        #define OGT_VOX_IMPLEMENTATION
        #include "path/to/ogt_vox.h"

    ogt_vox is little-endian by default, but it does support big-endian if OGT_VOX_BIGENDIAN_SWAP32(x) is #defined
    to a function that can swap byte order within a uint32_t word before the implementation. eg.

        #define OGT_VOX_BIGENDIAN_SWAP32(x)  __builtin_swap32(x)  // linux/gcc
        #define OGT_VOX_IMPLEMENTATION
        #include "path/to/ogt_vox.h"
*/
#ifndef OGT_VOX_H__
#define OGT_VOX_H__

#if _MSC_VER == 1400
    // VS2005 doesn't have inttypes or stdint so we just define what we need here.
    typedef unsigned char uint8_t;
    typedef signed int    int32_t;
    typedef unsigned int  uint32_t;
    #ifndef UINT32_MAX
        #define UINT32_MAX	((uint32_t)0xFFFFFFFF)
    #endif
    #ifndef INT32_MAX
        #define INT32_MAX	((int32_t)0x7FFFFFFF)
    #endif
    #ifndef UINT8_MAX
        #define UINT8_MAX	((uint8_t)0xFF)
    #endif
#elif defined(_MSC_VER)
    // general VS*
    #include <inttypes.h>
#elif __APPLE__
    // general Apple compiler
    #include <stdint.h>
    #include <limits.h>
    #include <stdlib.h> // for size_t
#elif defined(__GNUC__)
    // any GCC*
    #include <inttypes.h>
    #include <stdlib.h> // for size_t
#else
    #error some fixup needed for this platform?
#endif

#ifdef OGT_VOX_BIGENDIAN_SWAP32
    // host is big-endian, so we byte-swap
    #define _vox_htole32(x)  OGT_VOX_BIGENDIAN_SWAP32((x))
    #define _vox_le32toh(x)  OGT_VOX_BIGENDIAN_SWAP32((x))
#else
    // host is little-endian (default)
    #define _vox_htole32(x)  (x)
    #define _vox_le32toh(x)  (x)
#endif

    // denotes an invalid group index. Usually this is only applicable to the scene's root group's parent.
    static const uint32_t k_invalid_group_index = UINT32_MAX;
    // denotes an invalid layer index. Can happen for instances and groups at least.
    static const uint32_t k_invalid_layer_index = UINT32_MAX;

    // color
    typedef struct ogt_vox_rgba
    {
        uint8_t r,g,b,a;            // red, green, blue and alpha components of a color.
    } ogt_vox_rgba;

    // column-major 4x4 matrix
    typedef struct ogt_vox_transform
    {
        float m00, m01, m02, m03;   // column 0 of 4x4 matrix, 1st three elements = x axis vector, last element always 0.0
        float m10, m11, m12, m13;   // column 1 of 4x4 matrix, 1st three elements = y axis vector, last element always 0.0
        float m20, m21, m22, m23;   // column 2 of 4x4 matrix, 1st three elements = z axis vector, last element always 0.0
        float m30, m31, m32, m33;   // column 3 of 4x4 matrix. 1st three elements = translation vector, last element always 1.0
    } ogt_vox_transform;

    ogt_vox_transform ogt_vox_transform_get_identity();
    ogt_vox_transform ogt_vox_transform_multiply(const ogt_vox_transform & a, const ogt_vox_transform & b);

    // a palette of colors
    typedef struct ogt_vox_palette
    {
        ogt_vox_rgba color[256];      // palette of colors. use the voxel indices to lookup color from the palette.
    } ogt_vox_palette;

    // Extended Material Chunk MATL types
    enum ogt_matl_type
    {
        ogt_matl_type_diffuse = 0, // diffuse is default
        ogt_matl_type_metal   = 1,
        ogt_matl_type_glass   = 2,
        ogt_matl_type_emit    = 3,
        ogt_matl_type_blend   = 4,
        ogt_matl_type_media   = 5,
    };

    enum ogt_cam_mode
    {
        ogt_cam_mode_perspective  = 0,
        ogt_cam_mode_free         = 1,
        ogt_cam_mode_pano         = 2,
        ogt_cam_mode_orthographic = 3,
        ogt_cam_mode_isometric    = 4,
        ogt_cam_mode_unknown      = 5
    };

    // Content Flags for ogt_vox_matl values for a given material
    static const uint32_t k_ogt_vox_matl_have_metal  = 1 << 0;
    static const uint32_t k_ogt_vox_matl_have_rough  = 1 << 1;
    static const uint32_t k_ogt_vox_matl_have_spec   = 1 << 2;
    static const uint32_t k_ogt_vox_matl_have_ior    = 1 << 3;
    static const uint32_t k_ogt_vox_matl_have_att    = 1 << 4;
    static const uint32_t k_ogt_vox_matl_have_flux   = 1 << 5;
    static const uint32_t k_ogt_vox_matl_have_emit   = 1 << 6;
    static const uint32_t k_ogt_vox_matl_have_ldr    = 1 << 7;
    static const uint32_t k_ogt_vox_matl_have_trans  = 1 << 8;
    static const uint32_t k_ogt_vox_matl_have_alpha  = 1 << 9;
    static const uint32_t k_ogt_vox_matl_have_d      = 1 << 10;
    static const uint32_t k_ogt_vox_matl_have_sp     = 1 << 11;
    static const uint32_t k_ogt_vox_matl_have_g      = 1 << 12;
    static const uint32_t k_ogt_vox_matl_have_media  = 1 << 13;

    // media type for blend, glass and cloud materials
    enum ogt_media_type {
        ogt_media_type_absorb,  // Absorb media
        ogt_media_type_scatter, // Scatter media
        ogt_media_type_emit,    // Emissive media
        ogt_media_type_sss,     // Subsurface scattering media
    };

    // Extended Material Chunk MATL information
    typedef struct ogt_vox_matl
    {
        uint32_t       content_flags; // set of k_ogt_vox_matl_* OR together to denote contents available
        ogt_media_type media_type;    // media type for blend, glass and cloud materials
        ogt_matl_type  type;
        float          metal;
        float          rough;         // roughness
        float          spec;          // specular
        float          ior;           // index of refraction
        float          att;           // attenuation
        float          flux;          // radiant flux (power)
        float          emit;          // emissive
        float          ldr;           // low dynamic range
        float          trans;         // transparency
        float          alpha;
        float          d;             // density
        float          sp;
        float          g;
        float          media;
    } ogt_vox_matl;

    // Extended Material Chunk MATL array of materials
    typedef struct ogt_vox_matl_array
    {
        ogt_vox_matl matl[256];      // extended material information from Material Chunk MATL
    } ogt_vox_matl_array;

    typedef struct ogt_vox_cam
    {
        uint32_t     camera_id;
        ogt_cam_mode mode;
        float        focus[3];    // the target position
        float        angle[3];    // rotation in degree - pitch (-180 to +180), yaw (0 to 360), roll (0 to 360)
        float        radius;      // distance of camera position from target position, also controls frustum in MV for orthographic/isometric modes
        float        frustum;     // 'height' of near plane of frustum, either orthographic height in voxels or tan( fov/2.0f )
        int          fov;         // angle in degrees for height of field of view, ensure to set frustum as only used when changed in MV UI
    } ogt_vox_cam;

    typedef struct ogt_vox_sun
    {
        float        intensity;
        float        area;     // 1.0 ~= 43.5 degrees
        float        angle[2]; // elevation, azimuth
        ogt_vox_rgba rgba;
        bool         disk;     // visible sun disk
    } ogt_vox_sun;

    // a 3-dimensional model of voxels
    typedef struct ogt_vox_model
    {
        uint32_t       size_x;        // number of voxels in the local x dimension
        uint32_t       size_y;        // number of voxels in the local y dimension
        uint32_t       size_z;        // number of voxels in the local z dimension
        uint32_t       voxel_hash;    // hash of the content of the grid.
        const uint8_t* voxel_data;    // grid of voxel data comprising color indices in x -> y -> z order. a color index of 0 means empty, all other indices mean solid and can be used to index the scene's palette to obtain the color for the voxel.
    } ogt_vox_model;

    // a keyframe for animation of a transform
    typedef struct ogt_vox_keyframe_transform {
        uint32_t          frame_index;
        ogt_vox_transform transform;
    } ogt_vox_keyframe_transform;

    // a keyframe for animation of a model
    typedef struct ogt_vox_keyframe_model {
        uint32_t frame_index;
        uint32_t model_index;
    } ogt_vox_keyframe_model;

    // an animated transform
    typedef struct ogt_vox_anim_transform {
        const ogt_vox_keyframe_transform* keyframes;
        uint32_t                          num_keyframes;
        bool                              loop;
    } ogt_vox_anim_transform;

    // an animated model
    typedef struct ogt_vox_anim_model {
        const ogt_vox_keyframe_model* keyframes;
        uint32_t                      num_keyframes;
        bool                          loop;
    } ogt_vox_anim_model;

    // an instance of a model within the scene
    typedef struct ogt_vox_instance
    {
        const char*            name;                   // name of the instance if there is one, will be NULL otherwise.
        ogt_vox_transform      transform;              // orientation and position of this instance on first frame of the scene. This is relative to its group local transform if group_index is not 0
        uint32_t               model_index;            // index of the model used by this instance on the first frame of the scene. used to lookup the model in the scene's models[] array.
        uint32_t               layer_index;            // index of the layer used by this instance. used to lookup the layer in the scene's layers[] array.
        uint32_t               group_index;            // this will be the index of the group in the scene's groups[] array. If group is zero it will be the scene root group and the instance transform will be a world-space transform, otherwise the transform is relative to the group.
        bool                   hidden;                 // whether this instance is individually hidden or not. Note: the instance can also be hidden when its layer is hidden, or if it belongs to a group that is hidden.
        ogt_vox_anim_transform transform_anim;         // animation for the transform
        ogt_vox_anim_model     model_anim;             // animation for the model_index
    } ogt_vox_instance;

    // describes a layer within the scene
    typedef struct ogt_vox_layer
    {
        const char*  name;               // name of this layer if there is one, will be NULL otherwise.
        ogt_vox_rgba color;              // color of the layer.
        bool         hidden;             // whether this layer is hidden or not.
    } ogt_vox_layer;

    // describes a group within the scene
    typedef struct ogt_vox_group
    {
        const char*            name;                    // name of the group if there is one, will be NULL otherwise
        ogt_vox_transform      transform;               // transform of this group relative to its parent group (if any), otherwise this will be relative to world-space.
        uint32_t               parent_group_index;      // if this group is parented to another group, this will be the index of its parent in the scene's groups[] array, otherwise this group will be the scene root group and this value will be k_invalid_group_index
        uint32_t               layer_index;             // which layer this group belongs to. used to lookup the layer in the scene's layers[] array.
        bool                   hidden;                  // whether this group is hidden or not.
        ogt_vox_anim_transform transform_anim;          // animated transform data
    } ogt_vox_group;

    // the scene parsed from a .vox file.
    typedef struct ogt_vox_scene
    {
        uint32_t                file_version;     // version of the .vox file format.
        uint32_t                num_models;       // number of models within the scene.
        uint32_t                num_instances;    // number of instances in the scene (on anim frame 0)
        uint32_t                num_layers;       // number of layers in the scene
        uint32_t                num_groups;       // number of groups in the scene
        uint32_t                num_color_names;  // number of color names in the scene
        const char**            color_names;      // array of color names. size is num_color_names
        const ogt_vox_model**   models;           // array of models. size is num_models
        const ogt_vox_instance* instances;        // array of instances. size is num_instances
        const ogt_vox_layer*    layers;           // array of layers. size is num_layers
        const ogt_vox_group*    groups;           // array of groups. size is num_groups
        ogt_vox_palette         palette;          // the palette for this scene
        ogt_vox_matl_array      materials;        // the extended materials for this scene
        uint32_t                num_cameras;      // number of cameras for this scene
        const ogt_vox_cam*      cameras;          // the cameras for this scene
        ogt_vox_sun*            sun;              // sun - primary light at infinity
        uint32_t                anim_range_start; // the start frame of the animation range for this scene (META chunk since 0.99.7.2)
        uint32_t                anim_range_end;   // the end frame of the animation range for this scene (META chunk since 0.99.7.2)
    } ogt_vox_scene;

    // allocate memory function interface. pass in size, and get a pointer to memory with at least that size available.
    typedef void* (*ogt_vox_alloc_func)(size_t size);

    // free memory function interface. pass in a pointer previously allocated and it will be released back to the system managing memory.
    typedef void  (*ogt_vox_free_func)(void* ptr);

    // override the default scene memory allocator if you need to control memory precisely.
    void  ogt_vox_set_memory_allocator(ogt_vox_alloc_func alloc_func, ogt_vox_free_func free_func);
    void* ogt_vox_malloc(size_t size);
    void  ogt_vox_free(void* mem);

    // progress feedback function with option to cancel a ogt_vox_write_scene operation. Percentage complete is approximately given by: 100.0f * progress.
    typedef bool  (*ogt_vox_progress_callback_func)(float progress, void* user_data);

    // set the progress callback function and user data to pass to it
    void  ogt_vox_set_progress_callback_func(ogt_vox_progress_callback_func progress_callback_func, void* user_data);


    // flags for ogt_vox_read_scene_with_flags
    static const uint32_t k_read_scene_flags_groups                      = 1 << 0; // if not specified, all instance transforms will be flattened into world space. If specified, will read group information and keep all transforms as local transform relative to the group they are in.
    static const uint32_t k_read_scene_flags_keyframes                   = 1 << 1; // if specified, all instances and groups will contain keyframe data.
    static const uint32_t k_read_scene_flags_keep_empty_models_instances = 1 << 2; // if specified, all empty models and instances referencing those will be kept rather than culled.
    static const uint32_t k_read_scene_flags_keep_duplicate_models       = 1 << 3; // if specified, we do not de-duplicate models.

    // creates a scene from a vox file within a memory buffer of a given size.
    // you can destroy the input buffer once you have the scene as this function will allocate separate memory for the scene objecvt.
    const ogt_vox_scene* ogt_vox_read_scene(const uint8_t* buffer, uint32_t buffer_size);

    // just like ogt_vox_read_scene, but you can additionally pass a union of k_read_scene_flags
    const ogt_vox_scene* ogt_vox_read_scene_with_flags(const uint8_t* buffer, uint32_t buffer_size, uint32_t read_flags);

    // destroys a scene object to release its memory.
    void ogt_vox_destroy_scene(const ogt_vox_scene* scene);

    // writes the scene to a new buffer and returns the buffer size. free the buffer with ogt_vox_free
    uint8_t* ogt_vox_write_scene(const ogt_vox_scene* scene, uint32_t* buffer_size);

    // convert the camera into a transform
    void ogt_vox_camera_to_transform(const ogt_vox_cam* camera, ogt_vox_transform* transform);

    // merges the specified scenes together to create a bigger scene. Merged scene can be destroyed using ogt_vox_destroy_scene
    // If you require specific colors in the merged scene palette, provide up to and including 255 of them via required_colors/required_color_count.
    ogt_vox_scene* ogt_vox_merge_scenes(const ogt_vox_scene** scenes, uint32_t scene_count, const ogt_vox_rgba* required_colors, const uint32_t required_color_count);

    // sample the model index for a given instance at the given frame
    uint32_t          ogt_vox_sample_instance_model(const ogt_vox_instance* instance, uint32_t frame_index);

    // samples the transform for an instance at a given frame.
    //   ogt_vox_sample_instance_transform_global returns the transform in world space (aka global)
    //   ogt_vox_sample_instance_transform_local returns the transform relative to its parent group
    ogt_vox_transform ogt_vox_sample_instance_transform_global(const ogt_vox_instance* instance, uint32_t frame_index, const ogt_vox_scene* scene);
    ogt_vox_transform ogt_vox_sample_instance_transform_local(const ogt_vox_instance* instance, uint32_t frame_index);

    // sample the transform for a group at a given frame
    //  ogt_vox_sample_group_transform_global returns the transform in world space (aka global)
    //  ogt_vox_sample_group_transform_local returns the transform relative to its parent group
    ogt_vox_transform ogt_vox_sample_group_transform_global(const ogt_vox_group* group, uint32_t frame_index, const ogt_vox_scene* scene);
    ogt_vox_transform ogt_vox_sample_group_transform_local(const ogt_vox_group* group, uint32_t frame_index);

#endif // OGT_VOX_H__

/* -------------------------------------------------------------------------------------------------------------------------------------------------

    MIT License

    Copyright (c) 2019 Justin Paver

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.

------------------------------------------------------------------------------------------------------------------------------------------------- */
