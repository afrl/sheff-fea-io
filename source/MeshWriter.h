#pragma once

#include <array>
#include <stdint.h>
#include <vector>

#include <nlohmann/json.hpp>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

namespace fea_io
{
	using json = nlohmann::json;

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	struct MeshHeader
	{
		// Top-level mesh statistics
		GlobalIndex total_num_nodes;
		GlobalIndex total_num_elements;
		LocalIndex dimensionality;
		std::array<CoordType, 6> extent;
		// Physical division of domain
		LocalIndex num_clusters;
		LocalIndex total_num_components;
		std::vector<LocalIndex> component_to_cluster;
		// Computational division of domain
		LocalIndex total_num_subdomains;
		std::vector<std::streamoff> subdomain_offsets;

		auto tie() const { return std::tie(total_num_nodes,
			total_num_elements, dimensionality, extent,
			num_clusters, total_num_components, component_to_cluster,
			total_num_subdomains, subdomain_offsets); }
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	bool operator ==(const MeshHeader<GlobalIndex, LocalIndex, CoordType>& lhs, const MeshHeader<GlobalIndex, LocalIndex, CoordType>& rhs)
	{
		return lhs.tie() == rhs.tie();
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void to_json(json& j, const MeshHeader<GlobalIndex, LocalIndex, CoordType>& h)
	{
		j = json{ {"total_num_nodes", h.total_num_nodes},
			{"total_num_elements", h.total_num_elements},
			{"dimensionality", h.dimensionality},
			{"extent", h.extent},
			{"num_clusters", h.num_clusters},
			{"total_num_components", h.total_num_components},
			{"component_to_cluster", h.component_to_cluster},
			{"total_num_subdomains", h.total_num_subdomains},
			{"subdomain_offsets", h.subdomain_offsets}
		};
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void from_json(const json& j, MeshHeader<GlobalIndex, LocalIndex, CoordType>& h)
	{
		j.at("total_num_nodes").get_to(h.total_num_nodes);
		j.at("total_num_elements").get_to(h.total_num_elements);
		j.at("dimensionality").get_to(h.dimensionality);
		j.at("extent").get_to(h.extent);
		j.at("num_clusters").get_to(h.num_clusters);
		j.at("total_num_components").get_to(h.total_num_components);
		j.at("component_to_cluster").get_to(h.component_to_cluster);
		j.at("total_num_subdomains").get_to(h.total_num_subdomains);
		j.at("subdomain_offsets").get_to(h.subdomain_offsets);
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	struct DataTypeSizes
	{
		const int8_t global_index_size = sizeof(GlobalIndex);
		const int8_t local_index_size = sizeof(LocalIndex);
		const int8_t coord_type_size = sizeof(CoordType);
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void to_json(json& j, const DataTypeSizes<GlobalIndex, LocalIndex, CoordType>& dti)
	{
		j = json{ {"global_index_size", dti.global_index_size},
			{"local_index_size", dti.local_index_size},
			{"coord_type_size", dti.coord_type_size}
		};
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void from_json(const json& j, DataTypeSizes<GlobalIndex, LocalIndex, CoordType>& dti)
	{
		j.at("global_index_size").get_to(dti.global_index_size);
		j.at("local_index_size").get_to(dti.local_index_size);
		j.at("coord_type_size").get_to(dti.coord_type_size);
	}

	/// An enum describing what level the local coordinate system will be
	/// assigned for a component.  This could be:
	/// - Component: one LCS for the entire component
	/// - Element: one LCS for each element in the component
	/// - Node: one LCS for each node in the component
	enum class LCSAssignment : char
	{
		Component,
		Element,
		Node
	};

	/// An enum describing what parametrization will be used to describe
	/// a local coordinate system as a rotation from the global coordinate
	/// system.  The different options are:
	/// - Quaternion: 4 parameters (b0, b1, b2, b3)
	/// - AxisAngle: the axis (x, y, z) followed by the angle about the axis
	/// If these systems are unfamiliar, this link may be helpful:
	/// www.tu-berlin.de/fileadmin/fg169/miscellaneous/Quaternions.pdf
	enum class LCSParameterization : char
	{
		Quaternion,
		AxisAngle
		// Perhaps add EulerAngleXXX for the different options
	};

	/// Given a type of local coordinate system parameterization this
	/// function will return the number of paramerters needed to
	/// describe a coordinate system.
	static int get_num_parameters(const LCSParameterization& lcs_p)
	{
		if (lcs_p == LCSParameterization::Quaternion ||
			lcs_p == LCSParameterization::AxisAngle)
			return 4;
		// Euler angles would be 3
		else
			throw 0; // TODO throw a real error
	};

	/// This struct describes all of the informaiton needed to known
	/// the size of a particular component on the disk.
	/// The component data within the binary mesh file is expected to take the form:
	/// - Component index (LocalIndex)
	/// - # nodes in component (LocalIndex)
	/// - List of node indices that appears in any element in the
	///   component (LocalIndex * # nodes in the component)
	/// - # nodes in elements (LocalIndex)
	/// - List of element indices in the component
	///   (LocalIndex * # nodes in the component)
	/// - Level at which local coordinate systems are assigned (enum : char)
	/// - Parameterization type of local coordinate systems (enum : char)
	/// - List of local coordinate system parmaeters (size varies depending
	///   on the level of assignment and type of parameterization)
	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	struct ComponentInfo
	{
		LocalIndex component_index;
		LocalIndex num_nodes_in_component;
		LocalIndex num_elements_in_component;
		LCSAssignment lcs_assignment;
		LCSParameterization lcs_parameterization;

		/// Returns the number of bytes needed within the binary mesh file
		/// for this component.  This method is used when the offsets are
		/// being calculated.
		std::streamoff get_size_needed() const {
			// First figure out how many bytes will be needed
			std::streamoff size_for_lcs_data = 0;
			if (lcs_assignment == LCSAssignment::Component)
				size_for_lcs_data = get_num_parameters(lcs_parameterization) * sizeof(CoordType);
			else if (lcs_assignment == LCSAssignment::Element)
				size_for_lcs_data = num_elements_in_component * get_num_parameters(lcs_parameterization) * sizeof(CoordType);
			if (lcs_assignment == LCSAssignment::Node)
				size_for_lcs_data = num_nodes_in_component * get_num_parameters(lcs_parameterization) * sizeof(CoordType);

			// Figure out the total size on disk needed to store
			// everything needed for the component.
			return sizeof(component_index)
				+ sizeof(num_nodes_in_component)
				+ num_nodes_in_component * sizeof(LocalIndex)
				+ sizeof(num_elements_in_component)
				+ num_elements_in_component * sizeof(LocalIndex)
				+ sizeof(lcs_assignment)
				+ sizeof(lcs_parameterization)
				+ size_for_lcs_data;
		};
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void to_json(json& j, const ComponentInfo<GlobalIndex, LocalIndex, CoordType>& ci)
	{
		j = json{ {"component_index", ci.component_index},
			{"num_nodes_in_component", ci.num_nodes_in_component},
			{"num_elements_in_component", ci.num_elements_in_component},
			{"lcs_assignment", ci.lcs_assignment},
			{"lcs_parameterization", ci.lcs_parameterization}
		};
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void from_json(const json& j, ComponentInfo<GlobalIndex, LocalIndex, CoordType>& ci)
	{
		j.at("component_index").get_to(ci.component_index);
		j.at("num_nodes_in_component").get_to(ci.num_nodes_in_component);
		j.at("num_elements_in_component").get_to(ci.num_elements_in_component);
		j.at("lcs_assignment").get_to(ci.lcs_assignment);
		j.at("lcs_parameterization").get_to(ci.lcs_parameterization);
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	struct SubdomainInfo
	{
		LocalIndex subdomain_index;
		LocalIndex num_nodes;
		LocalIndex num_elements;
		std::array<CoordType, 6> extent;
		LocalIndex num_components;
		std::vector<ComponentInfo<GlobalIndex, LocalIndex, CoordType>> components;

		/// Returns the number of bytes needed within the binary mesh file
		/// for this subdomain.  This method is used when the offsets are
		/// being calculated.
		std::streamoff get_size_needed() const {
			// First figure how much space the components will needed
			std::streamoff size_for_components = 0;
			for (auto& component : components)
				size_for_components += component.get_size_needed();

			// Figure out the total size on disk needed to store
			// everything needed for the subdomain.
			return sizeof(subdomain_index) + sizeof(num_nodes)
				+ sizeof(num_elements) + sizeof(extent)
				+ 0 // Insert size of nodes and elements here
				+ sizeof(num_components)
				+ size_for_components;
		};
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void to_json(json& j, const SubdomainInfo<GlobalIndex, LocalIndex, CoordType>& ss)
	{
		j = json{ {"subdomain_index", ss.subdomain_index},
			{"num_nodes", ss.num_nodes},
			{"num_elements", ss.num_elements},
			{"extent", ss.extent},
			{"num_components", ss.num_components},
			{"components", ss.components}
		};
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void from_json(const json& j, SubdomainInfo<GlobalIndex, LocalIndex, CoordType>& ss)
	{
		j.at("subdomain_index").get_to(ss.subdomain_index);
		j.at("num_nodes").get_to(ss.num_nodes);
		j.at("num_elements").get_to(ss.num_elements);
		j.at("extent").get_to(ss.extent);
		j.at("num_components").get_to(ss.num_components);
		j.at("components").get_to(ss.components);
	}

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
		enum class WriteMode : char {
			Serial,
			Parallel
		};
		enum class WriteState : char {
			QueuePhase,
			WritePhase,
			Finalized
		};

		// Only constructor to use.
		MeshWriter(const MeshHeader<GlobalIndex, LocalIndex, CoordType>& header, const WriteMode& write_mode);

		// if in parallel mode, then this needs to be called for each subdomain
		// before writing begins.  An error will be thrown otherwise.
		void queue_subdomain_for_write(const SubdomainInfo<GlobalIndex, LocalIndex, CoordType>& subdomain_info);
		// Write one subdomain
		void write_subdomain(/**/);

		void finalize();

	private:
		MeshWriter() = delete;
		
		MeshHeader<GlobalIndex, LocalIndex, CoordType> header_;
		WriteMode write_mode_;
		WriteState write_state_;

		// Variables needed to track changes. Save positions as write_subdomain
		// is called, and use these positions to update the lookup tables at
		// the beginning of the destructor being called.
		std::vector<LocalIndex> updated_subdomain_ids_;
		std::vector<std::streamoff> updated_subdomain_offsets_;
		std::vector<SubdomainInfo<GlobalIndex, LocalIndex, CoordType>> subdomain_info_;
		
		// If MPI is available, then this will sync all the subdomain across
		// the MPI ranks.  Works by converting the struct to json, serializing
		// the data, and communicating the bytes over MPI.
		void sync_subdomain_info();
		// Given the current state of subdomain_info_, this method will calculate
		// the offsets that each subdomain will have.  header_ will be updated
		// accordingly.
		void calculate_offsets();
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline MeshWriter<GlobalIndex, LocalIndex, CoordType>::MeshWriter(const MeshHeader<GlobalIndex, LocalIndex, CoordType>& header, const WriteMode & write_mode)
	{
		header_ = header;
		write_mode_ = write_mode;

		if (write_mode == WriteMode::Serial)
			write_state_ = WriteState::WritePhase;
		else if (write_mode == WriteMode::Parallel)
			write_state_ = WriteState::QueuePhase;
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::sync_subdomain_info()
	{
		// Create a JSON array from the subdomain info objects
		json j = subdomain_info_;
		// Serialize the JSON data using CBOR (concise bindary object
		// representation), which will be communicated over MPI
		std::vector<std::uint8_t> serialized_subdomain_info = json::to_cbor(j);

		// Get information about the MPI communicator
		int comm_size;
		MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
		int comm_rank;
		MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);

		// Communicate the size of the data on each rank that will be communciated
		// Assume no two processors have duplicates of the subdomains
		int num_bytes_for_this_proc = serialized_subdomain_info.size();
		std::vector<int> num_bytes_for_each_proc(comm_size + 1);
		MPI_Allgather(&num_bytes_for_this_proc, 1, MPI_INT,
			&num_bytes_for_each_proc[0], 1, MPI_INT, MPI_COMM_WORLD);

		// TODO
		int num_total_bytes_needed = 0;
		for (auto num_bytes : num_bytes_for_each_proc)
			num_total_bytes_needed += num_bytes;

		// TODO
		std::vector<char> recv_buffer(num_total_bytes_needed);
		std::vector<int> recv_counts(comm_size);
		std::vector<int> recv_offsets(comm_size + 1);
		int offset = 0;
		for (auto i = 0; i < comm_size; i++) {
			recv_counts[i] = num_bytes_for_each_proc[i];
			recv_offsets[i] = offset;
			offset += recv_counts[i];
		}
		recv_offsets.back() = offset;

		// Set the 
		MPI_Allgatherv(&serialized_subdomain_info[0],
			static_cast<int>(serialized_subdomain_info.size()), MPI_CHAR,
			&recv_buffer[0], &recv_counts[0], &recv_offsets[0], MPI_CHAR,
			MPI_COMM_WORLD);

		// Communciate the number of subdomain_info objects
		// Communicate the size of each subdomain_info object

		// On rank 0, deserialize the subdomain info from each process
		if (comm_rank == 0) {
			subdomain_info_.clear();
			// TODO serialize
		}
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	inline void MeshWriter<GlobalIndex, LocalIndex, CoordType>::calculate_offsets()
	{
		// Compute the size needed for each domain, and save the size required
		// for each subdomain within an ordered vector (this means that the
		// subdomains will ordered by index within the binary file)
		std::vector<std::streamoff> subdomain_byte_sizes(subdomain_info_.size(), 0);
		for (auto& si : subdomain_info_)
			subdomain_byte_sizes[si.subdomain_index] = si.get_size_needed();

		// Compute the offsets of each subdomain (starting at 0)
		header_.subdomain_offsets.resize(subdomain_byte_sizes.size());
		auto prev_size = 0;
		for (auto i = 0; i < subdomain_byte_sizes.size(); i++) {
			header_.subdomain_offsets[i] = prev_size + subdomain_byte_sizes[i];
			prev_size = header_.subdomain_offsets[i];
		}
	}

}