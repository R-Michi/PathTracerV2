/* types used for the shaders */

struct payload_t
{
    vec3 color;
};

struct ray_t
{
    vec3 origin;
    vec3 direction;
};

struct attribute_t
{
    vec3 normal;
    vec2 texcoord;
};

struct material_t
{
    float ior;
};
