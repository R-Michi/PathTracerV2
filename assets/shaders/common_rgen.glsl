/* common functions used inside the ray generation shader */

/**
* @brief    Combutes the Normalized-Device-Coordinates via the gl_LaunchIDNV,
*           which holds the current pixel position of out image,
*           and gl_LaunchSizeNV, which holds the image size to render.
* @return   Normalized-Device-Coordinates of the current pixel postion.
*           The left upper corner has the coordinates (-1, -1) and
*           the right lower corner has the coordinates (+1, +1).
*/
vec2 get_NDC(void)
{
    const float aspect = float(gl_LaunchSizeNV.x) / float(gl_LaunchSizeNV.y);
    vec2 ndc = vec2(gl_LaunchIDNV) / vec2(gl_LaunchSizeNV);
    ndc = ndc * 2.0f - 1.0f;
    ndc.x *= aspect;
    return ndc;
}

/**
* @brief    Transforms a camera ray to any view direction.
* @param vd View direction.
* @param up Up vector.
* @param rd Ray direction to transform.
* @return   The camera ray transformed to the view direction
*/
vec3 combute_direction(in vec3 vd, in vec3 up, in vec3 rd)
{
    mat3 transform;
    transform[2] = vd;
    transform[0] = normalize(cross(up, vd));
    transform[1] = cross(transform[0], vd);

    return transform * rd;
}
