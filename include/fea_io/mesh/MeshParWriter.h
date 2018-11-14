#pragma once

#include "fea_io/common/MPIUtils.h"

#include "MeshWriter.h"

namespace fea_io
{
	using json = nlohmann::json;

	/// A class for writing a two-part mesh file(s) composed of a JSON and binary file
	/// in parallel.
	/// For example, to write in parallel:
	/// \code{cpp}
	/// MeshParWriter<int64_t, int32_t, double> writer(...);
	/// // Queue each partition before any processor writes anything so the size and offsets
	/// // of the data for each rank can be computed
	/// writer.queue_partition_for_write(...);
	/// // When all the partitions have been queued, indicate it is time to write
	/// writer.start_parallel_write_phase();
	/// // Then each processor writes the partitions that it queued
	///	writer.write_partition(...);
	/// \endcode
	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	class MeshParWriter : public MeshWriter<GlobalIndex, LocalIndex, CoordType>
	{
	public:
		typedef MeshMetaInfo<GlobalIndex, LocalIndex, CoordType> MeshMetaInfoType;
		typedef PartitionInfo<GlobalIndex, LocalIndex, CoordType> PartitionInfoType;
		typedef Partition<GlobalIndex, LocalIndex, CoordType> PartitionType;

		using MeshWriter<GlobalIndex, LocalIndex, CoordType>::MeshWriter;

		/// This needs to be called for each partition before writing begins.  An error will be thrown otherwise.
		void queue_partition_for_write(const PartitionInfoType& part_info);

		/// This must be called after all the queue_partition_for_write calls
		/// have been made, but before write_partition is ever called.
		void start_parallel_write_phase();

		/// Write a partition to the mesh.  This must be called after start_parallel_write_phase has been called.
		/// A few notes about the Partition struct:
		/// - queue_partition_for_write must have already been called for a partition with the partition_id.
		/// - node_global_inds, node_parent_partitions, and node_coords must be the same size.
		///   They must also match the number of nodes given earlier for the partition.
		/// - element_global_inds, element_parent_partitions, element_types, and element_connectivities
		///   must be the same size. They must also match the number of nodes given earlier for the partition.
		/// - subcomponents.component_ids_of_subcomponents must match the component IDs given for the
		///   partition (in the vector of subcomponent structs)
		virtual void write_partition(const PartitionType& partition) override;

		/// TODO: implement this method once this class has been debugged
		/// Takes an iterator adapter (see MeshIteratorAdapters.h for reference) for the begin and end of
		/// nodes and elements and writes the mesh data without intermediate data.
		template<typename NodeIteratorAdapter, typename ElementIteratorAdapter>
		void write_partition(
			const NodeIteratorAdapter node_begin, const NodeIteratorAdapter node_end,
			const ElementIteratorAdapter element_begin, const ElementIteratorAdapter element_end);

		/// Close the binary file, writes the JSON file, and resets the MeshParWriter object. 
		/// Note: this gets call with the destructor, so you don't have to call it unless
		/// you want to force it to up the files.
		virtual void close() override;

	protected:
		
		/// Gets partition_infos_ from all the ranks and calculates the offsets that each partition
		/// will have.  meta_info_ will be updated accordingly on every rank.
		/// Returns the total number of bytes needed for the mesh file.
		std::streamoff calculate_offsets();

		/// A helper method called by constructors
		virtual void initialize_state() override;

	private:
		typedef MeshWriter<GlobalIndex, LocalIndex, CoordType> Parent;
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshParWriter<GlobalIndex, LocalIndex, CoordType>::queue_partition_for_write(const PartitionInfoType& partition_info)
	{
		if (Parent::write_state_ != WriteState::QueuePhase)
			throw _MSG_EXCEPT_C("Attempted to queue a partition while not in queue phase.  If in serial, then you can write immediately.");

		Parent::partition_infos_.push_back(partition_info);
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshParWriter<GlobalIndex, LocalIndex, CoordType>::start_parallel_write_phase()
	{
		if (Parent::write_state_ != WriteState::QueuePhase)
			throw _MSG_EXCEPT_C("Attempted to start a parallel write while not in queue phase.  If in serial, then you can write immediately.");

		// First calculate the offsets and get the size of the file needed
		std::streamoff size_needed = calculate_offsets();

		if (MPIUtils::get_local_rank() == 0) {
			// For now, zeroes will be written to a file to preallocate it.
			// the OS should have a way to preallocate a non-fragmented file
			// without writing the data, but it is not clear how to do it in a
			// portable way.  So for now...the dumb method will suffice.
			std::ofstream out_stream(Parent::file_path_ + Parent::binary_ext, std::ios::binary);
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
		MPIUtils::update_from_root(Parent::meta_info_);

		// Everyone needs to open the file in in/out mode to be able to seek to different parts and write at will.
		// Note: append mode will not let you go back before the end.
		Parent::out_stream_.open(Parent::file_path_ + Parent::binary_ext, std::ios::binary | std::fstream::in | std::fstream::out);
		Parent::out_stream_.seekp(0, std::ios::beg);

		Parent::write_state_ = WriteState::WritePhase;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void fea_io::MeshParWriter<GlobalIndex, LocalIndex, CoordType>::write_partition(const PartitionType & partition)
	{
		if (Parent::write_state_ != WriteState::WritePhase)
			throw _MSG_EXCEPT_C("Attempted to write a partition while not in write phase.  If in parallel, be sure to call start_parallel_write_phase() after queuing the information.");

		// Make sure the given partition was already queued on this processor
		auto match_iter = std::find_if(Parent::partition_infos_.begin(), Parent::partition_infos_.end(),
			[id = partition.partition_id](const PartitionInfo<GlobalIndex, LocalIndex, CoordType>& x) -> bool {
			return x.partition_id == id;
		});
		if (match_iter == Parent::partition_infos_.end())
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

		// Part of the file is already reserved for this partition
		Parent::out_stream_.seekp(Parent::meta_info_.partition_offsets[partition.partition_id]);

		auto actual_size = Parent::write_partition_kernel(partition);

		// If the extent was not calculated when the partition info was queued, then
		// calculate it now.
		if (partition_info.extent.is_limit())
			partition_info.extent = calculate_extent(partition.node_coords, meta_info_.dimensionality);

		update_subcomponent_offsets(partition_info);

		auto expected_size = std::find_if(Parent::partition_infos_.begin(), Parent::partition_infos_.end(),
			[id = partition.partition_id](const PartitionInfo<GlobalIndex, LocalIndex, CoordType>& x) -> bool {
			return x.partition_id == id;
		})->get_size_needed(Parent::meta_info_);

		if (actual_size != expected_size)
			throw _MSG_EXCEPT_C("The data written for partition " + std::to_string(partition.partition_id) + " exceeded the space allocated for it."
				+ " Expected size of " + std::to_string(expected_size) + " but actually ended at " + std::to_string(actual_size) + ".");
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshParWriter<GlobalIndex, LocalIndex, CoordType>::close()
	{
		out_stream_.close();

		// Only rank 0 should write the JSON file
		if (MPIUtils::get_local_rank() == 0) {
			// The root rank will get the entire list of partition info, which will allow it
			// to calculate the appropriate offsets.
			auto all_partition_info = MPIUtils::gather_vectors(Parent::partition_infos_);

			std::ofstream json_component(file_path_ + Parent::json_ext);
			json j;
			j["meta_info"] = meta_info_;
			j["partition_info"] = all_partition_info;
			DataTypeSizes data_types;
			data_types.deduce<GlobalIndex, LocalIndex, CoordType>();
			j["types"] = data_types;
			json_component << std::setw(4) << j << std::endl;
			json_component.close();
		}

		
		write_state_ = WriteState::Finalized;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline std::streamoff MeshParWriter<GlobalIndex, LocalIndex, CoordType>::calculate_offsets()
	{
		// The root rank will get the entire list of partition info, which will allow it
		// to calculate the appropriate offsets.
		auto all_partition_info = MPIUtils::gather_vectors(Parent::partition_infos_);

		// Compute the size needed for each partition, and save the size required
		// for each partition within an ordered vector (this means that the
		// partitions will ordered by index within the binary file)
		std::vector<std::streamoff> partition_byte_sizes(all_partition_info.size(), 0);
		for (auto& pi : all_partition_info) {
			partition_byte_sizes[pi.partition_id] = pi.get_size_needed(Parent::meta_info_);
		}

		// Compute the offsets of each partition (starting at 0)
		Parent::meta_info_.partition_offsets.resize(partition_byte_sizes.size());
		std::streamoff prev_size = 0;
		for (auto i = 0; i < partition_byte_sizes.size(); i++) {
			Parent::meta_info_.partition_offsets[i] = prev_size;
			prev_size += partition_byte_sizes[i];
		}

		return prev_size;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshParWriter<GlobalIndex, LocalIndex, CoordType>::initialize_state()
	{
		Parent::write_state_ = WriteState::QueuePhase;
	}


}