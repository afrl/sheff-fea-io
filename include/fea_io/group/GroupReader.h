#pragma once

#include <fstream>
#include <map>
#include <stdint.h>
#include <vector>

#include <nlohmann/json.hpp>

#include "fea_io/common/Common.h"
#include "fea_io/common/Exceptions.h"
#include "fea_io/common/IOUtils.h"
#include "fea_io/common/MPIUtils.h"

#include "GroupStructs.h"

namespace fea_io
{
	using json = nlohmann::json;

	/// TODO
	class GroupReader
	{
	public:

		GroupReader(const std::string file_path);
		~GroupReader();

		/// TODO
		template<typename IndexType>
		void read_list(
			const int64_t group_id,
			const int64_t partition_id,
			std::vector<IndexType>& indices);

		/// TODO
		template<typename DataType>
		void read(
			const int64_t group_id,
			const int64_t partition_id,
			const int64_t data_array_id,
			std::vector<DataType>& data);

	private:
		std::vector<GroupInfo> group_infos_;
		std::ifstream in_stream_;
		const std::string binary_ext = ".bin";
	};

	inline GroupReader::GroupReader(const std::string file_path)
	{
		in_stream_.open(file_path + binary_ext, std::ios::binary | std::fstream::in);
		std::ifstream json_component(file_path + ".json");
		json j;
		json_component >> j;
		json_component.close();

		// Note: standard container overloads operator= so a simple:
		// partition_infos_ = j["partition_info"];
		// would throw an ambiguity error.
		for (auto& obj : j["groups"]) {
			GroupInfo group_info = obj;
			group_infos_.push_back(group_info);
		}
	}

	inline GroupReader::~GroupReader()
	{
		in_stream_.close();
	}

	template<typename IndexType>
	inline void GroupReader::read_list(const int64_t group_id, const int64_t partition_id, std::vector<IndexType>& indices)
	{
		if (group_id > static_cast<int64_t>(group_infos_.size()) - 1)
			throw _MSG_EXCEPT_C("The partition id was larger than the number of partitions indicated by the JSON file.");
		if (partition_id > static_cast<int64_t>(group_infos_.front().num_objects_in_partitions.size()) - 1)
			throw _MSG_EXCEPT_C("The partition id was larger than the number of partitions indicated by the JSON file.");

		GroupInfo& group_info = group_infos_[group_id];
		in_stream_.seekg(group_info.object_list_offsets[partition_id]);

		const std::size_t num_values = group_info.num_objects_in_partitions[partition_id];
		indices.resize(num_values);
		IOUtils::binary_read(in_stream_, indices, num_values, group_info.index_type_num_bytes);
	}

	template<typename DataType>
	inline void fea_io::GroupReader::read(const int64_t group_id, const int64_t partition_id, const int64_t data_array_id, std::vector<DataType>& data)
	{
		if (group_id > static_cast<int64_t>(group_infos_.size()) - 1)
			throw _MSG_EXCEPT_C("The partition id was larger than the number of partitions indicated by the JSON file.");
		if (partition_id > static_cast<int64_t>(group_infos_.front().num_objects_in_partitions.size()) - 1)
			throw _MSG_EXCEPT_C("The partition id was larger than the number of partitions indicated by the JSON file.");

		GroupInfo& group_info = group_infos_[group_id];
		if (data_array_id > static_cast<int64_t>(group_info.array_infos.size()) - 1)
			throw _MSG_EXCEPT_C("The data array id was larger than the number of data arrays indicated by the JSON file.");

		auto& array_info = group_info.array_infos[data_array_id];
		in_stream_.seekg(array_info.begin_offsets[partition_id]);

		const std::size_t num_values = array_info.num_bytes[partition_id] / array_info.data_type_num_bytes;
		data.resize(num_values);
		IOUtils::binary_read(in_stream_, &data[0], num_values, array_info.data_type_num_bytes);
	}

}
