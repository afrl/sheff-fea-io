#include "MeshWriter.h"
#include <iostream>
#include <iomanip>

using json = nlohmann::json;

int main()
{
	fea_io::MeshHeader<int16_t, int16_t, double> header = {
		4,
		1,
		2,
		{0, 0, 0, 1, 1, 1},
		2,
		4,
		{0, 0, 1, 1}
	};

	fea_io::DataTypeSizes<int16_t, int16_t, double> data_types;

	fea_io::ComponentInfo<int16_t, int16_t, double> component1 = {
		0,
		header.total_num_nodes,
		header.total_num_elements,
		fea_io::LCSAssignment::Component,
		fea_io::LCSParameterization::Quaternion
	};

	fea_io::SubdomainInfo<int16_t, int16_t, double> subdomain = {
		0,
		header.total_num_nodes,
		header.total_num_elements,
		header.extent,
		1,
		{ component1 }
	};

	std::vector< fea_io::SubdomainInfo<int16_t, int16_t, double>> subdomains = { subdomain };

	json j;
	j["header"] = header;
	j["data_type_sizes"] = data_types;
	j["subdomains"] = subdomains;

	std::cout << std::setw(4) << j << std::endl;

	// Test conversions
	fea_io::MeshHeader<int16_t, int16_t, double> header_check = j["header"];
	assert(header == header_check);
}