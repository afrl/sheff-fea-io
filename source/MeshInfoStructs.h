#pragma once

#include <array>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "MeshEnums.h"
#include "Exceptions.h"

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
		LocalIndex num_constituents;
		std::vector<LocalIndex> constituent_ids;
		std::vector<std::string> constituent_names;
		// Computational division of domain
		LocalIndex total_num_subdomains;
		std::vector<std::streamoff> subdomain_offsets;

		auto tie() const {
			return std::tie(total_num_nodes,
				total_num_elements, dimensionality, extent,
				num_constituents, constituent_ids, constituent_names,
				total_num_subdomains, subdomain_offsets);
		}
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
			{"num_constituents", h.num_constituents},
			{"constituent_ids", h.constituent_ids},
			{"constituent_names", h.constituent_names},
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
		j.at("num_constituents").get_to(h.num_constituents);
		j.at("constituent_ids").get_to(h.constituent_ids);
		j.at("constituent_names").get_to(h.constituent_names);
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

	/// This struct describes all of the informaiton needed to know
	/// the size on disk needed for a constituent.
	/// The data within the binary mesh file is expected to take the form:
	/// - Constituent ID (LocalIndex)
	/// - # nodes in constituent (LocalIndex)
	/// - List of node indices that appears in any element in the
	///   constituent (LocalIndex * # nodes in the constituent)
	///   Note: must contain no duplicates
	/// - # nodes in elements (LocalIndex)
	/// - List of element indices in the constituent
	///   (LocalIndex * # nodes in the constituent)
	/// - Level at which local coordinate systems are assigned (enum : char)
	/// - Parameterization type of local coordinate systems (enum : char)
	/// - List of local coordinate system parmaeters (size varies depending
	///   on the level of assignment and type of parameterization)
	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	struct SubconstituentInfo
	{
		LocalIndex constituent_id;
		LocalIndex num_nodes_in_subconstituent;
		LocalIndex num_elements_in_subconstituent;
		LCSAssignment lcs_assignment;
		LCSParameterization lcs_parameterization;

		/// Returns the number of LCS parameters that will be needed.
		int get_total_num_lcs_parameters() const {
			if (lcs_assignment == LCSAssignment::Constituent)
				return get_num_parameters(lcs_parameterization);
			else if (lcs_assignment == LCSAssignment::Element)
				return num_elements_in_subconstituent * get_num_parameters(lcs_parameterization);
			else if (lcs_assignment == LCSAssignment::Node)
				return num_nodes_in_subconstituent * get_num_parameters(lcs_parameterization);
			else
				throw _MSG_EXCEPT_C("LCS assignment not recognized.");
		}

		/// Returns the number of bytes needed within the binary mesh file for this constituent
		/// within a subdomain.  This method is used when the offsets are being calculated.
		std::streamoff get_size_needed() const {
			// Figure out the total size on disk needed to store
			// everything needed for the component.
			return sizeof(constituent_id)
				+ sizeof(num_nodes_in_subconstituent)
				+ num_nodes_in_subconstituent * sizeof(LocalIndex)
				+ sizeof(num_elements_in_subconstituent)
				+ num_elements_in_subconstituent * sizeof(LocalIndex)
				+ sizeof(lcs_assignment)
				+ sizeof(lcs_parameterization)
				+ this->get_total_num_lcs_parameters() * sizeof(CoordType);
		};
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void to_json(json& j, const SubconstituentInfo<GlobalIndex, LocalIndex, CoordType>& ci)
	{
		j = json{ {"constituent_id", ci.constituent_id},
			{"num_nodes_in_subconstituent", ci.num_nodes_in_subconstituent},
			{"num_elements_in_subconstituent", ci.num_elements_in_subconstituent},
			{"lcs_assignment", ci.lcs_assignment},
			{"lcs_parameterization", ci.lcs_parameterization}
		};
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void from_json(const json& j, SubconstituentInfo<GlobalIndex, LocalIndex, CoordType>& ci)
	{
		j.at("constituent_id").get_to(ci.constituent_id);
		j.at("num_nodes_in_subconstituent").get_to(ci.num_nodes_in_subconstituent);
		j.at("num_elements_in_subconstituent").get_to(ci.num_elements_in_subconstituent);
		j.at("lcs_assignment").get_to(ci.lcs_assignment);
		j.at("lcs_parameterization").get_to(ci.lcs_parameterization);
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	struct SubdomainInfo
	{
		LocalIndex subdomain_index;
		LocalIndex num_nodes;
		LocalIndex num_elements;
		LocalIndex total_num_connectivity_entries;
		std::array<CoordType, 6> extent;
		LocalIndex num_constituents;
		std::vector<SubconstituentInfo<GlobalIndex, LocalIndex, CoordType>> subconstituents;

		/// Returns the number of bytes needed within the binary mesh file
		/// for this subdomain.  This method is used when the offsets are
		/// being calculated.
		std::streamoff get_size_needed(const LocalIndex dimensionality) const {
			// Compute the sizes individually to allow debuggers to check values, but a release compile will
			// optimize most of the code away.
			const std::streamoff size_for_subdomain_header = sizeof(subdomain_index) + sizeof(num_nodes) + sizeof(num_elements) + sizeof(extent);
			// Size of nodes accounts for global id, parent subdomain, and coordintes of each node
			const std::streamoff size_for_nodes = num_nodes * (sizeof(GlobalIndex) + sizeof(LocalIndex) + dimensionality * sizeof(CoordType));
			// Size of element accounts for global id, type, parent subdomain, and connectivity of each element
			const std::streamoff size_for_elements = num_elements * (sizeof(GlobalIndex) + sizeof(ElementType) + sizeof(LocalIndex))
				+ total_num_connectivity_entries * sizeof(LocalIndex);
			const std::streamoff size_for_constituent_header = sizeof(num_constituents) + sizeof(std::streamoff) * num_constituents;
			// Figures out how much space will be needed for the subconstitutents
			std::streamoff size_for_subconstituents = 0;
			for (auto& subconstituent : subconstituents)
				size_for_subconstituents += subconstituent.get_size_needed();

			// Figure out the total size on disk needed to store
			// everything needed for the subdomain.
			return size_for_subdomain_header
				+ size_for_nodes + size_for_elements
				+ size_for_constituent_header
				+ size_for_subconstituents;
		};
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void to_json(json& j, const SubdomainInfo<GlobalIndex, LocalIndex, CoordType>& ss)
	{
		j = json{ {"subdomain_index", ss.subdomain_index},
			{"num_nodes", ss.num_nodes},
			{"num_elements", ss.num_elements},
			{"total_num_connectivity_entries", ss.total_num_connectivity_entries},
			{"extent", ss.extent},
			{"num_constituents", ss.num_constituents},
			{"subconstituents", ss.subconstituents}
		};
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void from_json(const json& j, SubdomainInfo<GlobalIndex, LocalIndex, CoordType>& ss)
	{
		j.at("subdomain_index").get_to(ss.subdomain_index);
		j.at("num_nodes").get_to(ss.num_nodes);
		j.at("num_elements").get_to(ss.num_elements);
		j.at("total_num_connectivity_entries").get_to(ss.total_num_connectivity_entries);
		j.at("extent").get_to(ss.extent);
		j.at("num_constituents").get_to(ss.num_constituents);
		j.at("subconstituents").get_to(ss.subconstituents);
	}

}