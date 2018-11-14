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
			const std::string group_name,
			const int64_t partition_id,
			std::vector<IndexType>& indices);

		/// TODO
		template<typename DataType>
		void read(
			const std::string group_name,
			const int64_t partition_id,
			const std::string array_name,
			std::vector<DataType>& data);

	private:
		std::vector<GroupInfo> group_infos_;
		std::map<std::string, std::size_t> group_name_to_id_map_;
		std::vector<std::map<std::string, std::size_t>> array_name_to_id_map_;
		std::ifstream in_stream_;
		const std::string json_ext = ".g.json";
		const std::string binary_ext = ".g.bin";
	};

	inline GroupReader::GroupReader(const std::string file_path)
	{
		in_stream_.open(file_path + binary_ext, std::ios::binary | std::fstream::in);
		std::ifstream json_component(file_path + json_ext);
		json j;
		json_component >> j;
		json_component.close();

		for (auto& obj : j["groups"]) {
			GroupInfo group_info = obj;
			group_infos_.push_back(group_info);
			group_name_to_id_map_[group_info.name] = group_info.id;
			array_name_to_id_map_.push_back(std::map<std::string, std::size_t>());
			for (auto& array_info : group_info.array_infos)
				array_name_to_id_map_.back()[array_info.name] = array_info.id;
		}
	}

	inline GroupReader::~GroupReader()
	{
		in_stream_.close();
	}

	template<typename IndexType>
	inline void GroupReader::read_list(const std::string group_name, const int64_t partition_id, std::vector<IndexType>& indices)
	{
		if (partition_id > static_cast<int64_t>(group_infos_.front().num_objects_in_partitions.size()) - 1)
			throw _MSG_EXCEPT_C("The partition id was larger than the number of partitions indicated by the JSON file.");

		auto find_iter = group_name_to_id_map_.find(group_name);
		if (find_iter == group_name_to_id_map_.end())
			throw _MSG_EXCEPT_C("Group " + group_name + " does not exist in the list of groups indicated by the JSON file.");
		GroupInfo& group_info = group_infos_[find_iter->second];

		in_stream_.seekg(group_info.object_list_offsets[partition_id]);
		const std::size_t num_values = group_info.num_objects_in_partitions[partition_id];
		indices.resize(num_values);
		IOUtils::binary_read(in_stream_, indices, num_values, group_info.index_type_num_bytes);
	}

	template<typename DataType>
	inline void fea_io::GroupReader::read(const std::string group_name, const int64_t partition_id, const std::string array_name, std::vector<DataType>& data)
	{
		if (partition_id > static_cast<int64_t>(group_infos_.front().num_objects_in_partitions.size()) - 1)
			throw _MSG_EXCEPT_C("The partition id was larger than the number of partitions indicated by the JSON file.");

		auto find_iter = group_name_to_id_map_.find(group_name);
		if (find_iter == group_name_to_id_map_.end())
			throw _MSG_EXCEPT_C("Group " + group_name + " does not exist in the list of groups indicated by the JSON file.");
		GroupInfo& group_info = group_infos_[find_iter->second];

		auto find_iter2 = array_name_to_id_map_[group_info.id].find(array_name);
		if (find_iter2 == array_name_to_id_map_[group_info.id].end())
			throw _MSG_EXCEPT_C("Data array " + array_name + " does not exist in the list of arrays indicated by the JSON file for group " + group_name + ".");
		auto& array_info = group_info.array_infos[find_iter2->second];

		in_stream_.seekg(array_info.begin_offsets[partition_id]);
		const std::size_t num_values = array_info.num_bytes[partition_id] / array_info.data_type_num_bytes;
		data.resize(num_values);
		IOUtils::binary_read(in_stream_, &data[0], num_values, array_info.data_type_num_bytes);
	}

}
