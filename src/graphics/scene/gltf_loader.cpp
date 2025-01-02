#include "graphics/scene/gltf_loader.h"

// std
#include <iostream>
#include <fstream>
// external
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
// lincore
#include "graphics/scene/scene_types.h"
#include "foundation/resources.h"
#include "foundation/logging.h"
#include "graphics/vk_device.h"
#include "graphics/scene/scene_node.h"
// Add this with other GLM includes
#include <glm/gtc/type_ptr.hpp> // for glm::make_mat4

namespace lincore::scene
{

    //------------------------ LoadedGLTF 实现 ------------------------//
    void LoadedGLTF::Draw(const glm::mat4 &transform, DrawContext &context)
    {
        for (auto &node : root_nodes_)
        {
            node->Draw(transform, context);
        }
    }

    Bounds LoadedGLTF::GetBounds() const
    {
        Bounds result;
        for (const auto &node : root_nodes_)
        {
            result.Merge(node->GetBounds());
        }
        return result;
    }

    void LoadedGLTF::Init(GpuDevice *gpu_device)
    {
        gpu_device_ = gpu_device;
    }

    void LoadedGLTF::SetName(const std::string &name)
    {
        name_ = name;
    }

    void LoadedGLTF::AddMesh(const std::string &name, std::shared_ptr<MeshAsset> mesh)
    {
        if (mesh)
        {
            meshes_[name] = mesh;
        }
    }

    void LoadedGLTF::AddMaterial(const std::string &name, std::shared_ptr<MaterialInstance> material)
    {
        if (material)
        {
            materials_[name] = material;
        }
    }

    void LoadedGLTF::AddNode(std::shared_ptr<SceneNode> node)
    {
        if (node)
        {
            nodes_.push_back(node);
            if (!node->GetParent())
            {
                root_nodes_.push_back(node);
                node->RefreshTransform(glm::mat4{ 1.f });
            }
        }
    }

    std::shared_ptr<MeshAsset> LoadedGLTF::FindMesh(const std::string &name) const
    {
        auto it = meshes_.find(name);
        return (it != meshes_.end()) ? it->second : nullptr;
    }

    std::shared_ptr<MaterialInstance> LoadedGLTF::FindMaterial(const std::string &name) const
    {
        auto it = materials_.find(name);
        return (it != materials_.end()) ? it->second : nullptr;
    }

    std::shared_ptr<SceneNode> LoadedGLTF::FindNode(const std::string &name) const
    {
        for (const auto &node : nodes_)
        {
            if (node->GetName() == name)
            {
                return node;
            }
            // 递归搜索子节点
            for (const auto &child : node->GetChildren())
            {
                if (child->GetName() == name)
                {
                    return child;
                }
            }
        }
        return nullptr;
    }

    void LoadedGLTF::ClearAll()
    {
        meshes_.clear();
        materials_.clear();
        nodes_.clear();
        root_nodes_.clear();
    }

    void LoadedGLTF::ReleaseCPUData()
    {
        for (auto &[name, mesh] : meshes_)
        {
            mesh->ReleaseCPUData();
        }
    }

    LoadedGLTF::Statistics LoadedGLTF::GetStatistics() const
    {
        Statistics stats;

        stats.mesh_count = meshes_.size();
        stats.material_count = materials_.size();
        stats.node_count = nodes_.size();

        for (const auto &[name, mesh] : meshes_)
        {
            stats.vertex_count += mesh->mesh_data.vertices.size();
            stats.triangle_count += mesh->mesh_data.indices.size() / 3;

            // 估算GPU内存使用
            size_t vertex_size = sizeof(Vertex);
            stats.gpu_memory_usage += vertex_size * mesh->mesh_data.vertices.size();
            stats.gpu_memory_usage += sizeof(uint32_t) * mesh->mesh_data.indices.size();
        }

        return stats;
    }

