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
		void read(const int64_t partition_id, const int64_t data_array_id, std::vector<DataType>& data);

	private:
		std::vector<ArrayInfo> array_infos_;
		std::ifstream in_stream_;
		const std::string binary_ext = ".bin";
	};

	inline DataReader::DataReader(const std::string file_path)
	{
		in_stream_.open(file_path + binary_ext, std::ios::binary | std::fstream::in);
		std::ifstream json_component(file_path + ".json");
		json j;
		json_component >> j;
		json_component.close();

		// Note: standard container overloads operator= so a simple:
		// partition_infos_ = j["partition_info"];
		// would throw an ambiguity error.
		for (auto& obj : j["data_arrays"]) {
			ArrayInfo data_array_info = obj;
			array_infos_.push_back(data_array_info);
		}
	}

	inline DataReader::~DataReader()
	{
		in_stream_.close();
	}

	template<typename DataType>
	inline void DataReader::read(const int64_t partition_id, const int64_t data_array_id, std::vector<DataType>& data)
	{
		if (partition_id > static_cast<int64_t>(array_infos_.front().begin_offsets.size()) - 1)
			throw _MSG_EXCEPT_C("The partition id was larger than the number of partitions indicated by the JSON file.");
		if (data_array_id > static_cast<int64_t>(array_infos_.size()) - 1)
			throw _MSG_EXCEPT_C("The data array id was larger than the number of data arrays indicated by the JSON file.");

		auto& array_info = array_infos_[data_array_id];
		in_stream_.seekg(array_info.begin_offsets[partition_id]);

		const std::size_t num_values = array_info.num_bytes[partition_id] / array_info.data_type_num_bytes;
		data.resize(num_values);
		IOUtils::binary_read(in_stream_, &data[0], num_values, array_info.data_type_num_bytes);
	}

}