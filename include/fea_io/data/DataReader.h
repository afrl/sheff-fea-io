#pragma once

#include <fstream>
#include <string>

#include "fea_io/common/Common.h"
#include "fea_io/common/Exceptions.h"
#include "fea_io/common/IOUtils.h"
#include "fea_io/common/MPIUtils.h"
#include "fea_io/mesh/MeshInfoStructs.h"
#include "fea_io/mesh/MeshStructs.h"

#include "DataStructs.h"

namespace fea_io
{
	/// TODO
	class DataReader
	{
	public:

		DataReader(const std::string file_path);
		~DataReader();

		/// TODO
		template<typename DataType>
		void read(const int64_t partition_id, const std::string array_name, std::vector<DataType>& data);

	private:
		std::vector<ArrayInfo> array_infos_;
		std::map<std::string, std::size_t> array_name_to_id_map_;
		std::ifstream in_stream_;
		const std::string json_ext = ".d.json";
		const std::string binary_ext = ".d.bin";
	};

	inline DataReader::DataReader(const std::string file_path)
	{
		in_stream_.open(file_path + binary_ext, std::ios::binary | std::fstream::in);
		std::ifstream json_component(file_path + json_ext);
		json j;
		json_component >> j;
		json_component.close();

		// Note: standard container overloads operator= so a simple:
		// partition_infos_ = j["partition_info"];
		// would throw an ambiguity error.
		for (auto& obj : j["data_arrays"]) {
			ArrayInfo array_info = obj;
			array_infos_.push_back(array_info);
			array_name_to_id_map_[array_info.name] = array_info.id;
		}
	}

	inline DataReader::~DataReader()
	{
		in_stream_.close();
	}

	template<typename DataType>
	inline void fea_io::DataReader::read(const int64_t partition_id, const std::string array_name, std::vector<DataType>& data)
	{
		if (partition_id > static_cast<int64_t>(array_infos_.front().begin_offsets.size()) - 1)
			throw _MSG_EXCEPT_C("The partition id was larger than the number of partitions indicated by the JSON file.");
		auto find_iter = array_name_to_id_map_.find(array_name);
		if (find_iter == array_name_to_id_map_.end())
			throw _MSG_EXCEPT_C("Data array " + array_name + " does not exist in the list of arrays indicated by the JSON file.");

		auto& array_info = array_infos_[find_iter->second];
		in_stream_.seekg(array_info.begin_offsets[partition_id]);

		const std::size_t num_values = array_info.num_bytes[partition_id] / array_info.data_type_num_bytes;
		data.resize(num_values);
		IOUtils::binary_read(in_stream_, &data[0], num_values, array_info.data_type_num_bytes);
	}

}