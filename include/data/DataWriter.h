#pragma once

#include <algorithm>
#include <array>
#include <stdint.h>
#include <vector>
#include <fstream>

#include <nlohmann/json.hpp>

#include "Common.h"
#include "MeshStructs.h"
#include "IOUtils.h"
#include "Exceptions.h"

#ifdef HAVE_MPI
#include "MPIUtils.h"
#endif

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
		LocalIndex id;
		DataArrayType type;
		LocalIndex num_comps;
		std::string name;
		std::vector<std::string> comp_names;
		std::vector<std::streamoff> data_array_offsets;

		template<typename PartitionInfo>
		std::streamoff get_size_needed(const PartitionInfo& pi)
		{
			if (type == DataArrayType::Node)
				return pi.num_nodes * num_comps * sizeof(CoordType);
			else if (type == DataArrayType::Element)
				return pi.num_elements * num_comps * sizeof(CoordType);
			else if (type == DataArrayType::NodeByElement)
				return pi.total_num_connectivity_entries * num_comps * sizeof(CoordType);
			else if (type == DataArrayType::NodeByComponent) {
				std::streamoff size_needed;
				for (auto& subcomponent_info : pi.subcomponents)
					size_needed += subcomponent_info.num_nodes_in_subcomponent * num_comps * sizeof(CoordType);
				return size_needed;
			}
			else if (type == DataArrayType::QuadPointByElement)
				return total_num_quad_points * num_elements * sizeof(CoordType);
			
		}
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	class DataWriter
	{
	public:
		typedef MeshMetaInfo<GlobalIndex, LocalIndex, CoordType> MeshMetaInfo;
		typedef PartitionInfo<GlobalIndex, LocalIndex, CoordType> PartitionInfo;
		typedef DataArrayInfo<LocalIndex, CoordType> DataArrayInfo;

		explicit DataWriter();
		virtual ~DataWriter();

		void initialize(
			const std::string file_path,
			const MeshMetaInfo mesh_info,
			const std::vector<PartitionInfo> partition_info,
			const std::vector<LocalIndex> num_quad_points_in_partitions,
			const std::vector<DataArrayInfo> data_array_infos);

		// Writes value
		void write(
			const LocalIndex partition_id,
			const LocalIndex data_array_id,
			const CoordType* data);

		// Writes array
		void write_by_element(
			const LocalIndex partition_id,
			const LocalIndex data_array_id,
			const LocalIndex num_values,
			const CoordType** data);

		// Writes array
		void write_by_component(
			const LocalIndex partition_id,
			const LocalIndex data_array_id,
			const LocalIndex num_values,
			const CoordType** data);

		
		/// Close the binary file, writes the JSON file, and resets the DataWriter object. 
		/// Note: this gets call with the destructor, so you don't have to call it unless
		/// you want to force it to up the files.
		void close();

	protected:
		MeshMetaInfo mesh_info_;
		std::vector<PartitionInfo> partition_infos_;
		std::vector<DataArrayInfo> data_array_infos_;
		std::vector<std::vector<std::streamoff>> data_array_write_positions_;
		WriteState write_state_;
		std::string file_path_;
		std::ofstream out_stream_;
		const std::string binary_ext = ".bin";

		DataArrayInfo& get_data_array_info(const data_array_id);
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
	inline void fea_io::DataWriter<GlobalIndex, LocalIndex, CoordType>::initialize(const std::string file_path, const MeshMetaInfo mesh_info, const std::vector<PartitionInfo> partition_info, const std::vector<LocalIndex> num_quad_points_in_partitions, const std::vector<DataArrayInfo> data_array_infos)
	{
		if (write_state_ != WriteState::Uninitialized && write_state_ != WriteState::Finalized)
			throw _MSG_EXCEPT_C("initialize(...) can only be called after construction or close().");

		file_path_ = file_path;
		mesh_info_ = mesh_info;
		partition_infos_ = partition_info;
		data_array_infos_ = data_array_infos;
		write_state_ = WriteState::WritePhase;

		// Compute the size needed for each partition, and save the size required for each partition within
		// an ordered vector (this means that the partitions will ordered by index within the binary file)
		std::streamoff curr_pos = 0;
		for (auto& da_info : data_array_infos_)
			da_info.data_array_offsets.resize(partition_infos_.size());
		for (auto i = 0; i < partition_infos_.size(); i++) {
			for (auto& da_info : data_array_infos_) {
				da_info.data_array_offsets[i] = curr_pos;
				curr_pos += da_info.get_size_needed(partition_infos_[i], num_quad_points_in_partitions[i]);
			}
		}
		std::streamoff size_needed = curr_pos;

		if (MPIUtils::get_local_rank() == 0) {
			// For now, zeroes will be written to a file to preallocate it.
			// the OS should have a way to preallocate a non-fragmented file
			// without writing the data, but it is not clear how to do it in a
			// portable way.  So for now...the dumb method will suffice.
			std::ofstream out_stream(Parent::file_path_ + Parent::binary_ext, std::ios::binary);
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

		// Everyone needs to open the file in in/out mode to be able to seek to different parts and write at will.
		// Note: append mode will not let you go back before the end.
		out_stream_.open(file_path_ + binary_ext, std::ios::binary | std::fstream::in | std::fstream::out);
		out_stream_.seekp(0, std::ios::beg);

		write_state_ = WriteState::WritePhase;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void DataWriter<GlobalIndex, LocalIndex, CoordType>::write(const LocalIndex partition_id, const LocalIndex data_array_id, const CoordType * data)
	{
		auto& data_info = data_array_infos_[data_array_id];
		if (data_info.type == DataArrayType::Node || data_info.type == DataArrayType::Element)
			throw _MSG_EXCEPT_C("The write(...) method can only be used to write node or element data (see write_by_element and write_by_component).");

		out_stream_.seekp(data_array_write_positions_[partition_id][data_array_id]);
		IOUtils::binary_write(out_stream_, data, data_info.num_comps);
		data_array_write_positions_[partition_id][data_array_id] + data_info.num_comps * sizeof(CoordType);
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void DataWriter<GlobalIndex, LocalIndex, CoordType>::write_by_element(const LocalIndex partition_id, const LocalIndex data_array_id, const LocalIndex num_values, const CoordType ** data)
	{
		auto& data_info = data_array_infos_[data_array_id];

		out_stream_.seekp(data_array_write_positions_[partition_id][data_array_id]);
		for (auto i = 0; i < num_values; i++)
			IOUtils::binary_write(out_stream_, data[i], data_info.num_comps);
		data_array_write_positions_[partition_id][data_array_id] + num_values * data_info.num_comps * sizeof(CoordType);
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void DataWriter<GlobalIndex, LocalIndex, CoordType>::write_by_component(const LocalIndex partition_id, const LocalIndex data_array_id, const LocalIndex num_values, const CoordType ** data)
	{
		auto& data_info = data_array_infos_[data_array_id];

		out_stream_.seekp(data_array_write_positions_[partition_id][data_array_id]);
		for (auto i = 0; i < num_values; i++)
			IOUtils::binary_write(out_stream_, data[i], data_info.num_comps);
		data_array_write_positions_[partition_id][data_array_id] + num_values * data_info.num_comps * sizeof(CoordType);
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void DataWriter<GlobalIndex, LocalIndex, CoordType>::close()
	{
		out_stream_.close();

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
}