    std::shared_ptr<LoadedGLTF> GLTFLoader::LoadGLTF(GpuDevice *device, const std::string &path, const LoadConfig &config)
    {
		LOGI("Loading GLTF file: {}", path);
        // 创建解析器
        fastgltf::Parser parser;
        auto data = fastgltf::GltfDataBuffer::FromPath(std::filesystem::path(path));
        if (auto error = data.error(); error != fastgltf::Error::None)
        {
            LOGE("Failed to load GLTF file: {}, error: {}", path, fastgltf::getErrorMessage(error));
            return nullptr;
        }

        // 解析GLTF
        constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember |
                                      fastgltf::Options::LoadExternalBuffers;

        fastgltf::Asset gltf;
        auto type = fastgltf::determineGltfFileType(data.get());

        if (type == fastgltf::GltfType::glTF)
        {
            auto load = parser.loadGltf(data.get(), std::filesystem::path(path).parent_path().string(), gltf_options);
            if (load)
            {
                gltf = std::move(load.get());
            }
            else
            {
                LOGE("Failed to load gltf: {}", fastgltf::to_underlying(load.error()));
                return nullptr;
            }
        }
        else if (type == fastgltf::GltfType::GLB)
        {
            auto load = parser.loadGltfBinary(data.get(), std::filesystem::path(path).parent_path().string(), gltf_options);
            if (load)
            {
                gltf = std::move(load.get());
            }
            else
            {
                LOGE("Failed to load glb: {}", fastgltf::to_underlying(load.error()));
                return nullptr;
            }
        }
        else
        {
            LOGE("Failed to determine glTF container type");
            return nullptr;
        }

        // 创建加载上下文
        LoadContext ctx(device, config, gltf, std::filesystem::path(path).parent_path());

        // 分步加载
        if (!LoadBuffers(ctx))
        {
            LOGW("Failed to load buffers");
        }
        if (!LoadMaterials(ctx))
        {
            LOGW("Failed to load materials");
        }
        if (!LoadMeshes(ctx))
        {
            LOGW("Failed to load meshes");
        }
        if (!LoadNodes(ctx))
        {
            LOGW("Failed to load nodes");
        }

		LOGI("GLTF file loaded: {}", path);
        return ctx.output;
    }

    bool GLTFLoader::LoadBuffers(LoadContext &ctx)
    {
        for (auto &buffer : ctx.asset.buffers)
        {
            std::vector<uint8_t> data;

            std::visit(
                fastgltf::visitor{
                    [](auto &arg) {},
                    [&](const fastgltf::sources::URI &uri)
                    {
                        if (!uri.uri.isLocalPath())
                        {
                            LOGE("Only local file paths are supported");
                            return;
                        }
                        if (uri.fileByteOffset != 0)
                        {
                            LOGE("URI byte offset is not supported");
                            return;
                        }
                        const std::string path = std::string(uri.uri.path().begin(), uri.uri.path().end());
                        std::ifstream file(path, std::ios::binary);
                        if (!file)
                        {
                            LOGE("Failed to open buffer file: {}", path);
                            return;
                        }
                        data.resize(buffer.byteLength);
                        file.read(reinterpret_cast<char *>(data.data()), buffer.byteLength);
                    },
                    [&](const fastgltf::sources::Vector &vector)
                    {
                        data.resize(vector.bytes.size());
                        std::transform(vector.bytes.begin(), vector.bytes.end(), data.begin(),
                                       [](const std::byte &b)
                                       { return static_cast<uint8_t>(b); });
                    },
                    [&](const fastgltf::sources::BufferView &view)
                    {
                        const auto &bufferView = ctx.asset.bufferViews[view.bufferViewIndex];
                        const auto &buffer = ctx.asset.buffers[bufferView.bufferIndex];

                        data.resize(bufferView.byteLength);

                        // 从源缓冲区复制数据
                        std::visit(
                            fastgltf::visitor{
                                [](auto &arg) {}, // 默认处理器
                                [&](const fastgltf::sources::Vector &vector)
                                {
                                    std::transform(
                                        vector.bytes.begin() + bufferView.byteOffset,
                                        vector.bytes.begin() + bufferView.byteOffset + bufferView.byteLength,
                                        data.begin(),
                                        [](const std::byte &b)
                                        { return static_cast<uint8_t>(b); });
                                },
                                [&](const fastgltf::sources::Array &array)
                                {
                                    std::transform(
                                        array.bytes.begin() + bufferView.byteOffset,
                                        array.bytes.begin() + bufferView.byteOffset + bufferView.byteLength,
                                        data.begin(),
                                        [](const std::byte &b)
                                        { return static_cast<uint8_t>(b); });
                                }},
                            buffer.data);
                    },
                    [&](const fastgltf::sources::Array &array)
                    {
                        data.resize(array.bytes.size());
                        std::transform(array.bytes.begin(), array.bytes.end(), data.begin(),
                                       [](const std::byte &b)
                                       { return static_cast<uint8_t>(b); });
                    }},
                buffer.data);

            if (data.empty())
            {
                return false;
            }

            ctx.output->buffer_data_.push_back(std::move(data));
        }

        return true;
    }

