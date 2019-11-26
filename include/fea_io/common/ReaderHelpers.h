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
        auto find_iter = std::find_if(
            analysis_info.timesteps.begin(),
            analysis_info.timesteps.end(),
			[time_step_id](const TimeStepInfo& ts) -> bool { return ts.time_step_id == time_step_id; });
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

	/// The meta file JSON is a bit more flexible than the reader supports.
	/// This helper method validates that there is only one mesh object
	/// for the given type of mesh at the level of detail indicated.
	/// \param levels_of_detail_reversed - should come from get_reversed_lods()
	std::vector<MeshObject> get_mesh_obj_for_each_level(
		const std::vector<LevelOfDetail>& levels_of_detail_reversed,
		const MeshObjectType type)
	{
		std::vector<MeshObject> mesh_objects;
		for (auto& lod : levels_of_detail_reversed)
		{
			bool found_object = false;
			for (auto& mesh_object : lod.mesh_objects)
			{
				if (mesh_object.type == type)
				{
					if (found_object)
					{
						_MSG_EXCEPT("Already found a mesh object for level of detail " +
							std::to_string(lod.level) + " of the same type and the implementation \
						cannot currently support multiple.");
					}
					found_object = true;

					mesh_objects.push_back(mesh_object);
				}
			}
		}
		return mesh_objects;
	}

    template<typename GlobalIndex, typename LocalIndex, typename CoordType>
    struct LODWithPart
    {
        int partition;
        MeshObject mesh_object;
        Partition<GlobalIndex, LocalIndex, CoordType> mesh_object_part;
        std::vector<short> parent_element_relative_levels;
    };

	/// The meta file JSON is a bit more flexible than the reader supports.
	/// This helper method validates that there is only one mesh object
	/// for the given type of mesh at the level of detail indicated.
	///
	/// Note: assumes the elements of the parent level should be inactive if
	/// their local id appears as a global id in the lower level.
	///
	/// \param lod_mesh_objects - should come from get_mesh_obj_for_each_level()
    template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	LODWithPart<GlobalIndex, LocalIndex, CoordType>
    read_level_of_detail_helper(
        const MetaReader& meta_reader,
		const std::vector<LevelOfDetail>& lods_reversed,
		const std::vector<MeshObject>& lod_rev_mesh_objects,
        const size_t& lod_rev_index,
        const int& partition,
		const MeshObjectType type)
    {
        using MeshReaderTyped = MeshReader<GlobalIndex, LocalIndex, CoordType>;

        auto& mesh_object = lod_rev_mesh_objects[lod_rev_index];
		const MeshObject* parent_mesh_obj_ptr = nullptr;
        if (lod_rev_index != lod_rev_mesh_objects.size() - 1
			&& lods_reversed[lod_rev_index].relationship_with_parent == LODParentRelation::ConformingOverlapping)
        {
            parent_mesh_obj_ptr = &(lod_rev_mesh_objects[lod_rev_index + 1]);
        }

        // Allocate active map for parent LOD
		LODWithPart<GlobalIndex, LocalIndex, CoordType> lod_w_part;
        if (parent_mesh_obj_ptr != nullptr)
        {
            auto mesh_path = meta_reader.get_adjusted_path(parent_mesh_obj_ptr->mesh_path);
            MeshReaderTyped mesh_reader(mesh_path);

            auto& partition_info = mesh_reader.get_partition_info();
			lod_w_part.parent_element_relative_levels.resize(
                partition_info[partition].num_elements,
				static_cast<short>(lod_rev_index - 1));
        }

		lod_w_part.mesh_object = mesh_object;

        auto mesh_path = meta_reader.get_adjusted_path(mesh_object.mesh_path);
        MeshReaderTyped mesh_reader(mesh_path);

        auto& partition_info = mesh_reader.get_partition_info();
        partition_info[partition].num_nodes;
        partition_info[partition].num_elements;

        mesh_reader.read_partition(partition, lod_w_part.mesh_object_part);

        // Set parent active element flags
        if (parent_mesh_obj_ptr != nullptr)
        {
			auto this_level = static_cast<short>(lods_reversed[lod_rev_index].level);
            auto& part = lod_w_part.mesh_object_part;
            for (auto& parent_local_id : part.element_global_ids)
            {
				lod_w_part.parent_element_relative_levels[parent_local_id] =
					static_cast<short>(-this_level);
            }
        }
        
        return lod_w_part;
    }
}