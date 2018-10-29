#pragma once

#include <algorithm>
#include <array>
#include <stdint.h>
#include <vector>
#include <fstream>

#include <nlohmann/json.hpp>

#ifdef HAVE_MPI
#include "MPIUtils.h"
#endif

#include "MeshStructs.h"
#include "IOUtils.h"
#include "Exceptions.h"

namespace fea_io
{
	using json = nlohmann::json;

	enum class WriteMode : char {
		Serial,
		Parallel
	};
	enum class WriteState : char {
		QueuePhase,
		WritePhase,
		Finalized
	};

	/// A class for writing a two-part mesh file(s) composed of a JSON and binary file.
	/// The class supports serial and parallel I/O through the same interface.
	/// For example, to write in serial:
	/// \code{cpp}
	/// MeshWriter<int64_t, int32_t, double> writer(..., WriteMode::Serial);
	///	writer.write_partition(...);
	/// \endcode
	/// 
	/// To write in parallel:
	/// \code{cpp}
	/// MeshWriter<int64_t, int32_t, double> writer(..., WriteMode::Parallel);
	/// // Queue each partition before any processor writes anything so the size and offsets
	/// // of the data for each rank can be computed
	/// writer.queue_partition_for_write(...);
	/// // When all the partitions have been queued, indicate it is time to write
	/// writer.start_parallel_write_phase();
	/// // Then each processor writes the partitions that it queued
	///	writer.write_partition(...);
	/// \endcode
	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	class MeshWriter
	{
	public:
		typedef MeshMetaInfo<GlobalIndex, LocalIndex, CoordType> MetaInfo;

		/// Some notes on the paramters:
		/// - file_path: leave off the extension but provide path and file name
		/// - dimensionality: indicates if 1D, 2D, or 3D (1, 2, or 3 respectively)
		/// - extent: 3D bounding box of entire mesh (helps with post-processing scripts)
		/// - component_ids must match size of component_names.  Each entry in each vector
		///   indicates the ID and name of a constitutent respectively, i.e. ordering matters.
		MeshWriter(
			const std::string file_path,
			const GlobalIndex total_num_nodes,
			const GlobalIndex total_num_elements,
			const LocalIndex dimensionality,
			const Extent<CoordType> extent,
			const std::vector<LocalIndex> component_ids,
			const std::vector<std::string> component_names,
			const std::vector<LCSAssignment> component_lcs_assignments,
			const std::vector<LCSParameterization> component_lcs_parameterizations,
			const LocalIndex total_num_partitions = 0,
			const WriteMode write_mode = WriteMode::Serial);

		MeshWriter(
			const std::string file_path,
			const MetaInfo meta_info,
			const WriteMode write_mode = WriteMode::Serial);

		/// Destructor calls finalize() if it has not been called.
		~MeshWriter();

		/// If in parallel mode, then this needs to be called for each partition
		/// before writing begins.  An error will be thrown otherwise.
		void queue_partition_for_write(const PartitionInfo<GlobalIndex, LocalIndex, CoordType>& part_info);

		/// If the constructor was called with parallel mode specified, then
		/// this must be called after all the queue_partition_for_write calls
		/// have been made, but before write_partition is ever called.
		void start_parallel_write_phase();

		/// Write a partition to the mesh.  if in parallel mode, this must be called
		/// after start_parallel_write_phase has been called.
		/// A few notes about the Partition struct:
		/// - For serial mode, partition_id must be given as an integer that is
		///   monotonically increasing (0, 1, 2, 3, etc.).
		/// - For parallel mode, queue_partition_for_write must have already been called
		///   for a partition with the partition_id.
		/// - node_global_inds, node_parent_partitions, and node_coords must be the same size
		///   (for parallel mode, they must also match the number of nodes given earlier for
		///   the partition).
		/// - element_global_inds, element_parent_partitions, element_types, and element_connectivities
		///   must be the same size (for parallel mode, they must also match the number of nodes 
		///   given earlier for the partition).
		/// - For parallel mode, subcomponents.component_ids_of_subcomponents must match the component
		///   IDs given for the partition (in the vector of subcomponent structs)
		void write_partition(const Partition<GlobalIndex, LocalIndex, CoordType>& partition);

		/// TODO: implement this method once this class has been debugged
		/// Takes an iterator adapter (see MeshIteratorAdapters.h for reference) for the begin and end of
		/// nodes and elements and writes the mesh data without intermediate data.
		template<typename NodeIteratorAdapter, typename ElementIteratorAdapter>
		void write_partition(
			const NodeIteratorAdapter node_begin, const NodeIteratorAdapter node_end,
			const ElementIteratorAdapter element_begin, const ElementIteratorAdapter element_end);

		/// A wrapper for writing the first partition in the mesh for serial mode
		/// Note: one could just use write_partition, which offers more flexibility
		/// with iterators
		void write(
			const std::vector<GlobalIndex> node_global_ids,
			const std::vector<CoordType> node_coords,
			const std::vector<GlobalIndex> element_global_ids,
			const std::vector<ElementType> element_types,
			const std::vector<std::vector<LocalIndex>> element_connectivities,
			const std::vector<Subcomponent<LocalIndex, CoordType>> subcomponents);

		/// Close the binary file, writes the JSON file, and resets the MeshWriter object. 
		/// Note: this gets call with the destructor, so you don't have to call it unless
		/// you want to force it to up the files.
		void close();

	private:
		MeshWriter() = delete;
		
		MeshMetaInfo<GlobalIndex, LocalIndex, CoordType> meta_info_;
		std::vector<PartitionInfo<GlobalIndex, LocalIndex, CoordType>> partition_infos_;
		WriteMode write_mode_;
		WriteState write_state_;
		std::string file_path_;
		std::ofstream out_stream_;
		const std::string binary_ext = ".bin";
		
		/// Gets partition_infos_ from all the ranks and calculates the offsets that each partition
		/// will have.  meta_info_ will be updated accordingly on every rank.
		/// Returns the total number of bytes needed for the mesh file.
		std::streamoff calculate_offsets();

		Extent<CoordType> calculate_extent(const std::vector<CoordType> node_coords, const LocalIndex dimensionality);
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline fea_io::MeshWriter<GlobalIndex, LocalIndex, CoordType>::MeshWriter(const std::string file_path, const GlobalIndex total_num_nodes, const GlobalIndex total_num_elements, const LocalIndex dimensionality, const Extent<CoordType> extent, const std::vector<LocalIndex> component_ids, const std::vector<std::string> component_names, const std::vector<LCSAssignment> component_lcs_assignments, const std::vector<LCSParameterization> component_lcs_parameterizations, const LocalIndex total_num_partitions, const WriteMode write_mode)
	{
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

		write_mode_ = write_mode;
		file_path_ = file_path;

		if (write_mode == WriteMode::Serial) {
			write_state_ = WriteState::WritePhase;
			out_stream_.open(file_path_ + binary_ext, std::ios::binary | std::fstream::out);
		}
		else if (write_mode == WriteMode::Parallel)
			write_state_ = WriteState::QueuePhase;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline fea_io::MeshWriter<GlobalIndex, LocalIndex, CoordType>::MeshWriter(const std::string file_path, const MetaInfo meta_info, const WriteMode write_mode)
	{
		write_mode_ = write_mode;
		meta_info_ = meta_info;
		file_path_ = file_path;

		if (write_mode == WriteMode::Serial) {
			write_state_ = WriteState::WritePhase;
			out_stream_.open(file_path_ + binary_ext, std::ios::binary | std::fstream::out);
		}
		else if (write_mode == WriteMode::Parallel)
			write_state_ = WriteState::QueuePhase;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline MeshWriter<GlobalIndex, LocalIndex, CoordType>::~MeshWriter()
	{
		if (write_state_ != WriteState::Finalized)
			close();
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::queue_partition_for_write(const PartitionInfo<GlobalIndex, LocalIndex, CoordType>& partition_info)
	{
		if (write_state_ != WriteState::QueuePhase)
			throw _MSG_EXCEPT_C("Attempted to queue a partition while not in queue phase.  If in serial, then you can write immediately.");

		partition_infos_.push_back(partition_info);
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::start_parallel_write_phase()
	{
		if (write_state_ != WriteState::QueuePhase)
			throw _MSG_EXCEPT_C("Attempted to start a parallel write while not in queue phase.  If in serial, then you can write immediately.");

		// First calculate the offsets and get the size of the file needed
		std::streamoff size_needed = calculate_offsets();

		if (MPIUtils::get_local_rank() == 0) {
			// For now, zeroes will be written to a file to preallocate it.
			// the OS should have a way to preallocate a non-fragmented file
			// without writing the data, but it is not clear how to do it in a
			// portable way.  So for now...the dumb method will suffice.
			std::ofstream out_stream(file_path_ + binary_ext, std::ios::binary);
			const int buffer_size = 1024* 1024; // 1 MB buffer
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

		// Broadcast the header to all rank so they know the offsets
		MPIUtils::update_from_root(meta_info_);

		// Everyone needs to open the file in in/out mode to be able to seek to different parts and write at will.
		// Note: append mode will not let you go back before the end.
		out_stream_.open(file_path_ + binary_ext, std::ios::binary | std::fstream::in | std::fstream::out);
		out_stream_.seekp(0, std::ios::beg);

		write_state_ = WriteState::WritePhase;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void fea_io::MeshWriter<GlobalIndex, LocalIndex, CoordType>::write_partition(const Partition<GlobalIndex, LocalIndex, CoordType> & partition)
	{
		if (write_state_ != WriteState::WritePhase)
			throw _MSG_EXCEPT_C("Attempted to write a partition while not in write phase.  If in parallel, be sure to call start_parallel_write_phase() after queuing the information.");

		if (write_mode_ == WriteMode::Parallel) {
			// Make sure the given partition was already queued on this processor
			auto match_iter = std::find_if(partition_infos_.begin(), partition_infos_.end(),
				[id = partition.partition_id](const PartitionInfo<GlobalIndex, LocalIndex, CoordType>& x) -> bool {
				return x.partition_id == id;
			});
			if (match_iter == partition_infos_.end())
				throw _MSG_EXCEPT_C("Data was provided for partition " + std::to_string(partition.partition_id) + " but this processor does not contain the info for it.");
			auto& partition_info = *match_iter;

			// Check that provided vectors match the expected size form the queued partition info
			if (partition_info.num_nodes != partition.node_global_ids.size())
				throw _MSG_EXCEPT_C("The node global ids given for partition " + std::to_string(partition.partition_id) + " do not match the size expected.");
			if (partition_info.num_elements != partition.element_global_ids.size())
				throw _MSG_EXCEPT_C("The element global ids given for partition " + std::to_string(partition.partition_id) + " do not match the size expected.");

			// Check that the components exist in the partition according to the PartitionInfo provided earlier
			for (auto& subcomponent : partition.subcomponents) {
				auto c_match_iter = std::find_if(partition_info.subcomponents.begin(), partition_info.subcomponents.end(),
					[id = subcomponent.component_id](const SubcomponentInfo<LocalIndex>& x) -> bool {
					return x.component_id == id;
				});
				if (c_match_iter == partition_info.subcomponents.end())
					throw _MSG_EXCEPT_C("In partition " + std::to_string(partition.partition_id) + ", component ID " + std::to_string(subcomponent.component_id) + " was given, but it was not given when the partition was queued earlier.");
			}
		}
		else {
			if (meta_info_.partition_offsets.size() != partition.partition_id)
				throw _MSG_EXCEPT_C("During a serial write, the partitions were attempted to be written non-sequentially. Partition " + std::to_string(meta_info_.partition_offsets.size()) + " expected, but " + std::to_string(partition.partition_id) + " given.");
		}

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

		// If in parallel mode, part of the file is already reserved for this partition
		if (write_mode_ == WriteMode::Parallel)
			out_stream_.seekp(meta_info_.partition_offsets[partition.partition_id]);

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
			IOUtils::binary_write(out_stream_, static_cast<std::underlying_type<ElementType>::type>(partition.element_types[i]));
			IOUtils::binary_write(out_stream_, partition.element_parent_partitions[i]);
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
			IOUtils::binary_write(out_stream_, subcomponent.lcs_parameters_in_subcomponent);
		}

		std::streamoff end_pos = out_stream_.tellp();

		if (write_mode_ == WriteMode::Parallel) {
			auto expected_size = std::find_if(partition_infos_.begin(), partition_infos_.end(),
				[id = partition.partition_id](const PartitionInfo<GlobalIndex, LocalIndex, CoordType>& x) -> bool {
				return x.partition_id == id;
			})->get_size_needed(meta_info_);
			if (end_pos - start_pos != expected_size)
				throw _MSG_EXCEPT_C("The data written for partition " + std::to_string(partition.partition_id) + " exceeded the space allocated for it."
					+ " Expected size of " + std::to_string(expected_size) + " but actually ended at " + std::to_string(end_pos - start_pos) + ".");
		}
		else {
			meta_info_.partition_offsets.push_back(start_pos);
		}
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::close()
	{
		out_stream_.close();

		std::ofstream json_component(file_path_ + ".json");
		json j;
		j["meta_info"] = meta_info_;
		DataTypeSizes<GlobalIndex, LocalIndex, CoordType> data_types;
		j["types"] = data_types;
		json_component << std::setw(4) << j << std::endl;
		json_component.close();

		write_state_ = WriteState::Finalized;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline std::streamoff MeshWriter<GlobalIndex, LocalIndex, CoordType>::calculate_offsets()
	{

#ifdef HAVE_MPI
		// The root rank will get the entire list of partition info, which will allow it
		// to calculate the appropriate offsets.
		auto all_partition_info = MPIUtils::gather_vectors(partition_infos_);
#else
		// If this was not compiled using MPI, the the process must have all of the data already
		auto all_partition_info = partition_infos_;
#endif

		// Compute the size needed for each domain, and save the size required
		// for each partition within an ordered vector (this means that the
		// partitions will ordered by index within the binary file)
		std::vector<std::streamoff> partition_byte_sizes(all_partition_info.size(), 0);
		for (auto& pi : all_partition_info) {
			partition_byte_sizes[pi.partition_id] = pi.get_size_needed(meta_info_);
		}

		// Compute the offsets of each partition (starting at 0)
		meta_info_.partition_offsets.resize(partition_byte_sizes.size());
		std::streamoff prev_size = 0;
		for (auto i = 0; i < partition_byte_sizes.size(); i++) {
			meta_info_.partition_offsets[i] = prev_size;
			prev_size += partition_byte_sizes[i];
		}

		return prev_size;
	}


	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline Extent<CoordType> fea_io::MeshWriter<GlobalIndex, LocalIndex, CoordType>::calculate_extent(const std::vector<CoordType> node_coords, const LocalIndex dimensionality)
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
		return extent;
	}
}