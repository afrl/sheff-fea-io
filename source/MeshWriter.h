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
#include "Exceptions.h"

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
			const LocalIndex total_num_subdomains,
			const WriteMode& write_mode,
			const std::string file_path);

		MeshWriter(const MeshHeader<GlobalIndex, LocalIndex, CoordType>& header,
			const WriteMode& write_mode,
			const std::string file_path);

		/// If in parallel mode, then this needs to be called for each subdomain
		/// before writing begins.  An error will be thrown otherwise.
		void queue_subdomain_for_write(const SubdomainInfo<GlobalIndex, LocalIndex, CoordType>& subdomain_info);

		/// If the constructor was called with parallel mode specified, then
		/// this must be called after all the queue_subdomain_for_write calls
		/// have been made, but before write_subdomain is ever called.
		void start_parallel_write_phase();

		// Write one subdomain
		void write_subdomain(
			const SubdomainInfo<GlobalIndex, LocalIndex, CoordType>& subdomain_info,
			std::vector<GlobalIndex> node_global_inds,
			std::vector<LocalIndex> node_parent_subdomains,
			std::vector<CoordType> node_coords,
			std::vector<GlobalIndex> element_global_inds,
			std::vector<LocalIndex> element_parent_subdomains,
			std::vector<ElementType> element_types,
			std::vector<std::vector<LocalIndex>> element_connectivities,
			std::vector<std::vector<LocalIndex>> nodes_in_subconstituents,
			std::vector<std::vector<LocalIndex>> elements_in_subconstituents,
			std::vector<std::vector<CoordType>> lcs_parameters_in_subconstituents);

		// Close the binary file, writes the JSON file, and resets the MeshWriter object. 
		// Note: this gets call with the destructor, so you don't have to call it unless
		// you want to force it to release memory early and finish up the files.
		void finalize();

	private:
		MeshWriter() = delete;
		
		MeshHeader<GlobalIndex, LocalIndex, CoordType> header_;
		std::vector<SubdomainInfo<GlobalIndex, LocalIndex, CoordType>> subdomain_infos_;
		WriteMode write_mode_;
		WriteState write_state_;
		std::string file_path_;
		std::ofstream out_stream_;
		const std::string binary_ext = ".bin";
		
		/// Given the current state of subdomain_infos_, this method will calculate the
		/// offsets that each subdomain will have.  header_ will be updated accordingly.
		/// Returns the total number of bytes needed for the mesh file.
		std::streamoff calculate_offsets();
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline fea_io::MeshWriter<GlobalIndex, LocalIndex, CoordType>::MeshWriter(const GlobalIndex total_num_nodes, const GlobalIndex total_num_elements, const LocalIndex dimensionality, const std::array<CoordType, 6> extent, const LocalIndex num_constituents, const std::vector<LocalIndex> constituent_ids, const std::vector<std::string> constituent_names, const LocalIndex total_num_subdomains, const WriteMode & write_mode, const std::string file_path)
	{
		header_.total_num_nodes = total_num_nodes;
		header_.total_num_elements = total_num_elements;
		header_.dimensionality = dimensionality;
		header_.extent = extent;
		header_.num_constituents = num_constituents;
		header_.constituent_ids = constituent_ids;
		header_.constituent_names = constituent_names;
		header_.total_num_subdomains = total_num_subdomains;

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

		if (write_mode == WriteMode::Serial) {
			write_state_ = WriteState::WritePhase;
			out_stream_.open(file_path_ + binary_ext, std::ios::binary | std::fstream::out);
		}
		else if (write_mode == WriteMode::Parallel)
			write_state_ = WriteState::QueuePhase;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::queue_subdomain_for_write(const SubdomainInfo<GlobalIndex, LocalIndex, CoordType>& subdomain_info)
	{
		if (write_state_ != WriteState::QueuePhase)
			throw _MSG_EXCEPT_C("Attempted to queue a subdomain while not in queue phase.  If in serial, then you can write immediately.");

		subdomain_infos_.push_back(subdomain_info);
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
		MPIUtils::update_from_root(header_);

		// Everyone needs to open the file in in/out mode to be able to seek to different parts and write at will.
		// Note: append mode will not let you go back before the end.
		out_stream_.open(file_path_ + binary_ext, std::ios::binary | std::fstream::in | std::fstream::out);
		out_stream_.seekp(0, std::ios::beg);

		write_state_ = WriteState::WritePhase;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void fea_io::MeshWriter<GlobalIndex, LocalIndex, CoordType>::write_subdomain(const SubdomainInfo<GlobalIndex, LocalIndex, CoordType>& subdomain_info, std::vector<GlobalIndex> node_global_inds, std::vector<LocalIndex> node_parent_subdomains, std::vector<CoordType> node_coords, std::vector<GlobalIndex> element_global_inds, std::vector<LocalIndex> element_parent_subdomains, std::vector<ElementType> element_types, std::vector<std::vector<LocalIndex>> element_connectivities, std::vector<std::vector<LocalIndex>> nodes_in_subconstituents, std::vector<std::vector<LocalIndex>> elements_in_subconstituents, std::vector<std::vector<CoordType>> lcs_parameters_in_subconstituents)
	{
		if (write_state_ != WriteState::WritePhase)
			throw _MSG_EXCEPT_C("Attempted to write a subdomain while not in write phase.  If in parallel, be sure to call start_parallel_write_phase() after queuing the information.");

		if (write_mode_ == WriteMode::Parallel) {
			auto match_iter = std::find_if(subdomain_infos_.begin(), subdomain_infos_.end(),
				[subdomain_index = subdomain_info.subdomain_index](const SubdomainInfo<GlobalIndex, LocalIndex, CoordType>& x) -> bool {
				return x.subdomain_index == subdomain_index;
			});
			if (match_iter == subdomain_infos_.end())
				throw _MSG_EXCEPT_C("Data was provided for subdomain " + std::to_string(subdomain_info.subdomain_index) + " but this processor does not contain the info for it.");
		}
		else {
			if (header_.subdomain_offsets.size() != subdomain_info.subdomain_index)
				throw _MSG_EXCEPT_C("During a serial write, the subdomains were attempted to be written non-sequentially. Subdomain " + std::to_string(header_.subdomain_offsets.size()) + " expected, but " + std::to_string(subdomain_info.subdomain_index) + " given.");
		}

		// Check to make sure the node vectors are the right size
		if (subdomain_info.num_nodes != node_global_inds.size())
			throw _MSG_EXCEPT_C("The node global ids given for subdomain " + std::to_string(subdomain_info.subdomain_index) + " do not match the size expected.");
		if (subdomain_info.num_nodes != node_parent_subdomains.size())
			throw _MSG_EXCEPT_C("The node parent subdomains given for subdomain " + std::to_string(subdomain_info.subdomain_index) + " do not match the size expected.");
		if (subdomain_info.num_nodes * header_.dimensionality != node_coords.size())
			throw _MSG_EXCEPT_C("The node coordinates given for subdomain " + std::to_string(subdomain_info.subdomain_index) + " do not match the size expected.");
		// Check to make sure the element vectors are the right size
		if (subdomain_info.num_elements != element_global_inds.size())
			throw _MSG_EXCEPT_C("The element global ids given for subdomain " + std::to_string(subdomain_info.subdomain_index) + " do not match the size expected.");
		if (subdomain_info.num_elements != element_types.size())
			throw _MSG_EXCEPT_C("The element types given for subdomain " + std::to_string(subdomain_info.subdomain_index) + " do not match the size expected.");
		if (subdomain_info.num_elements != element_parent_subdomains.size())
			throw _MSG_EXCEPT_C("The element parent subdomains given for subdomain " + std::to_string(subdomain_info.subdomain_index) + " do not match the size expected.");
		if (subdomain_info.num_elements != element_connectivities.size())
			throw _MSG_EXCEPT_C("The element connectivitites given for subdomain " + std::to_string(subdomain_info.subdomain_index) + " do not match the size expected.");

		// Figure out the size on disk needed by each component
		std::vector<std::streamoff> subconstituent_offsets;
		std::streamoff curr_offset = 0;
		for (auto sc : subdomain_info.subconstituents) {
			subconstituent_offsets.push_back(curr_offset);
			curr_offset += sc.get_size_needed();
		}

		// If in parallel mode, part of the file is already reserved for this subdomain
		if (write_mode_ == WriteMode::Parallel)
			out_stream_.seekp(header_.subdomain_offsets[subdomain_info.subdomain_index]);

		std::streamoff start_pos = out_stream_.tellp();

		// Write the high-level information about the subdomain
		IOUtils::binary_write(out_stream_, subdomain_info.subdomain_index);
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
		for (auto sc_ind = 0; sc_ind < subdomain_info.subconstituents.size(); sc_ind++) {
			auto& sc = subdomain_info.subconstituents[sc_ind];

			// Check to make sure everything is the right size
			if (sc.num_nodes_in_subconstituent != nodes_in_subconstituents[sc_ind].size())
				throw _MSG_EXCEPT_C("The nodes given for constituent " + std::to_string(sc.constituent_id) + " in subdomain " + std::to_string(subdomain_info.subdomain_index) + " do not match the size expected.");
			if (sc.num_elements_in_subconstituent != elements_in_subconstituents[sc_ind].size())
				throw _MSG_EXCEPT_C("The elements given for constituent " + std::to_string(sc.constituent_id) + " in subdomain " + std::to_string(subdomain_info.subdomain_index) + " do not match the size expected.");
			if (sc.get_total_num_lcs_parameters() != lcs_parameters_in_subconstituents[sc_ind].size())
				throw _MSG_EXCEPT_C("The LCS parameters given for constituent " + std::to_string(sc.constituent_id) + " in subdomain " + std::to_string(subdomain_info.subdomain_index) + " do not match the size expected.");

			IOUtils::binary_write(out_stream_, sc.constituent_id);
			IOUtils::binary_write(out_stream_, sc.num_nodes_in_subconstituent);
			IOUtils::binary_write(out_stream_, nodes_in_subconstituents[sc_ind]);
			IOUtils::binary_write(out_stream_, sc.num_elements_in_subconstituent);
			IOUtils::binary_write(out_stream_, elements_in_subconstituents[sc_ind]);
			IOUtils::binary_write(out_stream_, static_cast<std::underlying_type<LCSAssignment>::type>(sc.lcs_assignment));
			IOUtils::binary_write(out_stream_, static_cast<std::underlying_type<LCSParameterization>::type>(sc.lcs_parameterization));
			IOUtils::binary_write(out_stream_, lcs_parameters_in_subconstituents[sc_ind]);
		}

		std::streamoff end_pos = out_stream_.tellp();

		if (write_mode_ == WriteMode::Parallel) {
			auto expected_size = subdomain_info.get_size_needed(header_.dimensionality);
			if (end_pos - start_pos != expected_size)
				throw _MSG_EXCEPT_C("The data written for subdomain " + std::to_string(subdomain_info.subdomain_index) + " exceeded the space allocated for it."
					+ " Expected size of " + std::to_string(expected_size) + " but actually ended at " + std::to_string(end_pos - start_pos) + ".");
		}
		else {
			header_.subdomain_offsets.push_back(start_pos);
		}
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::finalize()
	{
		out_stream_.close();

		std::ofstream json_component(file_path_ + ".json");
		json j = header_;
		json_component << std::setw(4) << j << std::endl;
		json_component.close();

		write_state_ = WriteState::Finalized;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline std::streamoff MeshWriter<GlobalIndex, LocalIndex, CoordType>::calculate_offsets()
	{

#ifdef HAVE_MPI
		// The root rank will get the entire list of subdomain info, which will allow it
		// to calculate the appropriate offsets.
		auto all_subdomain_info = MPIUtils::gather_vectors(subdomain_infos_);
#else
		// If this was not compiled using MPI, the the process must have all of the data already
		auto all_subdomain_info = subdomain_infos_;
#endif

		// Compute the size needed for each domain, and save the size required
		// for each subdomain within an ordered vector (this means that the
		// subdomains will ordered by index within the binary file)
		std::vector<std::streamoff> subdomain_byte_sizes(all_subdomain_info.size(), 0);
		for (auto& si : all_subdomain_info) {
			subdomain_byte_sizes[si.subdomain_index] = si.get_size_needed(header_.dimensionality);
		}

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