#pragma once

namespace pt
{
    struct RtImage
    {
        VkImage image;
        VkImageView view;
        VkDeviceMemory mem;
    };

    struct AccelerationStructure
    {
        VkAccelerationStructureNV as;   // client-side acceleration structure handle
        VkDeviceMemory memory;          // memory that is bound to the acceleration structure
        uint64_t handle;                // device-side acceleration structure handle
    };

    struct ShaderBindingTable
    {
        vka::Buffer buff;       // SBT buffer
        uint32_t record_stride; // SBT record stride
        uint32_t rgen_offset;   // ray generation record offset
        uint32_t miss_offset;   // miss record offset
        uint32_t hg_offset;     // hit group record(s) offset
    };

    struct Pipeline
    {
        VkPipelineLayout layout;
        VkPipeline pipeline;
    };

    // Used as constant parameter within the SBT record.
    // Per-geometry data is stored there.
    struct RecordParameter
    {
        uint32_t geometryID;    // the ID of the geometry, this ID is unique
        uint32_t materialID;    // the ID of the material the geometry uses
                                // As one geometry equals one mesh, every geometry has one
                                // material ID.
    };

    // Stored within this structure are all properties that are
    // requiered for acceleration structure and geometry building.
    struct MeshProperties
    {
        uint32_t vertex_count;      // number of vertices within the mesh
        VkFormat vertex_format;     // format of vertices = VK_FORMAT_R32G32B32_SFLOAT
        size_t vertex_stride;       // the size of one vertex
        uint32_t index_count;       // number of indices of the mesh
        RecordParameter record;     // Per-mesh/geometry parameter for the SBT record.
    };

    struct RenderMesh
    {
        vka::Buffer vectices;       // vertex position data
        vka::Buffer attributes;     // vertex attributes like normals and texture coordinates
        vka::Buffer indices;        // index buffer
        MeshProperties properties;
    };

    // Stored within this structure are all properties that are
    // requiered to load the materials.
    struct MaterialProperties
    {
        // single material parameters
        glm2::vec3 single_emission;

        // map (texture) material parameters
        std::string map_albedo;
        std::string map_emission;
        std::string map_roughness;
        std::string map_metallic;
        std::string map_alpha;
        std::string map_normal;
    };

    // Stored within this structure are all non-texture material parameters.
    struct RenderMaterialUniform
    {
        float ior;
    };

    // Stored within this structure are all texture material parameters.
    struct RenderMaterialTexture
    {
        // Array layer 0:   albedo map [sRGB]
        //                  8bit image format
        vka::Texture albedo;

        // Array layer 0;   emission map [RGB]
        //                  32bit image format
        //                  (unsed [A], is needed for the format)
        vka::Texture emission;

        // Array layer 0:   roughness image [R]
        //                  metallic image [G]
        //                  alpha image [B]
        //                  (unused [A], is needed for the format)
        // Array layer 1:   normal map [RGB]
        //                  (unused [A], is needed for the format)
        //                  8bit image format
        vka::Texture rman;
    };

    struct RenderMaterial
    {
        RenderMaterialUniform uniform;
        RenderMaterialTexture texture;
        MaterialProperties properties;
    };

    // One model contains multiple meshes, the the scene can contain
    // multiple models. To keep things organized, meshes are stored
    // inside a 2D array.
    using model_array_t = std::vector<std::vector<RenderMesh>>;

    // Materials are NOT stored in a 2D array as every mesh
    // of the model vector contains a 1D material ID that
    // references a valid material in this material array.
    using material_array_t = std::vector<RenderMaterial>;

    class PathTracer
    {
        typedef void(*loadmsg_callback_t)(const std::string&);
    private:
        bool initialized;
        const Setup* setup;
        std::string environment_path;
        std::vector<std::string> model_paths;
        loadmsg_callback_t texture_load_callback;
        loadmsg_callback_t model_load_callback;

        RtImage render_target;      // render target image of the shaders
        RtImage output_image;       // rendered image that is written to a file

        model_array_t models;
        material_array_t materials; // all the texture-materials
        vka::Texture environment;   // environment map (equirectangular map), 32bit image format
        vka::Buffer mtl_buffer;     // all the single-value-materials
        vka::Buffer tlibo;          // buffer object that holds the instances for the tlas

        VkCommandPool cmd_pool;
        VkQueryPool timer_query_pool;
        AccelerationStructure blas, tlas;
        vka::DescriptorManager descriptors;
        vka::ShaderProgram stages;  // All the different shaders that are used
        std::vector<VkRayTracingShaderGroupCreateInfoNV> shader_groups; // shader groups that are used in the SBT

        Pipeline rtp;
        ShaderBindingTable sbt;

        void create_pools(void);
        void create_render_images(void);
        void create_render_models(void);
        void create_render_materials(void);
        void create_acceleration_structure(void);
        void create_shaders(void);
        void create_shader_groups(void);
        void create_descriptors(void);
        void create_pipeline(void);
        void create_sbt(void);

        void load_render_mesh(const vka::Mesh& mesh, RenderMesh& rmesh);
        void load_mtl_buffer(const material_array_t& mtlarray);
        void load_albedo_texture(RenderMaterial& mtl);
        void load_emissive_texture(RenderMaterial& mtl);
        void load_rman_texture(RenderMaterial& mtl);
        void load_environment_texture(void);
        void load_geometry(std::vector<VkGeometryNV>& geometry);
        void load_instances(const AccelerationStructure& blas);
        void load_blas(const std::vector<VkGeometryNV>& geometry, AccelerationStructure& blas);
        void load_tlas(AccelerationStructure& tlas);

        void init_texture(vka::Texture& tex);

        VkCommandBuffer record(void);
        void trace(VkCommandBuffer cbo);

        static inline glm2::mat4 identity_transform(void)
        {
            return glm2::mat4(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            );
        }

        static inline VkTransformMatrixNV glm2transformNV(const glm2::mat4& mat)
        {
            VkTransformMatrixNV ret;
            const glm2::mat3x4 r43 = glm2::transpose((glm2::mat4x3)mat);
            r43.store(ret.matrix[0]);
            return ret;
        }

    public:
        PathTracer(void);
        virtual ~PathTracer(void)
        { this->destroy(); }

        PathTracer(const PathTracer&) = delete;
        PathTracer& operator= (const PathTracer&) = delete;

        PathTracer(PathTracer&&) = delete;
        PathTracer& operator= (PathTracer&&) = delete;

        void set_model_load_callback(loadmsg_callback_t callback);
        void set_texture_load_callback(loadmsg_callback_t callback);

        /**
         * @brief initializes the path tracer
         */
        void init(const Setup* setup);

        /**
         * @brief destroys all internal objects of the path tracer
         */
        void destroy(void);
        
        /**
         * @brief       Loads an environment map.
         * @param path  Path to the environment image.
         * @note        The environment map is requiered for this application.
         *              If you decide not to load one, the application won't start.
         *              Additionally, only one environment map will be loaded.
         *              If 'load_environment' is called multiple times, only the 
         *              path to the file will be overwritten.
         *              The environment map must be an equirectangular map in the HDR image format.
         */
        void load_environment(const std::string& path);

        /**
         * @brief       Loads a model from an object file.
         * @param path  Path to the object file.
         */
        void load_model(const std::string& path);

        /**
         * @brief   Extecutes the path tracer.
         * @return  Execution time in nano seconds.
         */
        uint64_t run(void);

        const uint8_t* map_image(size_t& row_stride, uint32_t& component_count) const;

        void unmap_image(void) const noexcept;
    };
} // namespace pt

