#version 460 core
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "types.glsl"
#include "layout_rmiss.glsl"
#include "common_rmiss.glsl"

void main()
{
    payload.color = vec3(0.1f, 0.1f, 0.1f);
}
