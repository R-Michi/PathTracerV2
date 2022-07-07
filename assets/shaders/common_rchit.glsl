/* common functions used inside the closest hit shader */

/**
* @brief    Returns the interpolated attributes of the hit primitive.
* @return   Interpolated attributes of the hit primitive.
*/
attribute_t get_attribute(void)
{
    // Combute the barycentric coordinates from the barycentric weights
    // in the hitAttributeNV. The barycentric weights correspond to the UV
    // coordinates of the Triangle.
    // Following formulas are used to calculate the barycentric coordinates B(Bx / By / Bz):
    //  Bx = 1 - u - v
    //  By = u
    //  Bz = v
    // Those barycentric coordinates are the weights of the sides of a triangle.
    // Therefore, we can interpolate the 3 vertices (v0, v1, v2) that formed the triangle:
    //  f(B, v0, v1, v2) = v0 * Bx + v1 * By + v2 * Bz
    const vec3 barycentric = vec3(1.0f - hit_info.x - hit_info.y, hit_info.x, hit_info.y);

    // get the indices of the hit triangle
    // 3 indices make up one triangle.
    const uint i0 = ibo[record.geometryID].index[gl_PrimitiveID * 3 + 0];
    const uint i1 = ibo[record.geometryID].index[gl_PrimitiveID * 3 + 1];
    const uint i2 = ibo[record.geometryID].index[gl_PrimitiveID * 3 + 2];

    // Now the attributes can be read with the indices.
    const attribute_t a0 = abo[record.geometryID].attrib[i0];
    const attribute_t a1 = abo[record.geometryID].attrib[i1];
    const attribute_t a2 = abo[record.geometryID].attrib[i2];

    // After reading the attributes, they can be interpolated using the
    // barycentric coordinates and the function f(B, v0, v1, v2) mentioned above.
    attribute_t a;
    a.normal    = a0.normal * barycentric.x + a1.normal * barycentric.y + a2.normal * barycentric.z;
    a.texcoord  = a0.texcoord * barycentric.x + a1.texcoord * barycentric.y + a2.texcoord * barycentric.z;
    return a;
}

/**
* @brief    Returns the uniform material of the hit primitive.
* @return   Material of the hit primitive.
*/
material_t get_material(void)
{
    return ubo.material[record.materialID];
}

/**
* @brief    Returnes the albedo value of the hit primtive at the U,V coordinates.
* @param    U,V coordinates
* @return   Albedo value of the hit primitive at the U,V coordinates.
*/
vec3 get_albedo(in vec2 uv)
{
    return texture(map_albedo[record.materialID], uv).xyz;
}

/**
* @brief    Returnes the emission value of the hit primtive at the U,V coordinates.
* @param    U,V coordinates
* @return   Emission value of the hit primitive at the U,V coordinates.
*/
vec3 get_emission(in vec2 uv)
{
    return texture(map_emissive[record.materialID], uv).xyz;
}

/**
* @brief    Returnes the roughness, matallic and alpha value of the hit primtive at the U,V coordinates.
* @param    U,V coordinates
* @return   Roughness, matallic and alpha value of the hit primitive at the U,V coordinates.
*/
vec3 get_rma(in vec2 uv)
{
    return texture(map_rma[record.materialID], uv).xyz;
}

/**
* @brief    Returnes the normal map value of the hit primtive at the U,V coordinates
*           and automatically converts the normal vector to a range of [-1, 1].
* @param    U,V coordinates
* @return   Normal map value of the hit primitive at the U,V coordinates.
*/
vec3 get_normal(in vec2 uv)
{
    return normalize(texture(map_normal[record.materialID], uv).xyz * 2.0f - 1.0f);
}
