#pragma once

#include <vector>

#include <nlohmann/json.hpp>

#include "fea_io/data/DataStructs.h"
#include "fea_io/common/Exceptions.h"

namespace fea_io
{
	using json = nlohmann::json;

	enum class GroupType : char
	{
		Node,
		Element
	};

	static std::string to_friendly_name(const GroupType& gt)
	{
		if (gt == GroupType::Node) return "Node";
		else if (gt == GroupType::Element) return "Element";
		else throw _MSG_EXCEPT("GroupType " + std::to_string(static_cast<std::underlying_type<GroupType>::type>(gt)) + " not recognized.");
	}

	static GroupType group_type_from_name(const std::string& name)
	{
		if (name == "Node") return GroupType::Node;
		else if (name == "Element") return GroupType::Element;
		else throw _MSG_EXCEPT("GroupType " + name + " not recognized.");
	}

	struct GroupInfo
	{
		/// The name of node/element group
		std::string name;
		/// The type of group (node or element)
		GroupType type;
		/// The data arrays to write for the group.
		std::vector<ArrayInfo> array_infos;

		/// The number of nodes/elements in the group.  Note: the size should match
		/// the total number of partitions in the mesh.  Any partition that does not
		/// contain a node or element in the group should just have a 0.
		/// For use with GroupWriter, which writes in serial, this is set as groups are written.
		/// For use with GroupParWriter, this must be set before GroupParWriter::intialize() is
		/// called, and it can be set in GroupInfo::initialize().
		std::vector<int64_t> num_objects_in_partitions;
		
		/// The number of bytes that the integral type requres
		/// Note: set within GroupWriter::initialize
		int8_t index_type_num_bytes;
		/// The id of the data array (or index in the vector of ArrayInfo objects)
		/// Note: set within GroupWriter::initialize
		int32_t id;
		/// The offset of the index list for the nodes/elements in the group relative
		/// to the beginning of the file.  Note: set by GroupWriter::initialize(...)
		std::vector<std::streamoff> object_list_offsets;

		template<typename IndexType>
		void initialize(
			const std::string name_a,
			const GroupType type_a,
			const std::vector<ArrayInfo>& array_infos_a,
			const std::vector<int64_t>& num_objects_in_partitions_a = {})
		{
			name = name_a;
			type = type_a;
			array_infos = array_infos_a;
			num_objects_in_partitions = num_objects_in_partitions_a;

			id = -1;
			if (not std::is_integral<IndexType>::value)
				throw _MSG_EXCEPT_C("Groups must use integral data types for the indexing, but a non-integral type was provided.");
			index_type_num_bytes = sizeof(IndexType);
		}
	};

	static void to_json(json& j, const GroupInfo& h)
	{
		j = json{ {"id", h.id},
			{"name", h.name},
			{"type", to_friendly_name(h.type)},
			{"array_infos", h.array_infos},
			{"index_type_num_bytes", h.index_type_num_bytes},
			{"num_objects_in_partitions", h.num_objects_in_partitions},
			{"object_list_offsets", h.object_list_offsets}
		};
	}

	static void from_json(const json& j, GroupInfo& h)
	{
		j.at("id").get_to(h.id);
		j.at("name").get_to(h.name);
		h.type = group_type_from_name(j.at("type").get<std::string>());
		j.at("array_infos").get_to(h.array_infos);
		j.at("index_type_num_bytes").get_to(h.index_type_num_bytes);
		j.at("num_objects_in_partitions").get_to(h.num_objects_in_partitions);
		j.at("object_list_offsets").get_to(h.object_list_offsets);
	}

}