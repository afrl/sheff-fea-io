#include "MeshWriter.h"
#include "MPIUtils.h"

#include <iostream>
#include <iomanip>

using json = nlohmann::json;

#if defined(_WIN32) || defined(_WIN64)
#include <process.h>
#define GET_PID ::_getpid()
#else
#include <unistd.h>
#define GET_PID getpid()
#endif


int main(int *argc, char ***argv)
{
	MPI_Init(argc, argv);

	int comm_rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);
	std::cout << "P: " << comm_rank << " PID: " << GET_PID << std::endl;
	if (comm_rank == 0) {
		int temp;
		std::cout << "Please input a number: " << std::endl;
		std::cin >> temp;
	}
	MPI_Barrier(MPI_COMM_WORLD);

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

	fea_io::SubdomainInfo<int16_t, int16_t, double> subdomain1 = {
		0,
		header.total_num_nodes,
		header.total_num_elements,
		header.extent,
		1,
		{ component1 }
	};

	std::vector<fea_io::SubdomainInfo<int16_t, int16_t, double>> subdomains = { subdomain1 };

	auto all_subdomains = fea_io::MPIUtils::gather_vectors(subdomains);
	//std::vector<int> test = { 0, 1 };
	//auto tests = fea_io::MPIUtils::gather_vectors<int>(test);
	json j = all_subdomains;
	std::cout << std::setw(4) << j << std::endl;

	/*
	json j;
	j["header"] = header;
	j["data_type_sizes"] = data_types;
	j["subdomains"] = subdomains;

	std::cout << std::setw(4) << j << std::endl;

	// Test conversions
	fea_io::MeshHeader<int16_t, int16_t, double> header_check = j["header"];
	assert(header == header_check);
	*/

	MPI_Finalize();
}