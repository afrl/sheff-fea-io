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

#include "MeshInfoStructs.h"
#include "IOUtils.h"

namespace fea_io
{
	using json = nlohmann::json;

	/// The workflow should be:
	/// - provide header to constructor
	///   - it will deduce the data type sizes
	/// - write each subdomain
	/// or
	/// - provide sizes of each subdomain and write in parallel
	// Generate two files:
	// - json <title>.jmesh
	// - binary data <title>.bmesh
	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	class MeshWriter
	{
	public:
		enum class WriteMode : char {
			Serial,
			Parallel
		};
		enum class WriteState : char {
			QueuePhase,
			WritePhase,
			Finalized
		};

		MeshWriter(const GlobalIndex total_num_nodes,
			const GlobalIndex total_num_elements,
			const LocalIndex dimensionality,
			const std::array<CoordType, 6> extent,
			const LocalIndex num_constituents,
			const std::vector<LocalIndex> constituent_ids,
			const std::vector<std::string> constituent_names,
			const WriteMode& write_mode,
			const std::string file_path);

		MeshWriter(const MeshHeader<GlobalIndex, LocalIndex, CoordType>& header,
			const WriteMode& write_mode,
			const std::string file_path);

		/// If in parallel mode, then this needs to be called for each subdomain
		/// before writing begins.  An error will be thrown otherwise.
		void queue_subdomain_for_write(const SubdomainInfo<GlobalIndex, LocalIndex, CoordType>& subdomain_info);

		// TODO document
		void start_parallel_write_phase();

		// Write one subdomain
		void write_subdomain(LocalIndex subdomain_index,
			std::vector<GlobalIndex> node_global_inds,
			std::vector<LocalIndex> node_parent_subdomains,
			std::vector<CoordType> node_coords,
			std::vector<GlobalIndex> element_global_inds,
			std::vector<LocalIndex> element_parent_subdomains,
			std::vector<ElementType> element_types,
			std::vector<std::vector<LocalIndex>> element_connectivities,
			std::vector<std::vector<LocalIndex>> nodes_in_subconstituents,
			std::vector<std::vector<LocalIndex>> elements_in_subconstituents,
			std::vector<std::vector<double>> lcs_parameters_in_subconstituents);

		// Close the binary file, writes the JSON file, and resets the MeshWriter object. 
		// Note: this gets call with the destructor, so you don't have to call it unless
		// you want to force it to release memory early and finish up the files.
		void finalize();

	private:
		MeshWriter() = delete;
		
		MeshHeader<GlobalIndex, LocalIndex, CoordType> header_;
		WriteMode write_mode_;
		WriteState write_state_;
		std::string file_path_;
		std::ofstream out_stream_;

		// Variables needed to track changes. Save positions as write_subdomain
		// is called, and use these positions to update the lookup tables at
		// the beginning of the destructor being called.
		std::vector<LocalIndex> updated_subdomain_ids_;
		std::vector<std::streamoff> updated_subdomain_offsets_;
		std::vector<SubdomainInfo<GlobalIndex, LocalIndex, CoordType>> subdomain_info_;
		
		/// Given the current state of subdomain_info_, this method will calculate the
		/// offsets that each subdomain will have.  header_ will be updated accordingly.
		/// Returns the total number of bytes needed for the mesh file.
		std::streamoff calculate_offsets();
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline MeshWriter<GlobalIndex, LocalIndex, CoordType>::MeshWriter(const GlobalIndex total_num_nodes, const GlobalIndex total_num_elements, const LocalIndex dimensionality, const std::array<CoordType, 6> extent, const LocalIndex num_constituents, const std::vector<LocalIndex> constituent_ids, const std::vector<std::string> constituent_names, const WriteMode & write_mode, const std::string file_path)
	{
		header_.total_num_nodes = total_num_nodes;
		header_.total_num_elements = total_num_elements;
		header_.dimensionality = dimensionality;
		header_.extent = extent;
		header_.num_constituents = num_constituents;
		header_.constituent_ids = constituent_ids;
		header_.constituent_names = constituent_names;

		write_mode_ = write_mode;
		file_path_ = file_path;

		if (write_mode == WriteMode::Serial)
			write_state_ = WriteState::WritePhase;
		else if (write_mode == WriteMode::Parallel)
			write_state_ = WriteState::QueuePhase;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline MeshWriter<GlobalIndex, LocalIndex, CoordType>::MeshWriter(const MeshHeader<GlobalIndex, LocalIndex, CoordType>& header, const WriteMode & write_mode, const std::string file_path)
	{
		header_ = header;
		write_mode_ = write_mode;
		file_path_ = file_path;

		if (write_mode == WriteMode::Serial)
			write_state_ = WriteState::WritePhase;
		else if (write_mode == WriteMode::Parallel)
			write_state_ = WriteState::QueuePhase;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::queue_subdomain_for_write(const SubdomainInfo<GlobalIndex, LocalIndex, CoordType>& subdomain_info)
	{
		// TODO check if queue phase, otherwise throw

		subdomain_info_.push_back(subdomain_info);
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::start_parallel_write_phase()
	{
		// TODO check if queue phase, otherwise throw

		// First calculate the offsets and get the size of the file needed
		std::streamoff size_needed = calculate_offsets();

		if (MPIUtils::get_local_rank() == 0) {
			// For now, zeroes will be written to a file to preallocate it.
			// the OS should have a way to preallocate a non-fragmented file
			// without writing the data, but it is not clear how to do it in a
			// portable way.  So for now...the dumb method will suffice.
			std::ofstream out_stream(file_path_, std::ios::binary);
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
		MPIUtils::update_from_root(header_);

		// Everyone needs to open the file in append mode
		out_stream_.open(file_path_, std::ios::binary | std::ios::app);
		// Start every process out at the beginning of the file
		out_stream_.seekp(0, std::ios::beg);

		write_state_ = WriteState::WritePhase;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void fea_io::MeshWriter<GlobalIndex, LocalIndex, CoordType>::write_subdomain(LocalIndex subdomain_index, std::vector<GlobalIndex> node_global_inds, std::vector<LocalIndex> node_parent_subdomains, std::vector<CoordType> node_coords, std::vector<GlobalIndex> element_global_inds, std::vector<LocalIndex> element_parent_subdomains, std::vector<ElementType> element_types, std::vector<std::vector<LocalIndex>> element_connectivities, std::vector<std::vector<LocalIndex>> nodes_in_subconstituents, std::vector<std::vector<LocalIndex>> elements_in_subconstituents, std::vector<std::vector<double>> lcs_parameters_in_subconstituents)
	{
		const auto match_si_iter = std::find_if(subdomain_info_.begin(), subdomain_info_.end(),
			[&subdomain_index](const SubdomainInfo<GlobalIndex, LocalIndex, CoordType>& x) -> bool {
				return x.subdomain_index == subdomain_index;
		} );
		auto& subdomain_info = *match_si_iter;

		// Figure out the size on disk needed by each component
		std::vector<std::streamoff> subconstituent_offsets;
		std::streamoff curr_offset = 0;
		for (auto sc : subdomain_info.subconstituents) {
			subconstituent_offsets.push_back(curr_offset);
			curr_offset += sc.get_size_needed();
		}

		// Move to the part of the file reserved for this subdomain
		out_stream_.seekp(header_.subdomain_offsets[subdomain_index]);

		// Write the high-level information about the subdomain
		IOUtils::binary_write(out_stream_, subdomain_index);
		IOUtils::binary_write(out_stream_, subdomain_info.num_nodes);
		IOUtils::binary_write(out_stream_, subdomain_info.num_elements);
		IOUtils::binary_write(out_stream_, subdomain_info.extent);
		// Write the node information
		for (auto i = 0; i < subdomain_info.num_nodes; i++) {
			IOUtils::binary_write(out_stream_, node_global_inds[i]);
			IOUtils::binary_write(out_stream_, node_parent_subdomains[i]);
			for (auto j = 0; j < header_.dimensionality; j++)
				IOUtils::binary_write(out_stream_, node_coords[header_.dimensionality*i + j]);
		}
		// Write the element information
		for (auto i = 0; i < subdomain_info.num_elements; i++) {
			IOUtils::binary_write(out_stream_, element_global_inds[i]);
			IOUtils::binary_write(out_stream_, static_cast<std::underlying_type<ElementType>::type>(element_types[i]));
			IOUtils::binary_write(out_stream_, element_parent_subdomains[i]);
			IOUtils::binary_write(out_stream_, element_connectivities[i]);
		}
		// Write the component information
		IOUtils::binary_write(out_stream_, subdomain_info.num_constituents);
		IOUtils::binary_write(out_stream_, subconstituent_offsets);
		for (auto& sc : subdomain_info.subconstituents) {
			IOUtils::binary_write(out_stream_, sc.constituent_id);
		}
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline std::streamoff MeshWriter<GlobalIndex, LocalIndex, CoordType>::calculate_offsets()
	{

#ifdef HAVE_MPI
		// The root rank will get the entire list of subdomain info, which will allow it
		// to calculate the appropriate offsets.
		auto all_subdomain_info = MPIUtils::gather_vectors(subdomain_info_);
#else
		// If this was not compiled using MPI, the the process must have all of the data already
		auto all_subdomain_info = subdomain_info_;
#endif

		// Compute the size needed for each domain, and save the size required
		// for each subdomain within an ordered vector (this means that the
		// subdomains will ordered by index within the binary file)
		std::vector<std::streamoff> subdomain_byte_sizes(subdomain_info_.size(), 0);
		for (auto& si : subdomain_info_)
			subdomain_byte_sizes[si.subdomain_index] = si.get_size_needed();

		// Compute the offsets of each subdomain (starting at 0)
		header_.subdomain_offsets.resize(subdomain_byte_sizes.size());
		std::streamoff prev_size = 0;
		for (auto i = 0; i < subdomain_byte_sizes.size(); i++) {
			header_.subdomain_offsets[i] = prev_size;
			prev_size += subdomain_byte_sizes[i];
		}

		return prev_size;
	}

}