
struct payload_t
{
    vec3 color;
};

struct vertex_t
{
    float[3] pos;
    float[2] texcoord;
    float[3] normal;
};

struct ray_t
{
    vec3 origin;
    vec3 direction;
};
