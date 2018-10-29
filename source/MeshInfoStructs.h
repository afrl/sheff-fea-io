#pragma once

#include <array>
#include <numeric>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "MeshEnums.h"
#include "Exceptions.h"

namespace fea_io
{
	using json = nlohmann::json;

	template<typename LocalIndex>
	struct ConstituentMetaInfo
	{
		LocalIndex id;
		std::string name;
		LCSAssignment lcs_assignment;
		LCSParameterization lcs_parameterization;

		auto tie() const {
			return std::tie(id, name, lcs_assignment, lcs_parameterization);
		}
	};

	template<typename LocalIndex>
	void to_json(json& j, const ConstituentMetaInfo<LocalIndex>& h)
	{
		j = json{ {"id", h.id},
			{"name", h.name},
			{"lcs_assignment", h.lcs_assignment},
			{"lcs_parameterization", h.lcs_parameterization}
		};
	}

	template<typename LocalIndex>
	void from_json(const json& j, ConstituentMetaInfo<LocalIndex>& h)
	{
		j.at("id").get_to(h.id);
		j.at("name").get_to(h.name);
		j.at("lcs_assignment").get_to(h.lcs_assignment);
		j.at("lcs_parameterization").get_to(h.lcs_parameterization);
	}

	template<typename CoordType>
	struct Extent
	{
		std::array<CoordType, 3> min;
		std::array<CoordType, 3> max;

		CoordType& x_min() { return min[0]; }
		CoordType& y_min() { return min[1]; }
		CoordType& z_min() { return min[2]; }
		CoordType& x_max() { return max[0]; }
		CoordType& y_max() { return max[1]; }
		CoordType& z_max() { return max[2]; }

		const CoordType& x_min() const { return min[0]; }
		const CoordType& y_min() const { return min[1]; }
		const CoordType& z_min() const { return min[2]; }
		const CoordType& x_max() const { return max[0]; }
		const CoordType& y_max() const { return max[1]; }
		const CoordType& z_max() const { return max[2]; }

		Extent() {
			min = { std::numeric_limits<CoordType>::max(),
				std::numeric_limits<CoordType>::max(),
				std::numeric_limits<CoordType>::max() };
			max = { std::numeric_limits<CoordType>::min(),
				std::numeric_limits<CoordType>::min(),
				std::numeric_limits<CoordType>::min() };
		}
		Extent(const std::array<CoordType, 3> min_a, const std::array<CoordType, 3> max_a) {
			min = min_a;
			max = max_a;
		}

		auto tie() const {
			return std::tie(min, max);
		}
	};

	template<typename CoordType>
	void to_json(json& j, const Extent<CoordType>& e)
	{
		j = json{ {"x_min", e.x_min()},
			{"y_min", e.y_min()},
			{"z_min", e.z_min()},
			{"x_max", e.x_max()},
			{"y_max", e.y_max()},
			{"z_max", e.z_max()}
		};
	}