    bool GLTFLoader::LoadMaterials(LoadContext &ctx)
    {
        ctx.material_cache.resize(ctx.asset.materials.size());

        BufferCreation buffer_info{};
        buffer_info.Reset()
            .Set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 ResourceUsageType::Immutable,
                 static_cast<uint32_t>(sizeof(MaterialInstance::Parameters) * ctx.asset.materials.size()))
            .SetPersistent();
        buffer_info.initial_data = nullptr;
        ctx.material_data_buffer_handle = ctx.device->CreateResource(buffer_info);

        Buffer *material_data_buffer = ctx.device->GetResource<Buffer>(ctx.material_data_buffer_handle.index);
        MaterialInstance::Parameters *material_data = static_cast<MaterialInstance::Parameters *>(material_data_buffer->mapped_data);
        // 遍历材质
        for (size_t i = 0; i < ctx.asset.materials.size(); i++)
        {
            const auto &gltf_mat = ctx.asset.materials[i];
            auto material = std::make_shared<MaterialInstance>();
            std::string material_name = gltf_mat.name.empty() ? "material_" + std::to_string(i) : std::string(gltf_mat.name.c_str());
            // 基础颜色
            material->parameters.base_color_factor = glm::vec4(
                gltf_mat.pbrData.baseColorFactor[0],
                gltf_mat.pbrData.baseColorFactor[1],
                gltf_mat.pbrData.baseColorFactor[2],
                gltf_mat.pbrData.baseColorFactor[3]);

            // 金属度和粗糙度
            material->parameters.metallic_factor = gltf_mat.pbrData.metallicFactor;
            material->parameters.roughness_factor = gltf_mat.pbrData.roughnessFactor;

            // 法线贴图缩放
            if (gltf_mat.normalTexture)
            {
                material->parameters.normal_scale = gltf_mat.normalTexture->scale;
            }

            // 自发光因子
            material->parameters.emissive_factor = glm::vec3(
                gltf_mat.emissiveFactor[0],
                gltf_mat.emissiveFactor[1],
                gltf_mat.emissiveFactor[2]);

            // Alpha裁剪
            material->parameters.alpha_cutoff = gltf_mat.alphaCutoff;

            // 材质标志, 默认不透明
            uint32_t flags = 0;
            // 双面
            if (gltf_mat.doubleSided)
            {
                flags |= MATERIAL_FLAG_DOUBLE_SIDED;
            }
            // 透明
            if (gltf_mat.alphaMode == fastgltf::AlphaMode::Blend)
            {
                flags |= MATERIAL_FLAG_BLEND;
            }
            // 裁剪
            else if (gltf_mat.alphaMode == fastgltf::AlphaMode::Mask)
            {
                flags |= MATERIAL_FLAG_ALPHA_TEST;
            }
            material->parameters.material_flags = flags;

            TextureHandle &dummy_texture_handle = ctx.device->default_resources_.images.error_checker_board_image;
            SamplerHandle &dummy_sampler = ctx.device->default_resources_.samplers.linear;

            material->texture_indices.base_color = ctx.device->AddBindlessSampledImage(dummy_texture_handle, dummy_sampler);
            material->texture_indices.metallic_roughness = ctx.device->AddBindlessSampledImage(dummy_texture_handle, dummy_sampler);
            material->texture_indices.normal = ctx.device->AddBindlessSampledImage(dummy_texture_handle, dummy_sampler);
            material->texture_indices.emissive = ctx.device->AddBindlessSampledImage(dummy_texture_handle, dummy_sampler);

            // 加载贴图
            if (gltf_mat.pbrData.baseColorTexture)
            {
                material->texture_indices.base_color = LoadTexture(ctx, gltf_mat.pbrData.baseColorTexture->textureIndex);
            }

            if (gltf_mat.pbrData.metallicRoughnessTexture)
            {
                material->texture_indices.metallic_roughness = LoadTexture(ctx, gltf_mat.pbrData.metallicRoughnessTexture->textureIndex);
            }

            if (gltf_mat.normalTexture)
            {
                material->texture_indices.normal = LoadTexture(ctx, gltf_mat.normalTexture->textureIndex);
            }

            if (gltf_mat.emissiveTexture)
            {
                material->texture_indices.emissive = LoadTexture(ctx, gltf_mat.emissiveTexture->textureIndex);
            }

            ctx.material_cache[i] = material;
            ctx.output->AddMaterial(material_name, material);
            material_data[i] = material->parameters;
        }

