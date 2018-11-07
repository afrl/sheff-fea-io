#pragma once

#include <fstream>

#include "fea_io/common/Exceptions.h"
#include "fea_io/common/IOUtils.h"

#include "MeshInfoStructs.h"
#include "MeshStructs.h"

namespace fea_io
{
	/// A simple class for reading a mesh written with MeshWriter or MeshParWriter
	/// Unlike writing, reading is embarrassingly parallel.  This class is thread
	/// safe, so just create an instance for each process/thread that reads from
	/// the file.  However, if enough processors are reading from the file with a
	/// magnetic hard-disk, the frequent seeking may lower performance. RAID arrays
	/// or SSDs reduce the concern, but be aware of the possible issue with large analyses.
	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	class MeshReader
	{
	public:
		typedef MeshMetaInfo<GlobalIndex, LocalIndex, CoordType> MeshMetaInfoType;
		typedef PartitionInfo<GlobalIndex, LocalIndex, CoordType> PartitionInfoType;
		typedef Partition<GlobalIndex, LocalIndex, CoordType> PartitionType;

		MeshReader(const std::string file_path);
		~MeshReader();

		/// Read a particular partition from the file
		PartitionType read_partition(const LocalIndex& partition_id);

		const std::vector<PartitionInfoType>& get_partition_info() const { return partition_infos_; };

	private:
		MeshMetaInfoType meta_info_;
		std::vector<PartitionInfoType> partition_infos_;
		DataTypeSizes data_types_in_file;
		std::ifstream in_stream_;
		const std::string binary_ext = ".bin";
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline MeshReader<GlobalIndex, LocalIndex, CoordType>::MeshReader(const std::string file_path)
	{
		in_stream_.open(file_path + binary_ext, std::ios::binary | std::fstream::in);
		std::ifstream json_component(file_path + ".json");
		json j;
		json_component >> j;
		json_component.close();

		meta_info_ = j["meta_info"];
		// Note: standard container overloads operator= so a simple:
		// partition_infos_ = j["partition_info"];
		// would throw an ambiguity error.
		for (auto& obj : j["partition_info"]) {
			PartitionInfoType part_info = obj;
			partition_infos_.push_back(part_info);
		}

		// Check data_types
		DataTypeSizes data_types;
		data_types.deduce<GlobalIndex, LocalIndex, CoordType>();
		data_types_in_file = j["types"];
		if (not data_types.is_large_enough(data_types_in_file))
			throw _MSG_EXCEPT_C("The data types used for the MeshReader template are too small for the data in the file being read.");
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline MeshReader<GlobalIndex, LocalIndex, CoordType>::~MeshReader()
	{
		in_stream_.close();
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline Partition<GlobalIndex, LocalIndex, CoordType> MeshReader<GlobalIndex, LocalIndex, CoordType>::read_partition(const LocalIndex & partition_id)
	{
		if (partition_id > meta_info_.partition_offsets.size() - 1)
			throw _MSG_EXCEPT_C("The partition id was larger than the number of partitions indicated by the JSON file.");

		in_stream_.seekg(meta_info_.partition_offsets[partition_id]);
		Partition<GlobalIndex, LocalIndex, CoordType> partition;

		// Read high-level information
		LocalIndex num_nodes = partition_infos_[partition_id].num_nodes;
		LocalIndex num_elements = partition_infos_[partition_id].num_elements;
		IOUtils::binary_read(in_stream_, partition.partition_id, data_types_in_file.local_index_size);
		if (partition_id != partition.partition_id)
			throw _MSG_EXCEPT_C("The partition id in the file did not match the one requested, indicating a corrupted lookup table.  Perhaps the JSON file was changed externally.");
		// Read node information
		partition.node_global_ids.resize(num_nodes);
		partition.node_parent_partitions.resize(num_nodes);
		partition.node_coords.resize(num_nodes * meta_info_.dimensionality);
		for (auto i = 0; i < num_nodes; i++) {
			IOUtils::binary_read(in_stream_, partition.node_global_ids[i], data_types_in_file.global_index_size);
			IOUtils::binary_read(in_stream_, partition.node_parent_partitions[i], data_types_in_file.local_index_size);
			IOUtils::binary_read(in_stream_, &partition.node_coords[meta_info_.dimensionality*i], meta_info_.dimensionality, data_types_in_file.coord_type_size);
		}
		// Read element information
		partition.element_global_ids.resize(num_elements);
		partition.element_parent_partitions.resize(num_elements);
		partition.element_types.resize(num_elements);
		partition.element_connectivities.resize(num_elements);
		for (auto i = 0; i < num_elements; i++) {
			IOUtils::binary_read(in_stream_, partition.element_global_ids[i], data_types_in_file.global_index_size);
			IOUtils::binary_read(in_stream_, partition.element_parent_partitions[i], data_types_in_file.local_index_size);
			std::underlying_type<ElementType>::type element_type;
			IOUtils::binary_read(in_stream_, element_type);
			partition.element_types[i] = static_cast<ElementType>(element_type);
			auto num_nodes_for_elem = get_num_nodes_for_type(partition.element_types[i]);
			IOUtils::binary_read(in_stream_, partition.element_connectivities[i], num_nodes_for_elem, data_types_in_file.local_index_size);
		}
		// Read subcomponent information
		LocalIndex num_subcomponents = partition_infos_[partition_id].num_subcomponents;
		for (auto i = 0; i < num_subcomponents; i++) {
			Subcomponent<LocalIndex, CoordType> subcomponent;
			LocalIndex num_nodes_in_subcomponent;
			LocalIndex num_elements_in_subcomponent;
			LocalIndex num_lcs_params_in_subcomponent;

			IOUtils::binary_read(in_stream_, subcomponent.component_id, data_types_in_file.local_index_size);
			IOUtils::binary_read(in_stream_, num_nodes_in_subcomponent, data_types_in_file.local_index_size);
			IOUtils::binary_read(in_stream_, subcomponent.nodes_in_subcomponent, num_nodes_in_subcomponent, data_types_in_file.local_index_size);
			IOUtils::binary_read(in_stream_, num_elements_in_subcomponent, data_types_in_file.local_index_size);
			IOUtils::binary_read(in_stream_, subcomponent.elements_in_subcomponent, num_elements_in_subcomponent, data_types_in_file.local_index_size);
			IOUtils::binary_read(in_stream_, num_lcs_params_in_subcomponent, data_types_in_file.local_index_size);
			IOUtils::binary_read(in_stream_, subcomponent.lcs_parameters_in_subcomponent, num_lcs_params_in_subcomponent, data_types_in_file.coord_type_size);

			partition.subcomponents.push_back(subcomponent);
		}


		return partition;
	}
}