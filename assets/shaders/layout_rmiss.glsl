/* layout spiecifiers for the miss shader */

// location (set = 1, binding = 7) contains the environment map
// !!! The environment map is an HDR image. 32-Bit-Format !!!
layout (set = 1, binding = 7) uniform sampler2D environment;

// The closest hit and the miss shader can return some values via the payload.
// This payload uses the location 0.
layout (location = 0) rayPayloadInNV payload_t payload;
