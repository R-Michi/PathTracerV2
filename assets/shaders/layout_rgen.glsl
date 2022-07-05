/* layout spiecifiers for the ray generation shader */

// location (set = 0, binding = 0) contains the output image of the path tracer
// rgba8 stands for 8R 8G 8B 8A bits
// for more details see https://www.khronos.org/opengl/wiki/Image_Load_Store 
// and https://www.khronos.org/registry/OpenGL-Refpages/es3.0/html/glTexImage2D.xhtml
layout (set = 0, binding = 0, rgba8) uniform image2D rop;

// location (set = 0, binding = 1) contains the top level acceleration structure
layout (set = 0, binding = 1) uniform accelerationStructureNV tlas;
