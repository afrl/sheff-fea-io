#pragma once

#include <vector>

#include <nlohmann/json.hpp>

namespace fea_io
{
	using json = nlohmann::json;

	enum class DataArrayType {
		Node, /// One value/vector per a node
		Element, /// One value/vector per an element
		NodeByElement, /// A value/vector for each node in each element (nodes have multiple values if shared between elements)
		NodeByComponent, /// A value/vector for each node in each component (nodes have multiple values if shared between components)
		QuadPointByElement, /// One value/vector per a quad point per an element
	};

	template<typename LocalIndex, typename CoordType>
	struct DataArrayInfo
	{
		typedef CoordType ValueType;

		// Set by constructor
		LocalIndex id;
		DataArrayType type;
		LocalIndex num_comps;
		std::string name;
		std::vector<std::string> comp_names;
		// The following variables are set by DataWriter::initialize
		std::vector<std::streamoff> begin_offsets;
		std::vector<std::streamoff> num_bytes;
		// Note: excluded from JSON conversion
		std::vector<std::streamoff> curr_bytes_written;

		DataArrayInfo() {};

		DataArrayInfo(
			const LocalIndex id_a,
			const DataArrayType type_a,
			const LocalIndex num_comps_a,
			const std::string name_a,
			const std::vector<std::string> comp_names_a)
			: id(id_a), type(type_a), num_comps(num_comps_a), name(name_a), comp_names(comp_names_a)
		{
		}

		template<typename PartitionInfo>
		std::streamoff get_size_needed(const PartitionInfo& pi, const LocalIndex& total_num_quad_points)
		{
			if (type == DataArrayType::Node)
				return pi.num_nodes * num_comps * sizeof(CoordType);
			else if (type == DataArrayType::Element)
				return pi.num_elements * num_comps * sizeof(CoordType);
			else if (type == DataArrayType::NodeByElement)
				return pi.total_num_connectivity_entries * num_comps * sizeof(CoordType);
			else if (type == DataArrayType::NodeByComponent) {
				std::streamoff size_needed = 0;
				for (auto& subcomponent_info : pi.subcomponents)
					size_needed += subcomponent_info.num_nodes_in_subcomponent * num_comps * sizeof(CoordType);
				return size_needed;
			}
			else if (type == DataArrayType::QuadPointByElement)
				return total_num_quad_points * num_comps * sizeof(CoordType);
			else
				throw _MSG_EXCEPT_C("Unrecognized data array type.");
		}
	};

	template<typename LocalIndex, typename CoordType>
	void to_json(json& j, const DataArrayInfo<LocalIndex, CoordType>& ss)
	{
		j = json{ {"id", ss.id},
			{"type", ss.type},
			{"num_comps", ss.num_comps},
			{"name", ss.name},
			{"comp_names", ss.comp_names},
			{"begin_offsets", ss.begin_offsets},
			{"num_bytes", ss.num_bytes}
		};
	}

	template<typename LocalIndex, typename CoordType>
	void from_json(const json& j, DataArrayInfo<LocalIndex, CoordType>& ss)
	{
		j.at("id").get_to(ss.id);
		j.at("type").get_to(ss.type);
		j.at("num_comps").get_to(ss.num_comps);
		j.at("name").get_to(ss.name);
		j.at("comp_names").get_to(ss.comp_names);
		j.at("begin_offsets").get_to(ss.begin_offsets);
		j.at("num_bytes").get_to(ss.num_bytes);
	}
}