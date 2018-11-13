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

	class GroupWriter
	{
	public:
		explicit GroupWriter();
		virtual ~GroupWriter();

		/// Initializes the GroupWriter with the information needed to preallocate the file,
		/// calculate the offsets for each group and data array, and opens the file for writing.
		void initialize(
			const std::string file_path,
			const std::vector<GroupInfo> group_infos);

		template<typename IndexType>
		void write_list(
			const std::string group_name,
			const int64_t partition_id,
			const IndexType* object_index_list);

		/// Writes data for a single node/element or data for multiple
		/// nodes/elements with the data arranged as single list of values
		/// Note: the order of the nodes or elements for which this method is called
		///       should match the node/element list provided with write_list call.
		template<typename DataType>
		void write(
			const std::string group_name,
			const int64_t partition_id,
			const std::string array_name,
			const DataType* data,
			const int64_t num_values = 1);

		/// Writes data for multiple nodes/elements with the data arranged as double
		/// list of values (in other words, a matrix)
		/// Note: the order of the nodes or elements for which this method is called
		///       should match the node/element list provided with write_list call.
		template<typename DataType>
		void write(
			const std::string group_name,
			const int64_t partition_id,
			const std::string array_name,
			const DataType** data,
			const int64_t num_values);

		/// Close the binary file, writes the JSON file, and resets the DataWriter object. 
		/// Note: this gets call with the destructor, so you don't have to call it unless
		/// you want to force it to up the files.
		void close();

	protected:
		std::vector<GroupInfo> group_infos_;
		std::map<std::string, std::size_t> group_name_to_id_map_;
		std::vector<std::map<std::string, std::size_t>> array_name_to_id_map_;
		WriteState write_state_;
		std::string file_path_;
		std::ofstream out_stream_;
		const std::string binary_ext = ".bin";

		void verify_data_sizes() const;
	};

	inline GroupWriter::GroupWriter()
	{
		write_state_ = WriteState::Uninitialized;
	}

	inline GroupWriter::~GroupWriter()
	{
		if (write_state_ != WriteState::Finalized)
			close();
	}

	inline void GroupWriter::initialize(const std::string file_path, const std::vector<GroupInfo> group_infos)
	{
		if (write_state_ != WriteState::Uninitialized && write_state_ != WriteState::Finalized)
			throw _MSG_EXCEPT_C("initialize(...) can only be called after construction or close().");

		if (group_infos.empty())
			throw _MSG_EXCEPT_C("You must provide at least one group to write.");

		for (auto& group_info : group_infos_)
			for (ArrayInfo& da_info : group_info.array_infos)
				if (da_info.type != ArrayType::Node && da_info.type != ArrayType::Element)
					throw _MSG_EXCEPT_C("Currently, only ArrayType::Node and ArrayType::Element is supported for data arrays within group output.  Another type of data was given to initialize() for group " + group_info.name + ".");

		file_path_ = file_path;
		group_infos_ = group_infos;
		const std::size_t num_partitions = group_infos_.front().num_objects_in_partitions.size();

		// Set ids
		array_name_to_id_map_.resize(group_infos_.size());
		for (auto i = 0; i < group_infos_.size(); i++) {
			group_infos_[i].id = i;
			group_name_to_id_map_[group_infos_[i].name] = i;
			for (auto j = 0; j < group_infos_.size(); j++) {
				group_infos_[i].array_infos[j].id = j;
				array_name_to_id_map_[i][group_infos_[i].array_infos[j].name] = j;
			}
		}
		// Resize the containers for offsets to have the number of partitions
		for (auto& group_info : group_infos_) {
			group_info.object_list_offsets.resize(num_partitions);
			for (auto& da_info : group_info.array_infos)
				da_info.resize_offsets(num_partitions);
		}
		// Update the offset vectors within the GroupInfo objects
		std::streamoff curr_pos = 0;
		for (GroupInfo& group_info : group_infos_) {
			for (auto i = 0; i < num_partitions; i++) {
				group_info.object_list_offsets[i] = curr_pos;
				curr_pos += group_info.num_objects_in_partitions[i] * group_info.index_type_num_bytes;

				for (auto& da_info : group_info.array_infos) {
					da_info.begin_offsets[i] = curr_pos;
					da_info.num_bytes[i] = group_info.num_objects_in_partitions[i] * da_info.data_type_num_bytes;
					curr_pos += da_info.num_bytes[i];
				}
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

	template<typename IndexType>
	inline void fea_io::GroupWriter::write_list(const std::string group_name, const int64_t partition_id, const IndexType * object_index_list)
	{
		auto find_iter = group_name_to_id_map_.find(group_name);
		if (find_iter == group_name_to_id_map_.end())
			throw _MSG_EXCEPT_C("Group " + group_name + " does not exist in the list of groups indicated by the JSON file.");
		GroupInfo& group_info = group_infos_[find_iter->second];

		out_stream_.seekp(group_info.object_list_offsets[partition_id]);
		IOUtils::binary_write(out_stream_, &object_index_list[0], group_info.num_objects_in_partitions[partition_id]);
	}

	template<typename DataType>
	inline void fea_io::GroupWriter::write(const std::string group_name, const int64_t partition_id, const std::string array_name, const DataType * data, const int64_t num_values)
	{
		auto find_iter = group_name_to_id_map_.find(group_name);
		if (find_iter == group_name_to_id_map_.end())
			throw _MSG_EXCEPT_C("Group " + group_name + " does not exist in the list of groups indicated by the JSON file.");
		GroupInfo& group_info = group_infos_[find_iter->second];

		auto find_iter2 = array_name_to_id_map_[group_info.id].find(array_name);
		if (find_iter2 == array_name_to_id_map_[group_info.id].end())
			throw _MSG_EXCEPT_C("Data array " + array_name + " does not exist in the list of arrays indicated by the JSON file for group " + group_name + ".");
		auto& array_info = group_info.array_infos[find_iter2->second];

		out_stream_.seekp(array_info.begin_offsets[partition_id] + array_info.curr_bytes_written[partition_id]);
		for (auto i = 0; i < num_values; i++)
			IOUtils::binary_write(out_stream_, &data[i*array_info.num_comps], array_info.num_comps);
		array_info.curr_bytes_written[partition_id] += num_values * array_info.num_comps * sizeof(DataType);
	}

	template<typename DataType>
	inline void fea_io::GroupWriter::write(const std::string group_name, const int64_t partition_id, const std::string array_name, const DataType ** data, const int64_t num_values)
	{
		auto find_iter = group_infos_.find(group_name);
		if (find_iter == group_name_to_id_map_.end())
			throw _MSG_EXCEPT_C("Group " + group_name + " does not exist in the list of groups indicated by the JSON file.");
		GroupInfo& group_info = group_infos_[find_iter->second];

		auto find_iter2 = array_name_to_id_map_[group_info.id].find(array_name);
		if (find_iter2 == array_name_to_id_map_[group_info.id].end())
			throw _MSG_EXCEPT_C("Data array " + array_name + " does not exist in the list of arrays indicated by the JSON file for group " + group_name + ".");
		auto& array_info = group_info.array_infos[find_iter2->second];

		out_stream_.seekp(array_info.begin_offsets[partition_id] + array_info.curr_bytes_written[partition_id]);
		for (auto i = 0; i < num_values; i++)
			IOUtils::binary_write(out_stream_, data[i], array_info.num_comps);
		array_info.curr_bytes_written[partition_id] += num_values * array_info.num_comps * sizeof(DataType);
	}

	inline void GroupWriter::close()
	{
		out_stream_.close();

		verify_data_sizes();

		std::ofstream json_component(file_path_ + ".json");
		json j;
		j["groups"] = group_infos_;
		json_component << std::setw(4) << j << std::endl;
		json_component.close();

		write_state_ = WriteState::Finalized;
	}

	inline void GroupWriter::verify_data_sizes() const
	{
		const std::size_t num_partitions = group_infos_.front().num_objects_in_partitions.size();

		for (const GroupInfo& group_info : group_infos_) {
			for (auto& da_info : group_info.array_infos) {
				// For any partition written to on this processor, ensure the size written is as expected
				std::vector<int64_t> partition_written_to_for_this_proc;
				for (auto i = 0; i < num_partitions; i++) {
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

}