        return true;
    }

    uint32_t GLTFLoader::LoadTexture(LoadContext &ctx, size_t texture_index)
    {
        // 检查缓存
        if (ctx.texture_cache[texture_index] != UINT32_MAX)
        {
            return ctx.texture_cache[texture_index];
        }

        const auto &texture = ctx.asset.textures[texture_index];
        if (!texture.imageIndex.has_value())
        {
            LOGE("Texture {} has no image index", texture_index);
            return UINT32_MAX;
        }

        const auto &image = ctx.asset.images[texture.imageIndex.value()];
        TextureHandle texture_handle;
        if (!LoadTextureFromImage(ctx, image, texture_handle))
        {
            return k_invalid_texture.index;
        }
        // 创建或获取采样器
        SamplerHandle sampler = ctx.device->default_resources_.samplers.linear;
        if (texture.samplerIndex.has_value())
        {
            sampler = CreateSampler(ctx, ctx.asset.samplers[texture.samplerIndex.value()]);
        }
        // 添加到bindless更新队列
        ctx.device->AddBindlessSampledImage(texture_handle, sampler);
        return static_cast<uint32_t>(ctx.device->bindless_updates.updates.size() - 1);
    }

    bool GLTFLoader::LoadTextureFromImage(LoadContext &ctx, const fastgltf::Image &image, TextureHandle &out_texture_handle)
    {
        int width = 0, height = 0, channels = 0;
        stbi_uc *pixels = nullptr;
        bool success = false;

        std::visit(
            fastgltf::visitor{
                [](auto &arg) {}, // 默认处理器
                [&](const fastgltf::sources::URI &uri)
                {
                    // 检查是否为本地文件路径
                    if (!uri.uri.isLocalPath())
                    {
                        LOGE("Only local file paths are supported");
                        return;
                    }
                    if (uri.fileByteOffset != 0)
                    {
                        LOGE("URI byte offset is not supported");
                        return;
                    }

                    const std::string path = (ctx.base_path / std::string(uri.uri.path().begin(), uri.uri.path().end())).string();
                    pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
                    if (pixels)
                    {
                        success = true;
                    }
                    else
                    {
                        LOGE("Failed to load image from file: {}", path);
                    }
                },
                [&](const fastgltf::sources::Vector &vector)
                {
                    pixels = stbi_load_from_memory(
                        reinterpret_cast<const stbi_uc *>(vector.bytes.data()),
                        static_cast<int>(vector.bytes.size()),
                        &width, &height, &channels, STBI_rgb_alpha);
                    if (pixels)
                    {
                        success = true;
                    }
                    else
                    {
                        LOGE("Failed to load image from memory vector");
                    }
                },
                [&](const fastgltf::sources::BufferView &view)
                {
                    const auto &bufferView = ctx.asset.bufferViews[view.bufferViewIndex];
                    const auto &buffer = ctx.asset.buffers[bufferView.bufferIndex];

                    std::visit(
                        fastgltf::visitor{
                            [](auto &arg) {}, // 默认处理器
                            [&](const fastgltf::sources::Array &array)
                            {
                                pixels = stbi_load_from_memory(
                                    reinterpret_cast<const stbi_uc *>(array.bytes.data() + bufferView.byteOffset),
                                    static_cast<int>(bufferView.byteLength),
                                    &width, &height, &channels, STBI_rgb_alpha);
                                if (pixels)
                                {
                                    success = true;
                                }
                                else
                                {
                                    LOGE("Failed to load image from buffer view");
                                }
                            }},
                        buffer.data);
                }},
            image.data);

        if (!success || !pixels)
        {
            LOGE("Failed to load image: {}", image.name);
            return false;
        }

        // 创建纹理
        TextureCreation creation;
        creation.SetSize(width, height, 1)
            .SetFormat(VK_FORMAT_R8G8B8A8_UNORM)
            .SetFlags(TextureFlags::Default)
            .SetData(pixels)
            .SetName(image.name.c_str());

        TextureHandle texture = ctx.device->CreateResource(creation);
        if (!texture.IsValid())
        {
            LOGE("Failed to create texture from image: {}", image.name);
            stbi_image_free(pixels);
            return false;
        }

        out_texture_handle = texture;
        stbi_image_free(pixels);
        return true;
    }

    VkFilter GLTFLoader::ExtractFilter(std::optional<fastgltf::Filter> filter)
    {
        if (!filter.has_value())
        {
            return VK_FILTER_LINEAR; // 默认使用线性过滤
        }

        switch (filter.value())
        {
        // nearest samplers
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return VK_FILTER_NEAREST;

        // linear samplers
        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_FILTER_LINEAR;
        }
    }

    VkSamplerMipmapMode GLTFLoader::ExtractMipmapMode(std::optional<fastgltf::Filter> filter)
    {
        if (!filter.has_value())
        {
            return VK_SAMPLER_MIPMAP_MODE_LINEAR; // 默认使用线性Mipmap
        }

        switch (filter.value())
        {
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
    }

    VkSamplerAddressMode GLTFLoader::ExtractAddressMode(fastgltf::Wrap wrap)
    {
        switch (wrap)
        {
        case fastgltf::Wrap::ClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case fastgltf::Wrap::MirroredRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case fastgltf::Wrap::Repeat:
        default:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }

    SamplerHandle GLTFLoader::CreateSampler(LoadContext &ctx, const fastgltf::Sampler &sampler)
    {
        SamplerCreation creation;
        creation
            .SetMinMagMip(
                ExtractFilter(sampler.minFilter),
                ExtractFilter(sampler.magFilter),
                ExtractMipmapMode(sampler.minFilter))
            .SetAddressModeUVW(
                ExtractAddressMode(sampler.wrapS),
                ExtractAddressMode(sampler.wrapT),
                ExtractAddressMode(sampler.wrapT) // GLTF只支持2D纹理，所以W方向使用和T相同的寻址模式
                )
            .SetName(sampler.name.c_str());

        return ctx.device->CreateResource(creation);
    }

    bool GLTFLoader::LoadMeshes(LoadContext &ctx)
    {
        ctx.mesh_cache.resize(ctx.asset.meshes.size());

        for (size_t mesh_idx = 0; mesh_idx < ctx.asset.meshes.size(); mesh_idx++)
        {
            const auto &gltf_mesh = ctx.asset.meshes[mesh_idx];
            auto mesh = std::make_shared<MeshAsset>();
            mesh->name = gltf_mesh.name.empty() ? "mesh_" + std::to_string(mesh_idx) : std::string(gltf_mesh.name.c_str());

            mesh->mesh_data.vertices.clear();
            mesh->mesh_data.indices.clear();

            for (const auto &primitive : gltf_mesh.primitives)
            {
                GeoSurface surface;
                surface.first_index = static_cast<uint32_t>(mesh->mesh_data.indices.size());
                surface.index_count = static_cast<uint32_t>(ctx.asset.accessors[primitive.indicesAccessor.value()].count);

                size_t initial_vtx = mesh->mesh_data.vertices.size();
                glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
                glm::vec3 max = glm::vec3(std::numeric_limits<float>::min());
                // 加载索引
                {
                    const auto &index_accessor = ctx.asset.accessors[primitive.indicesAccessor.value()];
                    mesh->mesh_data.indices.reserve(mesh->mesh_data.indices.size() + index_accessor.count);

                    fastgltf::iterateAccessor<uint32_t>(
                        ctx.asset,
                        index_accessor,
                        [&](uint32_t idx)
                        {
                            mesh->mesh_data.indices.push_back(static_cast<uint32_t>(idx + initial_vtx));
                        });
                }

                // 加载顶点位置
                {
                    const auto &pos_accessor = ctx.asset.accessors[primitive.findAttribute("POSITION")->accessorIndex];
                    mesh->mesh_data.vertices.resize(mesh->mesh_data.vertices.size() + pos_accessor.count);

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        ctx.asset,
                        pos_accessor,
                        [&](glm::vec3 v, size_t index)
                        {
                            Vertex new_vtx;
                            new_vtx.position = v;
                            new_vtx.normal = {1, 0, 0};
                            new_vtx.color = glm::vec4{1.f};
                            new_vtx.uv_x = 0;
                            new_vtx.uv_y = 0;
                            mesh->mesh_data.vertices[initial_vtx + index] = new_vtx;
                            min = glm::min(min, v);
                            max = glm::max(max, v);
                        });
                }

                // 加载法线
                auto normal_it = primitive.findAttribute("NORMAL");
                if (normal_it != primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        ctx.asset,
                        ctx.asset.accessors[normal_it->accessorIndex],
                        [&](glm::vec3 v, size_t index)
                        {
                            mesh->mesh_data.vertices[initial_vtx + index].normal = v;
                        });
                }

                // 加载UV坐标
                auto uv_it = primitive.findAttribute("TEXCOORD_0");
                if (uv_it != primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(
                        ctx.asset,
                        ctx.asset.accessors[uv_it->accessorIndex],
                        [&](glm::vec2 v, size_t index)
                        {
                            mesh->mesh_data.vertices[initial_vtx + index].uv_x = v.x;
                            mesh->mesh_data.vertices[initial_vtx + index].uv_y = v.y;
                        });
                }

                // 加载顶点颜色
                auto color_it = primitive.findAttribute("COLOR_0");
                if (color_it != primitive.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(
                        ctx.asset,
                        ctx.asset.accessors[color_it->accessorIndex],
                        [&](glm::vec4 v, size_t index)
                        {
                            mesh->mesh_data.vertices[initial_vtx + index].color = v;
                        });
                }

                if (primitive.materialIndex.has_value())
                {
                    surface.material = ctx.material_cache[primitive.materialIndex.value()];
                }
                else
                {
                    surface.material = ctx.material_cache[0];
                }

                surface.bounds.SetMinMax(min, max);

                mesh->surfaces.push_back(surface);
                mesh->bounds.Merge(surface.bounds);
            }

            ctx.output->AddMesh(mesh->name, mesh);
            ctx.mesh_cache[mesh_idx] = mesh;
        }

        return true;
    }

    bool GLTFLoader::LoadNodes(LoadContext &ctx)
    {
        // 创建所有节点
        ctx.node_cache.resize(ctx.asset.nodes.size());

        for (size_t i = 0; i < ctx.asset.nodes.size(); i++)
        {
            const auto &gltf_node = ctx.asset.nodes[i];
            std::string node_name = gltf_node.name.empty() ? "node_" + std::to_string(i) : std::string(gltf_node.name.c_str());
            auto node = std::make_shared<SceneNode>(node_name);

            // 设置变换
            Transform transform;
            std::visit(
                fastgltf::visitor{
                    [&](const fastgltf::TRS &trs)
                    {
                        // 直接设置Transform组件
                        transform.position = glm::vec3(trs.translation[0],trs.translation[1],trs.translation[2]);
                        transform.rotation = glm::quat(trs.rotation[3], trs.rotation[0],trs.rotation[1], trs.rotation[2]);
                        transform.scale = glm::vec3(trs.scale[0],trs.scale[1],trs.scale[2]);
                    },
                    [&](const fastgltf::math::fmat4x4 &matrix)
                    {
                        // 使用Transform的Decompose函数分解矩阵
                        transform.Decompose(glm::make_mat4(matrix.data()));
                    }},
                gltf_node.transform);

            node->SetLocalTransform(transform);

            // 设置网格和材质
            if (gltf_node.meshIndex.has_value())
            {
                auto mesh = ctx.mesh_cache[gltf_node.meshIndex.value()];
                node->SetMesh(mesh);
            }

            ctx.node_cache[i] = node;
        }

        // 建立节点层级
        for (size_t i = 0; i < ctx.asset.nodes.size(); i++)
        {                         
            const auto &gltf_node = ctx.asset.nodes[i];
            auto node = ctx.node_cache[i];
            
            for (auto child_idx : gltf_node.children)
            {
                node->AddChild(ctx.node_cache[child_idx]);
            }

            ctx.output->AddNode(node);
        }

        // 设置根节点
        if (ctx.asset.scenes.empty())
        {
            LOGI("No scene found in GLTF file");
            return false;
        }

        return true;
    }

    bool GLTFLoader::LoadExternalBuffer(const std::filesystem::path &path, std::vector<uint8_t> &out_data)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            LOGE("Failed to open external buffer: {}", path.string());
            return false;
        }

        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        out_data.resize(size);
        file.read(reinterpret_cast<char *>(out_data.data()), size);

        return true;
    }

    void GLTFLoader::LogLoadingError(const std::string &message)
    {
        LOGE("GLTF Loading Error: {}", message);
    }

} // namespace lincore::scene
