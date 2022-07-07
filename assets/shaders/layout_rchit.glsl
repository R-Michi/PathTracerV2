/* layout spiecifiers for the closest hit shader */

// location (set = 1, binding = 0) contains all vertex attribute buffer descriptors.
// There are as many vertex attribute buffer descriptors as meshes loaded into the scene.
// NOTE: This array of descriptors has no fixed size, therefore the extension
// GL_EXT_nonuniform_qualifier is requiered.
layout (set = 1, binding = 0) buffer AttributeBuffer
{
    attribute_t attrib[];
} abo[];

// location (set = 1, binding = 1) contains all index buffer descriptors.
// There are as many index buffer descriptors as meshes loaded into the scene.
// NOTE: This array of descriptors has no fixed size, therefore the extension
// GL_EXT_nonuniform_qualifier is requiered.
layout (set = 1, binding = 1) buffer IndexBuffer
{
    uint index[];
} ibo[];

// location (set = 1, binding = 2) contains the buffer of all uniform materials
layout (set = 1, binding = 2) buffer MaterialBuffer
{
    material_t material[];
} ubo;

// location (set = 1, binding = 3) contains all albedo texture samplers
// There are as many albedo texture sampler descriptors as materials loaded into the scene.
// !!! Albedo textures are in the sRGB color space. 8-Bit-Format !!!
// NOTE: This array of descriptors has no fixed size, therefore the extension
// GL_EXT_nonuniform_qualifier is requiered.
layout (set = 1, binding = 3) uniform sampler2D map_albedo[];

// location (set = 1, binding = 4) contains all emissive texture samplers
// There are as many emissive texture sampler descriptors as materials loaded into the scene.
// !!! Emissive maps are HDR images. 32-Bit-Format !!!
// NOTE: This array of descriptors has no fixed size, therefore the extension
// GL_EXT_nonuniform_qualifier is requiered.
layout (set = 1, binding = 4) uniform sampler2D map_emissive[];

// location (set = 1, binding = 5) contains all RMA texture samplers
// RMA stands for: "Roughness, Metallic and Alpha".
// - The roughness map is stored inside the R component.
// - The metallic map is stored inside the G component.
// - The alpha map is stored inside the B component.
// There are as many RMA texture sampler descriptors as materials loaded into the scene.
// !!! RMA maps are RGB images. 8-Bit-Format !!!
// NOTE: This array of descriptors has no fixed size, therefore the extension
// GL_EXT_nonuniform_qualifier is requiered.
layout (set = 1, binding = 5) uniform sampler2D map_rma[];

// location (set = 1, binding = 6) contains all normal map texture samplers
// There are as many normal map texture sampler descriptors as materials loaded into the scene.
// !!! Normal maps are RGB images. 8-Bit-Format !!!
// NOTE: This array of descriptors has no fixed size, therefore the extension
// GL_EXT_nonuniform_qualifier is requiered.
layout (set = 1, binding = 6) uniform sampler2D map_normal[];

// SBT record parameter
layout (shaderRecordNV) buffer Record
{
    uint geometryID;
    uint materialID;
} record;

// The closest hit and the miss shader can return some values via the payload.
// This payload uses the location 0.
layout (location = 0) rayPayloadInNV payload_t payload;

// Barycentric weights of the triangle intersection
hitAttributeNV vec2 hit_info;
