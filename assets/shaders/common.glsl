
/**
* @brief Transformes a ray direction to any orientation.
* @param vd View direction.
* @param up Up vector.
* @param rd Ray direction to transform.
*/
vec3 combute_direction(in vec3 vd, in vec3 up, in vec3 rd)
{
    mat3 transform;
    transform[2] = vd;
    transform[0] = normalize(cross(up, vd));
    transform[1] = cross(transform[0], vd);

    return transform * rd;
}
