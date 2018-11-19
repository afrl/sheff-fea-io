#pragma once

#include <vector>

#include <nlohmann/json.hpp>

namespace fea_io
{
	using json = nlohmann::json;

	enum class ArrayType {
		Node, /// One value/vector per a node
		Element, /// One value/vector per an element
		NodeByElement, /// A value/vector for each node in each element (nodes have multiple values if shared between elements)
		NodeByComponent, /// A value/vector for each node in each component (nodes have multiple values if shared between components)
		QuadPointByElement, /// One value/vector per a quad point per an element
	};

	enum class ArrayDataType {
		FloatingPoint,
		Integral
	};

	struct ArrayInfo
	{
		// Set by constructor
		std::string name;
		ArrayType type;
		ArrayDataType data_type;
		int32_t data_type_num_bytes;
		int32_t num_comps;
		std::vector<std::string> comp_names;
		/// The id of the data array (or index in the vector of ArrayInfo objects)
		/// Note: set within DataWriter::intialize or GroupWriter::initialize
		int32_t id;
		/// The offset of the data for each partition relative to the beginning of the binary file.
		/// Note: set within DataWriter::initialize or GroupWriter::initialize
		std::vector<std::streamoff> begin_offsets;
		/// The number of bytes needed for the data in each partition.
		/// Note: set within DataWriter::initialize or GroupWriter::initialize
		std::vector<std::streamoff> num_bytes;
		/// A counter of the number of bytes written for each partition.  Only used for run-time checks.
		/// Note: excluded from JSON conversion
		std::vector<std::streamoff> curr_bytes_written;

		ArrayInfo() {};

		template<typename ValueType>
		void initialize(
			const std::string name_a,
			const ArrayType type_a,
			const int32_t num_comps_a,
			const std::vector<std::string> comp_names_a)
		{
			name = name_a;
			type = type_a;
			num_comps = num_comps_a;
			comp_names = comp_names_a;

			id = -1;
			if (std::is_floating_point<ValueType>::value)
				data_type = ArrayDataType::FloatingPoint;
			else if (std::is_integral<ValueType>::value)
				data_type = ArrayDataType::Integral;
			else
				throw _MSG_EXCEPT_C("Currently the format only supports floating and integral data types.");
			data_type_num_bytes = sizeof(ValueType);
		}

		void resize_offsets(const int64_t num_partitions) {
			begin_offsets.resize(num_partitions);
			std::fill(begin_offsets.begin(), begin_offsets.end(), 0);
			curr_bytes_written.resize(num_partitions);
			std::fill(curr_bytes_written.begin(), curr_bytes_written.end(), 0);
			num_bytes.resize(num_partitions);
			std::fill(num_bytes.begin(), num_bytes.end(), 0);
		}

		/// Returns the number of bytes needed for this data array on a single partition
		/// This version of get_size_needed(...) is called by DataWriter::initialize(...)
		template<typename PartitionInfo>
		std::streamoff get_size_needed(const PartitionInfo& pi, const int64_t& total_num_quad_points)
		{
			if (type == ArrayType::Node)
				return pi.num_nodes * num_comps * data_type_num_bytes;
			else if (type == ArrayType::Element)
				return pi.num_elements * num_comps * data_type_num_bytes;
			else if (type == ArrayType::NodeByElement)
				return pi.total_num_connectivity_entries * num_comps * data_type_num_bytes;
			else if (type == ArrayType::NodeByComponent) {
				std::streamoff size_needed = 0;
				for (auto& subcomponent_info : pi.subcomponents)
					size_needed += subcomponent_info.num_nodes_in_subcomponent * num_comps * data_type_num_bytes;
				return size_needed;
			}
			else if (type == ArrayType::QuadPointByElement)
				// The number of quad points per an element will be written along with the data for this
				// case, so account for the size of those lengths.  Type will be int16_t.
				return pi.num_elements * sizeof(int16_t) + total_num_quad_points * num_comps * data_type_num_bytes;
			else
				throw _MSG_EXCEPT_C("Unrecognized data array type.");
		}
	};

	static void to_json(json& j, const ArrayInfo& ss)
	{
		j = json{ {"id", ss.id},
			{"type", ss.type},
			{"data_type", ss.data_type},
			{"data_type_num_bytes", ss.data_type_num_bytes},
			{"num_comps", ss.num_comps},
			{"name", ss.name},
			{"comp_names", ss.comp_names},
			{"begin_offsets", ss.begin_offsets},
			{"num_bytes", ss.num_bytes}
		};
	}

	static void from_json(const json& j, ArrayInfo& ss)
	{
		j.at("id").get_to(ss.id);
		j.at("type").get_to(ss.type);
		j.at("data_type").get_to(ss.data_type);
		j.at("data_type_num_bytes").get_to(ss.data_type_num_bytes);
		j.at("num_comps").get_to(ss.num_comps);
		j.at("name").get_to(ss.name);
		j.at("comp_names").get_to(ss.comp_names);
		j.at("begin_offsets").get_to(ss.begin_offsets);
		j.at("num_bytes").get_to(ss.num_bytes);
	}
}