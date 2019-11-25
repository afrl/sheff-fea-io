#pragma once

#include "fea_io/mesh/MeshReader.h"
#include "fea_io/meta/MetaReader.h"

namespace fea_io
{
    template<typename GlobalIndex, typename LocalIndex, typename CoordType>
    std::vector<LevelOfDetail> get_reversed_lods(
        const MetaReader& meta_reader,
        const int64_t& time_step_id)
    {
        auto& analysis_info = meta_reader.get_analysis_meta();
        auto find_iter = std::find(
            analysis_info.timesteps.begin(),
            analysis_info.timesteps.end(),
            time_step_id);
        if (find_iter == analysis_info.timesteps.end())
        {
            _MSG_EXCEPT("Time step not found with id: " + std::to_string(time_step_id));
        }

        auto& levels_of_detail = (*find_iter).levels_of_detail;

        auto levels_of_detail_reversed = levels_of_detail;
        // Assume that the JSON may not have the level's of detail sorted.
        std::sort(
            levels_of_detail_reversed.begin(),
            levels_of_detail_reversed.end(),
            [](const LevelOfDetail& lhs, const LevelOfDetail& rhs) -> bool
        {
            return lhs.level < rhs.level;
        });
        // Make sure it is contiguous (note: not reversed quite yet)
        for (std::size_t i = 0; i < levels_of_detail_reversed.size(); i++)
        {
            if (levels_of_detail_reversed[i].level != i)
            {
                _MSG_EXCEPT("Levels of detail are expected to be have a contiguous range from \
                    0 to N-1, where N is the number of LoD's, which was not the case in the meta file.");
            }
        }

        // Now reverse so we start with the highest level of detail.
        std::reverse(levels_of_detail_reversed.begin(), levels_of_detail_reversed.end());

        if (levels_of_detail_reversed.size() > analysis_info.levels_of_detail_names.size())
        {
            _MSG_EXCEPT("There wre more levels of details than there were names in the meta file.");
        }

        return levels_of_detail_reversed;
    }

    template<typename GlobalIndex, typename LocalIndex, typename CoordType>
    struct LODWithMesh
    {
        int partition;
        std::vector<MeshObject> mesh_objects;
        std::vector<Partition<GlobalIndex, LocalIndex, CoordType>> mesh_object_parts;
        std::vector<bool> parent_active_element_flags;
    };

    template<typename GlobalIndex, typename LocalIndex, typename CoordType>
    LODWithMesh<GlobalIndex, LocalIndex, CoordType>
    read_level_of_detail_helper(
        const MetaReader& meta_reader,
        const std::vector<LevelOfDetail>& levels_of_detail_reversed,
        const size_t& level_of_detail_rev_index,
        const int& partition)
    {
        using MeshReaderTyped = MeshReader<GlobalIndex, LocalIndex, CoordType>;

        auto& lod = levels_of_detail_reversed[level_of_detail_rev_index];
        LevelOfDetail* parent_lod_ptr = nullptr;
        if (level_of_detail_rev_index != levels_of_detail_reversed.size() - 1)
        {
            auto parent_lod_ptr = &levels_of_detail_reversed[level_of_detail_rev_index + 1];
        }

        // Allocate active map for parent LOD
        LODWithMesh<GlobalIndex, LocalIndex, CoordType> lod_w_mesh;
        MeshObject* parent_mesh_object_ptr = nullptr;
        if (parent_lod_ptr != nullptr)
        {
            for (auto& mesh_object : parent_lod_ptr->mesh_objects)
            {
                if (mesh_object.type == MeshObjectType::Volume)
                {
                    if (parent_mesh_object_ptr == nullptr)
                    {
                        parent_mesh_object_ptr = &mesh_object;

                        // TODO get adjusted path
                        auto mesh_path = mesh_object.mesh_path;
                        MeshReaderTyped mesh_reader(mesh_path);
                        auto& partition_info = mesh_reader.get_partition_info();
                        lod_w_mesh.parent_active_element_flags.resize(
                            partition_info[partition].num_elements, true);
                    }
                    else
                    {
                        // The JSON would have to be extended to indicate which mesh object
                        // is the parent for a lower level if multiple mesh objects are to
                        // be supported.
                        _MSG_EXCEPT("There were multiple mesh objects for level " +
                            std::to_string(parent_lod_ptr->level) +
                            ", which is not supported right now as a parent level of detail.");
                    }
                }
            }
        }

        for (auto& mesh_object : lod.mesh_objects)
        {
            if (mesh_object.type == MeshObjectType::Volume)
            {
                lod_w_mesh.mesh_objects.push_back(mesh_object);

                // TODO get adjusted path
                auto mesh_path = mesh_object.mesh_path;
                MeshReaderTyped mesh_reader(mesh_path);
                auto& partition_info = mesh_reader.get_partition_info();
                partition_info[partition].num_nodes;
                partition_info[partition].num_elements;

                lod_w_mesh.mesh_object_parts.emplace_back(
                    mesh_reader.read_partition(partition));

                // Set parent active element flags
                if (parent_mesh_object_ptr != nullptr)
                {
                    auto& part = lod_w_mesh.mesh_object_parts.back();
                    for (auto& parent_local_id : part.element_global_ids)
                    {
                        lod_w_mesh.parent_active_element_flags[parent_local_id] = false;
                    }
                }
            }
        }
        
        return lod_w_mesh;
    }
}