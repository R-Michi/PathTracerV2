#version 460 core
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#include "types.glsl"
#include "layout_rchit.glsl"
#include "common_rchit.glsl"

void main()
{
    payload.color = vec3(0.2f, 0.2f, 0.2f);
}
