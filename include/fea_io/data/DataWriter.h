#pragma once

#include <algorithm>
#include <array>
#include <stdint.h>
#include <vector>
#include <fstream>

#include <nlohmann/json.hpp>

#include "fea_io/common/Common.h"
#include "fea_io/common/Exceptions.h"
#include "fea_io/common/IOUtils.h"
#include "fea_io/common/MPIUtils.h"
#include "fea_io/mesh/MeshInfoStructs.h"
#include "fea_io/mesh/MeshStructs.h"

#include "DataStructs.h"

namespace fea_io
{
	using json = nlohmann::json;

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	class DataWriter
	{
	public:
		typedef MeshMetaInfo<GlobalIndex, LocalIndex, CoordType> MeshMetaInfoType;
		typedef PartitionInfo<GlobalIndex, LocalIndex, CoordType> PartitionInfoType;
		typedef DataArrayInfo<LocalIndex, CoordType> DataArrayInfoType;

		explicit DataWriter();
		virtual ~DataWriter();

		void initialize(
			const std::string file_path,
			const MeshMetaInfoType mesh_info,
			const std::vector<PartitionInfoType> partition_info,
			const std::vector<LocalIndex> num_quad_points_in_partitions,
			const std::vector<DataArrayInfoType> data_array_infos);

		// Writes value for a single node, element, or quad point
		// Note: the order for each partition should match the order in the mesh file.
		//       It is up to the calling program to ensure the order is correct.
		void write(
			const LocalIndex partition_id,
			const LocalIndex data_array_id,
			const LocalIndex num_values,
			const CoordType* data);

		// Writes array
		void write(
			const LocalIndex partition_id,
			const LocalIndex data_array_id,
			const LocalIndex num_values,
			const CoordType** data);

		/// Close the binary file, writes the JSON file, and resets the DataWriter object. 
		/// Note: this gets call with the destructor, so you don't have to call it unless
		/// you want to force it to up the files.
		void close();

	protected:
		MeshMetaInfoType mesh_info_;
		std::vector<PartitionInfoType> partition_infos_;
		std::vector<DataArrayInfoType> data_array_infos_;
		WriteState write_state_;
		std::string file_path_;
		std::ofstream out_stream_;
		const std::string binary_ext = ".bin";

		void verify_data_sizes() const;
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline DataWriter<GlobalIndex, LocalIndex, CoordType>::DataWriter()
	{
		write_state_ = WriteState::Uninitialized;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline DataWriter<GlobalIndex, LocalIndex, CoordType>::~DataWriter()
	{
		if (write_state_ != WriteState::Finalized)
			close();
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void fea_io::DataWriter<GlobalIndex, LocalIndex, CoordType>::initialize(const std::string file_path, const MeshMetaInfoType mesh_info, const std::vector<PartitionInfoType> partition_info, const std::vector<LocalIndex> num_quad_points_in_partitions, const std::vector<DataArrayInfoType> data_array_infos)
	{
		if (write_state_ != WriteState::Uninitialized && write_state_ != WriteState::Finalized)
			throw _MSG_EXCEPT_C("initialize(...) can only be called after construction or close().");

		if (mesh_info.total_num_partitions != partition_info.size())
			throw _MSG_EXCEPT_C("The number of PartitionInfo entries provided do not match the number of partitions expected.  If using MPI, each process must pass the full list of PartitionInfo entries (even for those that the process are not responsible for). You can use MeshReader::get_partition_info to get the full list.");

		file_path_ = file_path;
		mesh_info_ = mesh_info;
		partition_infos_ = partition_info;
		data_array_infos_ = data_array_infos;
		write_state_ = WriteState::WritePhase;

		// Update data_array_infos_ offsets and sizes vectors
		// Compute the size needed for each partition, and save the size required for each partition within
		// an ordered vector (this means that the partitions will ordered by index within the binary file)
		std::streamoff curr_pos = 0;
		for (auto& da_info : data_array_infos_) {
			da_info.begin_offsets.resize(partition_infos_.size(), 0);
			da_info.curr_bytes_written.resize(partition_infos_.size(), 0);
			da_info.num_bytes.resize(partition_infos_.size(), 0);
		}
		for (auto i = 0; i < partition_infos_.size(); i++) {
			for (auto& da_info : data_array_infos_) {
				da_info.begin_offsets[i] = curr_pos;
				da_info.num_bytes[i] = da_info.get_size_needed(partition_infos_[i], num_quad_points_in_partitions[i]);
				curr_pos += da_info.num_bytes[i];
			}
		}
		std::streamoff size_needed = curr_pos;

		if (MPIUtils::get_local_rank() == 0) {
			// For now, zeroes will be written to a file to preallocate it.
			// the OS should have a way to preallocate a non-fragmented file
			// without writing the data, but it is not clear how to do it in a
			// portable way.  So for now...the dumb method will suffice.
			std::ofstream out_stream(file_path_ + binary_ext, std::ios::binary);
			const int buffer_size = 1024 * 1024; // 1 MB buffer
			std::vector<char> zero_buffer(buffer_size, 0);
			std::streamoff num_bytes_left = size_needed;
			while (num_bytes_left != 0) {
				if (num_bytes_left < buffer_size) {
					out_stream.write(&zero_buffer[0], num_bytes_left);
					num_bytes_left = 0;
				}
				else {
					out_stream.write(&zero_buffer[0], buffer_size);
					num_bytes_left -= buffer_size;
				}
			}
			out_stream.close();
		}
		// Be sure the root rank finishes writing the file before opening it
		MPIUtils::barrier();

		// Everyone needs to open the file in in/out mode to be able to seek to different parts and write at will.
		// Note: append mode will not let you go back before the end.
		out_stream_.open(file_path_ + binary_ext, std::ios::binary | std::fstream::in | std::fstream::out);
		out_stream_.seekp(0, std::ios::beg);

		write_state_ = WriteState::WritePhase;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void DataWriter<GlobalIndex, LocalIndex, CoordType>::write(const LocalIndex partition_id, const LocalIndex data_array_id, const LocalIndex num_values, const CoordType * data)
	{
		auto& data_info = data_array_infos_[data_array_id];
		out_stream_.seekp(data_info.begin_offsets[partition_id] + data_info.curr_bytes_written[partition_id]);
		for (auto i = 0; i < num_values; i++)
			IOUtils::binary_write(out_stream_, &data[i*data_info.num_comps], data_info.num_comps);
		data_info.curr_bytes_written[partition_id] += num_values * data_info.num_comps * sizeof(CoordType);
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void DataWriter<GlobalIndex, LocalIndex, CoordType>::write(const LocalIndex partition_id, const LocalIndex data_array_id, const LocalIndex num_values, const CoordType ** data)
	{
		auto& data_info = data_array_infos_[data_array_id];
		out_stream_.seekp(data_info.begin_offsets[partition_id] + data_info.curr_bytes_written[partition_id]);
		for (auto i = 0; i < num_values; i++)
			IOUtils::binary_write(out_stream_, data[i], data_info.num_comps);
		data_info.curr_bytes_written[partition_id] += num_values * data_info.num_comps * sizeof(CoordType);
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void DataWriter<GlobalIndex, LocalIndex, CoordType>::close()
	{
		out_stream_.close();

		verify_data_sizes();

		std::ofstream json_component(file_path_ + ".json");
		json j;
		j["data_arrays"] = data_array_infos_;
		DataTypeSizes data_types;
		data_types.deduce<GlobalIndex, LocalIndex, CoordType>();
		j["types"] = data_types;
		json_component << std::setw(4) << j << std::endl;
		json_component.close();

		write_state_ = WriteState::Finalized;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void DataWriter<GlobalIndex, LocalIndex, CoordType>::verify_data_sizes() const
	{
		for (auto& da_info : data_array_infos_) {
			// For any partition written to on this processor, ensure the size written is as expected
			std::vector<LocalIndex> partition_written_to_for_this_proc;
			for (auto i = 0; i < partition_infos_.size(); i++) {
				const std::streamoff bytes_written = da_info.curr_bytes_written[i];
				if (bytes_written > 0 && bytes_written != da_info.num_bytes[i])
					throw _MSG_EXCEPT_C("The number of bytes written for data array " + da_info.name + " did not match the size expected. Bytes written = " + std::to_string(bytes_written) + " but bytes expected = " + std::to_string(da_info.num_bytes[i]) + ".");
				partition_written_to_for_this_proc.push_back(i);
			}

			// Ensure each partition was only written to once across all the processors
			auto all_partitions_written_to = MPIUtils::gather_vectors(partition_written_to_for_this_proc);
			if (MPIUtils::get_local_rank() == 0) {
				std::sort(all_partitions_written_to.begin(), all_partitions_written_to.end());
				if (all_partitions_written_to[0] != 0)
					throw _MSG_EXCEPT_C("Partition 0 was not written to.");
				for (auto i = 0; i < all_partitions_written_to.size() - 1; i++) {
					if (all_partitions_written_to[i + 1] == all_partitions_written_to[i])
						throw _MSG_EXCEPT_C("Partition " + std::to_string(all_partitions_written_to[i]) + " was written to twice.");
					if (all_partitions_written_to[i + 1] != all_partitions_written_to[i] + 1)
						throw _MSG_EXCEPT_C("Partition " + std::to_string(all_partitions_written_to[i] + 1) + " was not written to.");
				}
			}
		}
	}
}