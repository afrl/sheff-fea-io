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

namespace fea_io
{
	using json = nlohmann::json;

	/// A class for writing a two-part mesh file(s) composed of a JSON and binary file.
	/// The class supports serial and parallel I/O through the same interface.
	/// For example, to write in serial:
	/// \code{cpp}
	/// MeshWriter<int64_t, int32_t, double> writer(...);
	///	writer.write_partition(...);
	/// // ... or if there is only one partition, then:
	/// writer.write(...);
	/// \endcode
	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	class MeshWriter
	{
	public:
		typedef MeshMetaInfo<GlobalIndex, LocalIndex, CoordType> MetaInfo;

		explicit MeshWriter();
		virtual ~MeshWriter();

		/// Some notes on the paramters:
		/// - file_path: leave off the extension but provide path and file name
		/// - dimensionality: indicates if 1D, 2D, or 3D (1, 2, or 3 respectively)
		/// - extent: 3D bounding box of entire mesh (helps with post-processing scripts)
		/// - component_ids must match size of component_names.  Each entry in each vector
		///   indicates the ID and name of a constitutent respectively, i.e. ordering matters.
		void initialize(
			const std::string file_path,
			const GlobalIndex total_num_nodes,
			const GlobalIndex total_num_elements,
			const LocalIndex dimensionality,
			const Extent<CoordType> extent,
			const std::vector<LocalIndex> component_ids,
			const std::vector<std::string> component_names,
			const std::vector<LCSAssignment> component_lcs_assignments,
			const std::vector<LCSParameterization> component_lcs_parameterizations,
			const LocalIndex total_num_partitions = 0);

		void initialize(
			const std::string file_path,
			const MetaInfo meta_info);

		/// Write the mesh if there is only one partition (will assume it is partition 0 under the hood).
		void write(
			const std::vector<GlobalIndex> node_global_ids,
			const std::vector<CoordType> node_coords,
			const std::vector<GlobalIndex> element_global_ids,
			const std::vector<ElementType> element_types,
			const std::vector<std::vector<LocalIndex>> element_connectivities,
			const std::vector<Subcomponent<LocalIndex, CoordType>> subcomponents);

		/// Write a partition to the mesh.
		/// A few notes about the Partition struct:
		/// - For serial mode, partition_id must be given as an integer that is
		///   monotonically increasing (0, 1, 2, 3, etc.).
		/// - node_global_inds, node_parent_partitions, and node_coords must be the same size
		/// - element_global_inds, element_parent_partitions, element_types, and element_connectivities
		///   must be the same size
		virtual void write_partition(const Partition<GlobalIndex, LocalIndex, CoordType>& partition);

		/// Close the binary file, writes the JSON file, and resets the MeshParWriter object. 
		/// Note: this gets call with the destructor, so you don't have to call it unless
		/// you want to force it to up the files.
		void close();

	protected:
		MeshMetaInfo<GlobalIndex, LocalIndex, CoordType> meta_info_;
		std::vector<PartitionInfo<GlobalIndex, LocalIndex, CoordType>> partition_infos_;
		WriteState write_state_;
		std::string file_path_;
		std::ofstream out_stream_;
		const std::string binary_ext = ".bin";

		/// The underlying method for writing the mesh partition.  It is called by write_partition
		/// but it lacks many checks of the data, leaving the checks for write_partition.
		/// Returns the number of bytes written
		std::streamoff write_partition_kernel(const Partition<GlobalIndex, LocalIndex, CoordType>& partition);

		/// Calculate the bounding box (extent) of the node coordinates
		Extent<CoordType> calculate_extent(const std::vector<CoordType> node_coords, const LocalIndex dimensionality);


		/// A helper method called by constructors
		virtual void initialize_state();
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline MeshWriter<GlobalIndex, LocalIndex, CoordType>::MeshWriter()
	{
		write_state_ = WriteState::Uninitialized;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline MeshWriter<GlobalIndex, LocalIndex, CoordType>::~MeshWriter()
	{
		if (write_state_ != WriteState::Finalized)
			close();
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::initialize(const std::string file_path, const GlobalIndex total_num_nodes, const GlobalIndex total_num_elements, const LocalIndex dimensionality, const Extent<CoordType> extent, const std::vector<LocalIndex> component_ids, const std::vector<std::string> component_names, const std::vector<LCSAssignment> component_lcs_assignments, const std::vector<LCSParameterization> component_lcs_parameterizations, const LocalIndex total_num_partitions)
	{
		if (write_state_ != WriteState::Uninitialized && write_state_ != WriteState::Finalized)
			throw _MSG_EXCEPT_C("initialize(...) can only be called after construction or close().");

		if ((component_ids.size() != component_names.size()) || (component_ids.size() != component_lcs_assignments.size())
			|| (component_ids.size() != component_lcs_parameterizations.size()))
			throw _MSG_EXCEPT_C("The vectors passed for component_ids and component_names do not match.");

		meta_info_.total_num_nodes = total_num_nodes;
		meta_info_.total_num_elements = total_num_elements;
		meta_info_.dimensionality = dimensionality;
		meta_info_.extent = extent;
		for (auto i = 0; i < static_cast<int>(component_ids.size()); i++) {
			meta_info_.components.push_back({ component_ids[i], component_names[i],
				component_lcs_assignments[i], component_lcs_parameterizations[i] });
		}
		meta_info_.total_num_partitions = total_num_partitions;
		file_path_ = file_path;
		initialize_state();
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::initialize(const std::string file_path, const MetaInfo meta_info)
	{
		if (write_state_ != WriteState::Uninitialized && write_state_ != WriteState::Finalized)
			throw _MSG_EXCEPT_C("initialize(...) can only be called after construction or close().");

		meta_info_ = meta_info;
		file_path_ = file_path;
		initialize_state();
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::write(const std::vector<GlobalIndex> node_global_ids, const std::vector<CoordType> node_coords, const std::vector<GlobalIndex> element_global_ids, const std::vector<ElementType> element_types, const std::vector<std::vector<LocalIndex>> element_connectivities, const std::vector<Subcomponent<LocalIndex, CoordType>> subcomponents)
	{
		Partition<GlobalIndex, LocalIndex, CoordType> partition;
		partition.partition_id = 0;
		partition.node_global_ids = node_global_ids;
		partition.node_parent_partitions.resize(node_global_ids.size(), 0);
		partition.node_coords = node_coords;
		partition.element_global_ids = element_global_ids;
		partition.element_parent_partitions.resize(element_global_ids.size(), 0);
		partition.element_types = element_types;
		partition.element_connectivities = element_connectivities;
		partition.subcomponents = subcomponents;
		write_partition(partition);
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::write_partition(const Partition<GlobalIndex, LocalIndex, CoordType>& partition)
	{
		if (write_state_ != WriteState::WritePhase)
			throw _MSG_EXCEPT_C("Attempted to write a partition while not in write phase.  Be sure the writer is initialized and close() has not be closed.");

		if (meta_info_.partition_offsets.size() != partition.partition_id)
			throw _MSG_EXCEPT_C("During a serial write, the partitions were attempted to be written non-sequentially. Partition " + std::to_string(meta_info_.partition_offsets.size()) + " expected, but " + std::to_string(partition.partition_id) + " given.");

		meta_info_.partition_offsets.push_back(out_stream_.tellp());

		auto actual_size = write_partition_kernel(partition);
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::close()
	{
		out_stream_.close();

		std::ofstream json_component(file_path_ + ".json");
		json j;
		j["meta_info"] = meta_info_;
		DataTypeSizes data_types;
		data_types.deduce<GlobalIndex, LocalIndex, CoordType>();
		j["types"] = data_types;
		json_component << std::setw(4) << j << std::endl;
		json_component.close();

		write_state_ = WriteState::Finalized;
	}


	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline std::streamoff MeshWriter<GlobalIndex, LocalIndex, CoordType>::write_partition_kernel(const Partition<GlobalIndex, LocalIndex, CoordType>& partition)
	{
		// Check to make sure the node vectors are the right size
		if (partition.node_global_ids.size() != partition.node_parent_partitions.size())
			throw _MSG_EXCEPT_C("The node parent partitions given for partition " + std::to_string(partition.partition_id) + " do not match the size expected.");
		if (partition.node_global_ids.size() * meta_info_.dimensionality != partition.node_coords.size())
			throw _MSG_EXCEPT_C("The node coordinates given for partition " + std::to_string(partition.partition_id) + " do not match the size expected.");
		// Check to make sure the element vectors are the right size
		if (partition.element_global_ids.size() != partition.element_types.size())
			throw _MSG_EXCEPT_C("The element types given for partition " + std::to_string(partition.partition_id) + " do not match the size expected.");
		if (partition.element_global_ids.size() != partition.element_parent_partitions.size())
			throw _MSG_EXCEPT_C("The element parent partitions given for partition " + std::to_string(partition.partition_id) + " do not match the size expected.");
		if (partition.element_global_ids.size() != partition.element_connectivities.size())
			throw _MSG_EXCEPT_C("The element connectivitites given for partition " + std::to_string(partition.partition_id) + " do not match the size expected.");

		// Check that the component ID exists in the meta_info
		for (auto& subcomponent : partition.subcomponents) {
			auto match_iter = std::find_if(meta_info_.components.begin(), meta_info_.components.end(),
				[id = subcomponent.component_id](const ComponentMetaInfo<LocalIndex>& x) -> bool {
				return x.id == id;
			});
			if (match_iter == meta_info_.components.end())
				throw _MSG_EXCEPT_C("In partition " + std::to_string(partition.partition_id) + ", component ID " + std::to_string(subcomponent.component_id) + " was given, but it was not included as a component when constructed.");
		}

		// Figure out the size on disk needed by each component
		std::vector<std::streamoff> subcomponent_offsets;
		std::streamoff curr_offset = 0;
		for (auto& subcomponent : partition.subcomponents) {
			subcomponent_offsets.push_back(curr_offset);

			// Note: it was already tested that the component id exists in the meta_info
			auto component = *std::find_if(meta_info_.components.begin(), meta_info_.components.end(),
				[id = subcomponent.component_id](const ComponentMetaInfo<LocalIndex>& x) -> bool {
				return x.id == id;
			});

			curr_offset += get_size_for_subcomponent<LocalIndex, CoordType>(
				static_cast<LocalIndex>(subcomponent.nodes_in_subcomponent.size()),
				static_cast<LocalIndex>(subcomponent.elements_in_subcomponent.size()),
				component.lcs_assignment, component.lcs_parameterization);
		}

		std::streamoff start_pos = out_stream_.tellp();

		// Write the high-level information about the partition
		IOUtils::binary_write(out_stream_, partition.partition_id);
		IOUtils::binary_write(out_stream_, static_cast<LocalIndex>(partition.node_global_ids.size()));
		IOUtils::binary_write(out_stream_, static_cast<LocalIndex>(partition.element_global_ids.size()));
		auto extent = calculate_extent(partition.node_coords, meta_info_.dimensionality);
		IOUtils::binary_write(out_stream_, extent.min);
		IOUtils::binary_write(out_stream_, extent.max);
		// Write the node information
		for (auto i = 0; i < static_cast<GlobalIndex>(partition.node_global_ids.size()); i++) {
			IOUtils::binary_write(out_stream_, partition.node_global_ids[i]);
			IOUtils::binary_write(out_stream_, partition.node_parent_partitions[i]);
			IOUtils::binary_write(out_stream_, &partition.node_coords[meta_info_.dimensionality*i], meta_info_.dimensionality);
		}
		// Write the element information
		for (auto i = 0; i < static_cast<GlobalIndex>(partition.element_global_ids.size()); i++) {
			IOUtils::binary_write(out_stream_, partition.element_global_ids[i]);
			IOUtils::binary_write(out_stream_, partition.element_parent_partitions[i]);
			IOUtils::binary_write(out_stream_, static_cast<std::underlying_type<ElementType>::type>(partition.element_types[i]));
			IOUtils::binary_write(out_stream_, partition.element_connectivities[i]);
		}
		// Write the subcomponent information
		IOUtils::binary_write(out_stream_, static_cast<LocalIndex>(partition.subcomponents.size()));
		IOUtils::binary_write(out_stream_, subcomponent_offsets);
		for (auto& subcomponent : partition.subcomponents) {
			// Note: it was already tested that the component id exists in the meta_info
			auto component = *std::find_if(meta_info_.components.begin(), meta_info_.components.end(),
				[id = subcomponent.component_id](const ComponentMetaInfo<LocalIndex>& x) -> bool {
				return x.id == id;
			});

			IOUtils::binary_write(out_stream_, subcomponent.component_id);
			IOUtils::binary_write(out_stream_, static_cast<LocalIndex>(subcomponent.nodes_in_subcomponent.size()));
			IOUtils::binary_write(out_stream_, subcomponent.nodes_in_subcomponent);
			IOUtils::binary_write(out_stream_, static_cast<LocalIndex>(subcomponent.elements_in_subcomponent.size()));
			IOUtils::binary_write(out_stream_, subcomponent.elements_in_subcomponent);
			IOUtils::binary_write(out_stream_, static_cast<LocalIndex>(subcomponent.lcs_parameters_in_subcomponent.size()));
			IOUtils::binary_write(out_stream_, subcomponent.lcs_parameters_in_subcomponent);
		}

		std::streamoff end_pos = out_stream_.tellp();

		return end_pos - start_pos;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline Extent<CoordType> MeshWriter<GlobalIndex, LocalIndex, CoordType>::calculate_extent(const std::vector<CoordType> node_coords, const LocalIndex dimensionality)
	{
		Extent<CoordType> extent;
		for (auto i = 0; i < static_cast<GlobalIndex>(node_coords.size()) / dimensionality;) {
			for (auto j = 0; j < dimensionality; j++) {
				const GlobalIndex node_i = dimensionality * i + j;
				if (extent.min[j] > node_coords[node_i])
					extent.min[j] = node_coords[node_i];
				if (extent.max[j] < node_coords[node_i])
					extent.max[j] = node_coords[node_i];
			}
			i += dimensionality;
		}
		for (auto j = dimensionality; j < 3; j++) {
			extent.min[j] = 0;
			extent.max[j] = 0;
		}
		return extent;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::initialize_state()
	{
		write_state_ = WriteState::WritePhase;
		out_stream_.open(file_path_ + binary_ext, std::ios::binary | std::fstream::out);
	}
}