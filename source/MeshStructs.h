#pragma once

#include <array>
#include <numeric>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "MeshEnums.h"
#include "Exceptions.h"

namespace fea_io
{
	using json = nlohmann::json;

	template<typename LocalIndex>
	struct ComponentMetaInfo
	{
		LocalIndex id;
		std::string name;
		LCSAssignment lcs_assignment;
		LCSParameterization lcs_parameterization;

		auto tie() const {
			return std::tie(id, name, lcs_assignment, lcs_parameterization);
		}
	};

	template<typename LocalIndex>
	void to_json(json& j, const ComponentMetaInfo<LocalIndex>& h)
	{
		j = json{ {"id", h.id},
			{"name", h.name},
			{"lcs_assignment", to_friendly_name(h.lcs_assignment)},
			{"lcs_parameterization", to_friendly_name(h.lcs_parameterization)}
		};
	}

	template<typename LocalIndex>
	void from_json(const json& j, ComponentMetaInfo<LocalIndex>& h)
	{
		j.at("id").get_to(h.id);
		j.at("name").get_to(h.name);
		h.lcs_assignment = assignment_from_name(j.at("lcs_assignment").get<std::string>());
		h.lcs_parameterization = parameterizaiton_from_name(j.at("lcs_parameterization").get<std::string>());
	}

	template<typename CoordType>
	struct Extent
	{
		std::array<CoordType, 3> min;
		std::array<CoordType, 3> max;

		CoordType& x_min() { return min[0]; }
		CoordType& y_min() { return min[1]; }
		CoordType& z_min() { return min[2]; }
		CoordType& x_max() { return max[0]; }
		CoordType& y_max() { return max[1]; }
		CoordType& z_max() { return max[2]; }

		const CoordType& x_min() const { return min[0]; }
		const CoordType& y_min() const { return min[1]; }
		const CoordType& z_min() const { return min[2]; }
		const CoordType& x_max() const { return max[0]; }
		const CoordType& y_max() const { return max[1]; }
		const CoordType& z_max() const { return max[2]; }

		Extent() {
			min = { std::numeric_limits<CoordType>::max(),
				std::numeric_limits<CoordType>::max(),
				std::numeric_limits<CoordType>::max() };
			max = { std::numeric_limits<CoordType>::min(),
				std::numeric_limits<CoordType>::min(),
				std::numeric_limits<CoordType>::min() };
		}
		Extent(const std::array<CoordType, 3> min_a, const std::array<CoordType, 3> max_a) {
			min = min_a;
			max = max_a;
		}

		auto tie() const {
			return std::tie(min, max);
		}
	};

	template<typename CoordType>
	void to_json(json& j, const Extent<CoordType>& e)
	{
		j = json{ {"x_min", e.x_min()},
			{"y_min", e.y_min()},
			{"z_min", e.z_min()},
			{"x_max", e.x_max()},
			{"y_max", e.y_max()},
			{"z_max", e.z_max()}
		};
	}

