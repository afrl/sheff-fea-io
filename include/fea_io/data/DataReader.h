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
	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	class DataReader
	{
	public:
		typedef DataArrayInfo<LocalIndex, CoordType> DataArrayInfoType;

		DataReader(const std::string file_path);
		~DataReader();

		/// TODO
		std::vector<CoordType> read(
			const LocalIndex partition_id,
			const LocalIndex data_array_id);

	private:
		std::vector<DataArrayInfoType> data_array_infos_;
		DataTypeSizes data_types_in_file;
		std::ifstream in_stream_;
		const std::string binary_ext = ".bin";
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline DataReader<GlobalIndex, LocalIndex, CoordType>::DataReader(const std::string file_path)
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
			DataArrayInfoType data_array_info = obj;
			data_array_infos_.push_back(data_array_info);
		}

		// Check data_types
		DataTypeSizes data_types;
		data_types.deduce<GlobalIndex, LocalIndex, CoordType>();
		data_types_in_file = j["types"];
		if (not data_types.is_large_enough(data_types_in_file))
			throw _MSG_EXCEPT_C("The data types used for the MeshReader template are too small for the data in the file being read.");
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline DataReader<GlobalIndex, LocalIndex, CoordType>::~DataReader()
	{
		in_stream_.close();
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline std::vector<CoordType> DataReader<GlobalIndex, LocalIndex, CoordType>::read(const LocalIndex partition_id, const LocalIndex data_array_id)
	{
		if (partition_id > data_array_infos_.front().begin_offsets.size() - 1)
			throw _MSG_EXCEPT_C("The partition id was larger than the number of partitions indicated by the JSON file.");
		if (data_array_id > data_array_infos_.size() - 1)
			throw _MSG_EXCEPT_C("The data array id was larger than the number of data arrays indicated by the JSON file.");

		auto& data_info = data_array_infos_[data_array_id];
		in_stream_.seekg(data_info.begin_offsets[partition_id]);
		const LocalIndex num_values = static_cast<LocalIndex>(data_info.num_bytes[partition_id]) / data_types_in_file.coord_type_size;
		std::vector<CoordType> data(num_values);
		IOUtils::binary_read(in_stream_, &data[0], num_values, data_types_in_file.coord_type_size);
		return data;
	}
}