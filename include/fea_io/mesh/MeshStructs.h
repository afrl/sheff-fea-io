#pragma once

#include <array>
#include <numeric>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "fea_io/common/Comparators.h"
#include "fea_io/common/Exceptions.h"

#include "MeshEnums.h"

namespace fea_io
{
	using json = nlohmann::json;

	/// Holds the data describing a subcomponent
	template<typename LocalIndex, typename CoordType>
	struct Subcomponent
	{
		LocalIndex component_id;
		std::vector<LocalIndex> nodes_in_subcomponent;
		std::vector<LocalIndex> elements_in_subcomponent;
		std::vector<CoordType> lcs_parameters_in_subcomponent;
	};

	template<typename LocalIndex, typename CoordType,
		typename LocalIndex2, typename CoordType2>
	bool operator ==(const Subcomponent<LocalIndex, CoordType>& lhs, const Subcomponent<LocalIndex2, CoordType2>& rhs)
	{
		return (lhs.component_id == rhs.component_id)
			&& compare_vec(lhs.nodes_in_subcomponent, rhs.nodes_in_subcomponent)
			&& compare_vec(lhs.elements_in_subcomponent, rhs.elements_in_subcomponent)
			&& compare_vec(lhs.lcs_parameters_in_subcomponent, rhs.lcs_parameters_in_subcomponent);
	};

	/// Holds the mesh for a partition
	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	struct Partition
	{
		LocalIndex partition_id;
		std::vector<GlobalIndex> node_global_ids;
		std::vector<LocalIndex> node_parent_partitions;
		std::vector<CoordType> node_coords;
		std::vector<GlobalIndex> element_global_ids;
		std::vector<LocalIndex> element_parent_partitions;
		std::vector<ElementType> element_types;
		std::vector<std::vector<LocalIndex>> element_connectivities;
		std::vector<Subcomponent<LocalIndex, CoordType>> subcomponents;

		/// A helper to resize the node and element vectors
		void resize(const LocalIndex num_nodes, const LocalIndex num_elements) {
			node_global_ids.resize(num_nodes);
			node_parent_partitions.resize(num_nodes);
			node_coords.resize(num_nodes);
			element_global_ids.resize(num_elements);
			element_parent_partitions.resize(num_elements);
			element_types.resize(num_elements);
			element_connectivities.resize(num_elements);
		}
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType,
		typename GlobalIndex2, typename LocalIndex2, typename CoordType2>
	bool operator ==(
        const Partition<GlobalIndex, LocalIndex, CoordType>& lhs,
        const Partition<GlobalIndex2, LocalIndex2, CoordType2>& rhs)
	{
		return (lhs.partition_id == rhs.partition_id)
			&& compare_vec(lhs.node_global_ids, rhs.node_global_ids)
			&& compare_vec(lhs.node_parent_partitions, rhs.node_parent_partitions)
			&& compare_vec(lhs.node_coords, rhs.node_coords)
			&& compare_vec(lhs.element_global_ids, rhs.element_global_ids)
			&& compare_vec(lhs.element_parent_partitions, rhs.element_parent_partitions)
			&& compare_vec(lhs.element_types, rhs.element_types)
			&& compare_vec_vec(lhs.element_connectivities, rhs.element_connectivities)
			&& compare_vec(lhs.subcomponents, rhs.subcomponents);
	};
}