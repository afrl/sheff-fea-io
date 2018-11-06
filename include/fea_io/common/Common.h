#pragma once

#include <nlohmann/json.hpp>

namespace fea_io
{
	using json = nlohmann::json;

	enum class WriteMode : char {
		Serial,
		Parallel
	};

	enum class WriteState : char {
		Uninitialized,
		QueuePhase,
		WritePhase,
		Finalized
	};

	struct DataTypeSizes
	{
		int8_t global_index_size;
		int8_t local_index_size;
		int8_t coord_type_size;

		template<typename GlobalIndex, typename LocalIndex, typename CoordType>
		void deduce() {
			global_index_size = sizeof(GlobalIndex);
			local_index_size = sizeof(LocalIndex);
			coord_type_size = sizeof(CoordType);
		}

		bool is_large_enough(const DataTypeSizes& other) {
			return global_index_size >= other.global_index_size &&
				local_index_size >= other.local_index_size &&
				coord_type_size >= other.coord_type_size;
		}
	};

	void to_json(json& j, const DataTypeSizes& dti)
	{
		j = json{ {"global_index_size", dti.global_index_size},
			{"local_index_size", dti.local_index_size},
			{"coord_type_size", dti.coord_type_size}
		};
	}

	void from_json(const json& j, DataTypeSizes& dti)
	{
		j.at("global_index_size").get_to(dti.global_index_size);
		j.at("local_index_size").get_to(dti.local_index_size);
		j.at("coord_type_size").get_to(dti.coord_type_size);
	}
}