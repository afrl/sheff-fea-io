#include "MeshWriter.h"
#include <iostream>
#include <iomanip>

using json = nlohmann::json;

int main()
{
	fea_io::MeshHeader<int16_t, int16_t, double> h = { 4, 1, 2, {0, 0, 0, 1, 1, 1}, 2, 4, {0, 0, 1, 1} };
	json j;
	j["header"] = h;
	std::cout << std::setw(4) << j << std::endl;
	fea_io::MeshHeader<int16_t, int16_t, double> h2 = j["header"];
	assert(h == h2);

	fea_io::DataTypeSizes<int16_t, int16_t, double> dti;
	j["data_type_sizes"] = dti;
	std::cout << std::setw(4) << j << std::endl;
}