	template<typename CoordType>
	void from_json(const json& j, Extent<CoordType>& e)
	{
		j.at("x_min").get_to(e.x_min());
		j.at("y_min").get_to(e.y_min());
		j.at("z_min").get_to(e.z_min());
		j.at("x_max").get_to(e.x_max());
		j.at("y_max").get_to(e.y_max());
		j.at("z_max").get_to(e.z_max());
	}
	

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	struct MeshMetaInfo
	{
		GlobalIndex total_num_nodes;
		GlobalIndex total_num_elements;
		LocalIndex dimensionality;
		Extent<CoordType> extent;
		std::vector<ComponentMetaInfo<LocalIndex>> components;
		LocalIndex total_num_partitions;
		std::vector<std::streamoff> partition_offsets;

		auto tie() const {
			return std::tie(total_num_nodes,
				total_num_elements, dimensionality, extent,
				components, total_num_partitions, partition_offsets);
		}
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	bool operator ==(const MeshMetaInfo<GlobalIndex, LocalIndex, CoordType>& lhs, const MeshMetaInfo<GlobalIndex, LocalIndex, CoordType>& rhs)
	{
		return lhs.tie() == rhs.tie();
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void to_json(json& j, const MeshMetaInfo<GlobalIndex, LocalIndex, CoordType>& h)
	{
		j = json{ {"total_num_nodes", h.total_num_nodes},
			{"total_num_elements", h.total_num_elements},
			{"dimensionality", h.dimensionality},
			{"extent", h.extent},
			{"components", h.components},
			{"total_num_partitions", h.total_num_partitions},
			{"partition_offsets", h.partition_offsets}
		};
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void from_json(const json& j, MeshMetaInfo<GlobalIndex, LocalIndex, CoordType>& h)
	{
		j.at("total_num_nodes").get_to(h.total_num_nodes);
		j.at("total_num_elements").get_to(h.total_num_elements);
		j.at("dimensionality").get_to(h.dimensionality);
		j.at("extent").get_to(h.extent);
		j.at("components").get_to(h.components);
		j.at("total_num_partitions").get_to(h.total_num_partitions);
		j.at("partition_offsets").get_to(h.partition_offsets);
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	struct DataTypeSizes
	{
		const int8_t global_index_size = sizeof(GlobalIndex);
		const int8_t local_index_size = sizeof(LocalIndex);
		const int8_t coord_type_size = sizeof(CoordType);
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void to_json(json& j, const DataTypeSizes<GlobalIndex, LocalIndex, CoordType>& dti)
	{
		j = json{ {"global_index_size", dti.global_index_size},
			{"local_index_size", dti.local_index_size},
			{"coord_type_size", dti.coord_type_size}
		};
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void from_json(const json& j, DataTypeSizes<GlobalIndex, LocalIndex, CoordType>& dti)
	{
		j.at("global_index_size").get_to(dti.global_index_size);
		j.at("local_index_size").get_to(dti.local_index_size);
		j.at("coord_type_size").get_to(dti.coord_type_size);
	}

	template<typename LocalIndex>
	struct SubcomponentInfo
	{
		LocalIndex component_id;
		LocalIndex num_nodes_in_subcomponent;
		LocalIndex num_elements_in_subcomponent;
	};

	template<typename LocalIndex>
	void to_json(json& j, const SubcomponentInfo<LocalIndex>& ci)
	{
		j = json{ {"component_id", ci.component_id},
			{"num_nodes_in_subcomponent", ci.num_nodes_in_subcomponent},
			{"num_elements_in_subcomponent", ci.num_elements_in_subcomponent}
		};
	}

	template<typename LocalIndex>
	void from_json(const json& j, SubcomponentInfo<LocalIndex>& ci)
	{
		j.at("component_id").get_to(ci.component_id);
		j.at("num_nodes_in_subcomponent").get_to(ci.num_nodes_in_subcomponent);
		j.at("num_elements_in_subcomponent").get_to(ci.num_elements_in_subcomponent);
	}

	/// Returns the number of bytes needed within the binary mesh file for this component
	/// within a partition.  This method is used when the offsets are being calculated.
	template<typename LocalIndex, typename CoordType>
	static std::streamoff get_size_for_subcomponent(
		const LocalIndex num_nodes_in_subcomponent,
		const LocalIndex num_elements_in_subcomponent,
		const LCSAssignment lcs_assignment,
		const LCSParameterization lcs_parameterization)
	{
		std::streamoff num_lcs_parameters = 0;
		if (lcs_assignment == LCSAssignment::Component)
			num_lcs_parameters = get_num_parameters(lcs_parameterization);
		else if (lcs_assignment == LCSAssignment::Element)
			num_lcs_parameters = num_elements_in_subcomponent * get_num_parameters(lcs_parameterization);
		else if (lcs_assignment == LCSAssignment::Node)
			num_lcs_parameters = num_nodes_in_subcomponent * get_num_parameters(lcs_parameterization);
		else
			throw _MSG_EXCEPT("LCS assignment not recognized.");

		return sizeof(LocalIndex) // component id
			+ sizeof(num_nodes_in_subcomponent)
			+ num_nodes_in_subcomponent * sizeof(LocalIndex)
			+ sizeof(num_elements_in_subcomponent)
			+ num_elements_in_subcomponent * sizeof(LocalIndex)
			+ num_lcs_parameters * sizeof(CoordType);
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	struct PartitionInfo
	{
		LocalIndex partition_id;
		LocalIndex num_nodes;
		LocalIndex num_elements;
		LocalIndex total_num_connectivity_entries;
		Extent<CoordType> extent;
		LocalIndex num_components;
		std::vector<SubcomponentInfo<LocalIndex>> subcomponents;

		/// Returns the number of bytes needed within the binary mesh file
		/// for this partition.  This method is used when the offsets are
		/// being calculated.
		std::streamoff get_size_needed(const MeshMetaInfo<GlobalIndex, LocalIndex, CoordType>& meta_info) const {
			// Compute the sizes individually to allow debuggers to check values, but a release compile will
			// optimize most of the code away.
			const std::streamoff size_for_partition_header = sizeof(partition_id) + sizeof(num_nodes) + sizeof(num_elements) + sizeof(extent);
			// Size of nodes accounts for global id, parent partition, and coordintes of each node
			const std::streamoff size_for_nodes = num_nodes * (sizeof(GlobalIndex) + sizeof(LocalIndex) + meta_info.dimensionality * sizeof(CoordType));
			// Size of element accounts for global id, type, parent partition, and connectivity of each element
			const std::streamoff size_for_elements = num_elements * (sizeof(GlobalIndex) + sizeof(ElementType) + sizeof(LocalIndex))
				+ total_num_connectivity_entries * sizeof(LocalIndex);
			const std::streamoff size_for_component_header = sizeof(num_components) + sizeof(std::streamoff) * num_components;
			// Figures out how much space will be needed for the subconstitutents
			std::streamoff size_for_subcomponents = 0;
			for (auto& subcomponent : subcomponents) {
				// Note: it was already tested that the component id exists in the meta_info
				auto component_iter = std::find_if(meta_info.components.begin(), meta_info.components.end(),
					[id = subcomponent.component_id](const ComponentMetaInfo<LocalIndex>& x) -> bool {
					return x.id == id;
				});
				if (component_iter == meta_info.components.end())
					throw _MSG_EXCEPT_C("In partition " + std::to_string(partition_id) + ", component ID " + std::to_string(subcomponent.component_id) + " was given, but it was not included as a component when constructed.");

				size_for_subcomponents += get_size_for_subcomponent<LocalIndex, CoordType>(subcomponent.num_nodes_in_subcomponent,
					subcomponent.num_elements_in_subcomponent, component_iter->lcs_assignment, component_iter->lcs_parameterization);
			}

			// Figure out the total size on disk needed to store
			// everything needed for the partition.
			return size_for_partition_header
				+ size_for_nodes + size_for_elements
				+ size_for_component_header
				+ size_for_subcomponents;
		};
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void to_json(json& j, const PartitionInfo<GlobalIndex, LocalIndex, CoordType>& ss)
	{
		j = json{ {"partition_id", ss.partition_id},
			{"num_nodes", ss.num_nodes},
			{"num_elements", ss.num_elements},
			{"total_num_connectivity_entries", ss.total_num_connectivity_entries},
			{"extent", ss.extent},
			{"num_components", ss.num_components},
			{"subcomponents", ss.subcomponents}
		};
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void from_json(const json& j, PartitionInfo<GlobalIndex, LocalIndex, CoordType>& ss)
	{
		j.at("partition_id").get_to(ss.partition_id);
		j.at("num_nodes").get_to(ss.num_nodes);
		j.at("num_elements").get_to(ss.num_elements);
		j.at("total_num_connectivity_entries").get_to(ss.total_num_connectivity_entries);
		j.at("extent").get_to(ss.extent);
		j.at("num_components").get_to(ss.num_components);
		j.at("subcomponents").get_to(ss.subcomponents);
	}

	/// Holds the data describing a subcomponent
	template<typename LocalIndex, typename CoordType>
	struct Subcomponent
	{
		LocalIndex component_id;
		std::vector<LocalIndex> nodes_in_subcomponent;
		std::vector<LocalIndex> elements_in_subcomponent;
		std::vector<CoordType> lcs_parameters_in_subcomponent;
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
}