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
			const std::vector<GroupInfo>& group_infos);

		template<typename IndexType>
		void write_list(
			const std::string group_name,
			const int64_t partition_id,
			const IndexType n_objects,
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
		const std::string json_ext = ".g.json";
		const std::string binary_ext = ".g.bin";
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

	inline void GroupWriter::initialize(const std::string file_path, const std::vector<GroupInfo>& group_infos)
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

		// Set ids
		array_name_to_id_map_.resize(group_infos_.size());
		for (auto i = 0; i < group_infos_.size(); i++) {
			group_infos_[i].id = i;
			group_name_to_id_map_[group_infos_[i].name] = i;
			for (auto j = 0; j < group_infos_[i].array_infos.size(); j++) {
				group_infos_[i].array_infos[j].id = j;
				array_name_to_id_map_[i][group_infos_[i].array_infos[j].name] = j;
			}
		}

		out_stream_.open(file_path_ + binary_ext, std::ios::binary | std::fstream::out);
		out_stream_.seekp(0, std::ios::beg);

		write_state_ = WriteState::WritePhase;
	}

	template<typename IndexType>
	inline void GroupWriter::write_list(
		const std::string group_name,
		const int64_t partition_id,
		const IndexType n_objects,
		const IndexType* object_index_list)
	{
		auto find_iter = group_name_to_id_map_.find(group_name);
		if (find_iter == group_name_to_id_map_.end())
		{
			throw _MSG_EXCEPT_C("Group " + group_name + " does not exist in the list of groups indicated by the JSON file.");
		}
		GroupInfo& group_info = group_infos_[find_iter->second];

		// Make sure the partitions are written sequentially.
		assert(group_info.object_list_offsets.size() == partition_id);
		assert(group_info.num_objects_in_partitions.size() == partition_id);

		group_info.object_list_offsets.push_back(out_stream_.tellp());
		group_info.num_objects_in_partitions.push_back(n_objects);
		IOUtils::binary_write(out_stream_, &object_index_list[0], n_objects);
	}

	template<typename DataType>
	inline void fea_io::GroupWriter::write(
		const std::string group_name, 
		const int64_t partition_id, 
		const std::string array_name, 
		const DataType * data, 
		const int64_t num_values)
	{
		auto find_iter = group_name_to_id_map_.find(group_name);
		if (find_iter == group_name_to_id_map_.end())
			throw _MSG_EXCEPT_C("Group " + group_name + " does not exist in the list of groups indicated by the JSON file.");
		GroupInfo& group_info = group_infos_[find_iter->second];

		auto find_iter2 = array_name_to_id_map_[group_info.id].find(array_name);
		if (find_iter2 == array_name_to_id_map_[group_info.id].end())
			throw _MSG_EXCEPT_C("Data array " + array_name + " does not exist in the list of arrays indicated by the JSON file for group " + group_name + ".");
		auto& array_info = group_info.array_infos[find_iter2->second];

		// Make sure the partitions are written sequentially.
		assert(array_info.begin_offsets.size() == partition_id);
		assert(array_info.num_bytes.size() == partition_id);
		assert(array_info.curr_bytes_written.size() == partition_id);

		array_info.begin_offsets.push_back(out_stream_.tellp());
		array_info.num_bytes.push_back(num_values * array_info.num_comps * sizeof(DataType));
		array_info.curr_bytes_written.push_back(num_values * array_info.num_comps * sizeof(DataType));

		for (auto i = 0; i < num_values; i++)
		{
			IOUtils::binary_write(out_stream_, &data[i * array_info.num_comps], array_info.num_comps);
		}
	}

	template<typename DataType>
	inline void fea_io::GroupWriter::write(const std::string group_name, const int64_t partition_id, const std::string array_name, const DataType ** data, const int64_t num_values)
	{
		auto find_iter = group_name_to_id_map_.find(group_name);
		if (find_iter == group_name_to_id_map_.end())
			throw _MSG_EXCEPT_C("Group " + group_name + " does not exist in the list of groups indicated by the JSON file.");
		GroupInfo& group_info = group_infos_[find_iter->second];

		auto find_iter2 = array_name_to_id_map_[group_info.id].find(array_name);
		if (find_iter2 == array_name_to_id_map_[group_info.id].end())
			throw _MSG_EXCEPT_C("Data array " + array_name + " does not exist in the list of arrays indicated by the JSON file for group " + group_name + ".");
		auto& array_info = group_info.array_infos[find_iter2->second];

		// Make sure the partitions are written sequentially.
		assert(array_info.begin_offsets.size() == partition_id);
		assert(array_info.num_bytes.size() == partition_id);
		assert(array_info.curr_bytes_written.size() == partition_id);

		array_info.begin_offsets.push_back(out_stream_.tellp());
		array_info.num_bytes.push_back(num_values * array_info.num_comps * sizeof(DataType));
		array_info.curr_bytes_written.push_back(num_values * array_info.num_comps * sizeof(DataType));

		for (auto i = 0; i < num_values; i++)
		{
			IOUtils::binary_write(out_stream_, data[i], array_info.num_comps);
		}
	}

	inline void GroupWriter::close()
	{
		out_stream_.close();

		std::ofstream json_component(file_path_ + json_ext);
		json j;
		j["groups"] = group_infos_;
		json_component << std::setw(4) << j << std::endl;
		json_component.close();

		write_state_ = WriteState::Finalized;
	}
}