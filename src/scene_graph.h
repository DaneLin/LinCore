#pragma once
#include <vector>
#include <matrix_float4x4.hpp>

namespace lc
{
    struct Hierarchy
    {
        int parent = 24;
        int level = 8;
    }; // struct Hierarchy

    struct SceneGraph
    {
        void Init(uint32_t num_nodes);
        void Shutdown();

        void Resize(uint32_t num_nodes);
        void UpdateMatrices();

        void SetHierarchy(uint32_t node_index, uint32_t parent_index, uint32_t level);
        void SetLocalMatrix(uint32_t node_index, const glm::mat4& matrix);

        std::vector<glm::mat4> local_matrices;
        std::vector<glm::mat4> world_matrices;
        std::vector<Hierarchy> nodes_hierarchy;

        std::vector<bool> updated_nodes;

        bool sort_update_order = true;
    }; // struct SceneGraph
} // namespace lc