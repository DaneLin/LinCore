#include "scene_graph.h"

#include <algorithm>

namespace lc
{
    void SceneGraph::Init(uint32_t num_nodes)
    {
        nodes_hierarchy.resize(num_nodes);
        local_matrices.resize(num_nodes);
        world_matrices.resize(num_nodes);
        updated_nodes.resize(num_nodes, false);
    }

    void SceneGraph::Shutdown()
    {
        nodes_hierarchy.clear();
        local_matrices.clear();
        world_matrices.clear();
        updated_nodes.clear();
    }

    void SceneGraph::Resize(uint32_t num_nodes)
    {
        nodes_hierarchy.resize(num_nodes);
        local_matrices.resize(num_nodes);
        world_matrices.resize(num_nodes);
        updated_nodes.resize(num_nodes, false);

        memset(nodes_hierarchy.data(), 0, sizeof(Hierarchy) * num_nodes);

        for (uint32_t i = 0; i < num_nodes; ++i)
        {
            nodes_hierarchy[i].parent = -1;
        }
    }

    void SceneGraph::UpdateMatrices()
    {
        // TODO : per level update
        uint32_t max_level = 0;
        for (uint32_t i =0 ; i < nodes_hierarchy.size(); ++i)
        {
            max_level = std::max(max_level, static_cast<uint32_t>(nodes_hierarchy[i].level));
        }

        uint32_t current_level = 0;
        uint32_t nodes_visited = 0;

        while (current_level <= max_level)
        {
            for (uint32_t i = 0; i < nodes_hierarchy.size(); ++i)
            {
                if (nodes_hierarchy[i].level != current_level)
                {
                    continue;
                }

                if (updated_nodes[i])
                {
                    continue;
                }

                updated_nodes[i] = true;

                if (nodes_hierarchy[i].parent == -1)
                {
                    world_matrices[i] = local_matrices[i];
                }
                else
                {
                    const glm::mat4& parent_matrix = world_matrices[ nodes_hierarchy[ i ].parent ];
                    world_matrices[i] = parent_matrix * local_matrices[i];
                }

                ++nodes_visited;
            }

            ++current_level;
        }
    }

    void SceneGraph::SetHierarchy(uint32_t node_index, uint32_t parent_index, uint32_t level)
    {
        // Mark node as updated
        updated_nodes[node_index] = true;
        nodes_hierarchy[node_index].parent = parent_index;
        nodes_hierarchy[node_index].level = level;

        sort_update_order = true;
    }

    void SceneGraph::SetLocalMatrix(uint32_t node_index, const glm::mat4 &matrix)
    {
        // Mark node as updated
        updated_nodes[node_index] = true;
        local_matrices[node_index] = matrix;
    }
}