	template<typename CoordType>
	void from_json(const json& j, Extent<CoordType>& e)
	{
		j.at("x_min").get_to(e.x_min());
		j.at("y_min").get_to(e.y_min());
		j.at("z_min").get_to(e.z_min());
		j.at("x_max").get_to(e.x_max());
		j.at("y_max").get_to(e.y_max());
		j.at("z_max").get_to(e.z_max());
	}
	

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	struct MeshMetaInfo
	{
		GlobalIndex total_num_nodes;
		GlobalIndex total_num_elements;
		LocalIndex dimensionality;
		Extent<CoordType> extent;
		std::vector<ConstituentMetaInfo<LocalIndex>> constituents;
		LocalIndex total_num_subdomains;
		std::vector<std::streamoff> subdomain_offsets;

		auto tie() const {
			return std::tie(total_num_nodes,
				total_num_elements, dimensionality, extent,
				constituents, total_num_subdomains, subdomain_offsets);
		}
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	bool operator ==(const MeshMetaInfo<GlobalIndex, LocalIndex, CoordType>& lhs, const MeshMetaInfo<GlobalIndex, LocalIndex, CoordType>& rhs)
	{
		return lhs.tie() == rhs.tie();
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void to_json(json& j, const MeshMetaInfo<GlobalIndex, LocalIndex, CoordType>& h)
	{
		j = json{ {"total_num_nodes", h.total_num_nodes},
			{"total_num_elements", h.total_num_elements},
			{"dimensionality", h.dimensionality},
			{"extent", h.extent},
			{"constituents", h.constituents},
			{"total_num_subdomains", h.total_num_subdomains},
			{"subdomain_offsets", h.subdomain_offsets}
		};
	}

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	void from_json(const json& j, MeshMetaInfo<GlobalIndex, LocalIndex, CoordType>& h)
	{
		j.at("total_num_nodes").get_to(h.total_num_nodes);
		j.at("total_num_elements").get_to(h.total_num_elements);
		j.at("dimensionality").get_to(h.dimensionality);
		j.at("extent").get_to(h.extent);
		j.at("constituents").get_to(h.constituents);
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

	template<typename LocalIndex>
	struct SubconstituentInfo
	{
		LocalIndex constituent_id;
		LocalIndex num_nodes_in_subconstituent;
		LocalIndex num_elements_in_subconstituent;
	};

	template<typename LocalIndex>
	void to_json(json& j, const SubconstituentInfo<LocalIndex>& ci)
	{
		j = json{ {"constituent_id", ci.constituent_id},
			{"num_nodes_in_subconstituent", ci.num_nodes_in_subconstituent},
			{"num_elements_in_subconstituent", ci.num_elements_in_subconstituent}
		};
	}

	template<typename LocalIndex>
	void from_json(const json& j, SubconstituentInfo<LocalIndex>& ci)
	{
		j.at("constituent_id").get_to(ci.constituent_id);
		j.at("num_nodes_in_subconstituent").get_to(ci.num_nodes_in_subconstituent);
		j.at("num_elements_in_subconstituent").get_to(ci.num_elements_in_subconstituent);
	}

	/// Returns the number of bytes needed within the binary mesh file for this constituent
	/// within a subdomain.  This method is used when the offsets are being calculated.
	template<typename LocalIndex, typename CoordType>
	static std::streamoff get_size_for_subconstituent(
		const LocalIndex num_nodes_in_subconstituent,
		const LocalIndex num_elements_in_subconstituent,
		const LCSAssignment lcs_assignment,
		const LCSParameterization lcs_parameterization)
	{
		std::streamoff num_lcs_parameters = 0;
		if (lcs_assignment == LCSAssignment::Constituent)
			num_lcs_parameters = get_num_parameters(lcs_parameterization);
		else if (lcs_assignment == LCSAssignment::Element)
			num_lcs_parameters = num_elements_in_subconstituent * get_num_parameters(lcs_parameterization);
		else if (lcs_assignment == LCSAssignment::Node)
			num_lcs_parameters = num_nodes_in_subconstituent * get_num_parameters(lcs_parameterization);
		else
			throw _MSG_EXCEPT("LCS assignment not recognized.");

		return sizeof(LocalIndex) // constituent id
			+ sizeof(num_nodes_in_subconstituent)
			+ num_nodes_in_subconstituent * sizeof(LocalIndex)
			+ sizeof(num_elements_in_subconstituent)
			+ num_elements_in_subconstituent * sizeof(LocalIndex)
			+ num_lcs_parameters * sizeof(CoordType);
	};

	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	struct SubdomainInfo
	{
		LocalIndex subdomain_index;
		LocalIndex num_nodes;
		LocalIndex num_elements;
		LocalIndex total_num_connectivity_entries;
		Extent<CoordType> extent;
		LocalIndex num_constituents;
		std::vector<SubconstituentInfo<LocalIndex>> subconstituents;

		/// Returns the number of bytes needed within the binary mesh file
		/// for this subdomain.  This method is used when the offsets are
		/// being calculated.
		std::streamoff get_size_needed(const MeshMetaInfo<GlobalIndex, LocalIndex, CoordType>& meta_info) const {
			// Compute the sizes individually to allow debuggers to check values, but a release compile will
			// optimize most of the code away.
			const std::streamoff size_for_subdomain_header = sizeof(subdomain_index) + sizeof(num_nodes) + sizeof(num_elements) + sizeof(extent);
			// Size of nodes accounts for global id, parent subdomain, and coordintes of each node
			const std::streamoff size_for_nodes = num_nodes * (sizeof(GlobalIndex) + sizeof(LocalIndex) + meta_info.dimensionality * sizeof(CoordType));
			// Size of element accounts for global id, type, parent subdomain, and connectivity of each element
			const std::streamoff size_for_elements = num_elements * (sizeof(GlobalIndex) + sizeof(ElementType) + sizeof(LocalIndex))
				+ total_num_connectivity_entries * sizeof(LocalIndex);
			const std::streamoff size_for_constituent_header = sizeof(num_constituents) + sizeof(std::streamoff) * num_constituents;
			// Figures out how much space will be needed for the subconstitutents
			std::streamoff size_for_subconstituents = 0;
			for (auto& subconstituent : subconstituents) {
				// Note: it was already tested that the constituent id exists in the meta_info
				auto constituent_iter = std::find_if(meta_info.constituents.begin(), meta_info.constituents.end(),
					[id = subconstituent.constituent_id](const ConstituentMetaInfo<LocalIndex>& x) -> bool {
					return x.id == id;
				});
				if (constituent_iter == meta_info.constituents.end())
					throw _MSG_EXCEPT_C("In subdomain " + std::to_string(subdomain_index) + ", constituent ID " + std::to_string(subconstituent.constituent_id) + " was given, but it was not included as a constituent when constructed.");

				size_for_subconstituents += get_size_for_subconstituent<LocalIndex, CoordType>(subconstituent.num_nodes_in_subconstituent,
					subconstituent.num_elements_in_subconstituent, constituent_iter->lcs_assignment, constituent_iter->lcs_parameterization);
			}

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