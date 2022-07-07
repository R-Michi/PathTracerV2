/* common functions used inside the miss shader */

/**
* @brief            Samples an equirectangular environment map.
* @param direction  direction to sample from
* @return           Color of the environment map at the specified position.
*/
const vec2 inv_atan = vec2(0.1591f, 0.3183f);
vec3 sample_environment(in vec3 direction)
{
    vec2 uv = vec2(atan(direction.z, direction.x), asin(direction.y));
    uv *= inv_atan;
    uv += 0.5f;
    return texture(environment, uv).xyz;
}
