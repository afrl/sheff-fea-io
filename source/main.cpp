#include "MeshWriter.h"
#include "MPIUtils.h"

#include <functional>
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

#define RUN_TEST(x) run_test(x, #x);

void run_test(const std::function<bool()>& test_function, const std::string function_name)
{
	bool test_return = test_function();
	if (fea_io::MPIUtils::get_local_rank() == 0)
	{
		if (test_return)
			std::cout << "Test " << function_name << " passed." << std::endl;
		else
			std::cout << "Test " << function_name << " failed." << std::endl;
	}
}

bool test_gather_vectors_int()
{
	std::vector<int> data_on_this_proc;
	if (fea_io::MPIUtils::get_local_rank() == 0)
		data_on_this_proc = { 0, 1, 2 };
	else if (fea_io::MPIUtils::get_local_rank() == 1)
		data_on_this_proc = { 3, 4, 5 };
	else
		data_on_this_proc = { 6, 7 };

	// The resulting vector of all the data should be:
	// { 0, 1, 2, 3, 4, 5, 6, 7, 6, 7 ... }
	// Where 6, 7 repeats for N-2 processes
	auto all_data = fea_io::MPIUtils::gather_vectors(data_on_this_proc);

	int num_procs = fea_io::MPIUtils::get_num_local_ranks();
	std::vector<int> expected_data = { 0, 1, 2 };
	if (num_procs > 1) {
		expected_data.push_back(3);
		expected_data.push_back(4);
		expected_data.push_back(5);
	}
	for (auto i = 0; i < num_procs - 2; i++) {
		expected_data.push_back(6);
		expected_data.push_back(7);
	}

	if (fea_io::MPIUtils::get_local_rank() == 0)
		return all_data == expected_data;
	else
		return true;
}

void get_test_subdomain_info(fea_io::MeshHeader<int16_t, int16_t, double>& header, std::vector<fea_io::SubdomainInfo<int16_t, int16_t, double>>& subdomains)
{
	header = {
		9,
		4,
		2,
		{0, 0, 0, 1, 1, 0},
		2,
		4,
		{0, 0, 1, 1},
		2,
		{}
	};

	fea_io::DataTypeSizes<int16_t, int16_t, double> data_types;

	fea_io::ComponentInfo<int16_t, int16_t, double> component0 = {
		0,
		4,
		1,
		fea_io::LCSAssignment::Component,
		fea_io::LCSParameterization::Quaternion
	};

	fea_io::ComponentInfo<int16_t, int16_t, double> component1 = {
		1,
		4,
		4,
		fea_io::LCSAssignment::Component,
		fea_io::LCSParameterization::Quaternion
	};

	fea_io::ComponentInfo<int16_t, int16_t, double> component2 = {
		2,
		4,
		1,
		fea_io::LCSAssignment::Element,
		fea_io::LCSParameterization::Quaternion
	};

	fea_io::ComponentInfo<int16_t, int16_t, double> component3 = {
		3,
		4,
		4,
		fea_io::LCSAssignment::Element,
		fea_io::LCSParameterization::Quaternion
	};

	fea_io::SubdomainInfo<int16_t, int16_t, double> subdomain0 = {
		0,
		6,
		2,
		{0, 0, 0, 0.5, 0.5, 0},
		2,
		{ component0, component2 }
	};

	fea_io::SubdomainInfo<int16_t, int16_t, double> subdomain1 = {
		1,
		6,
		2,
		{0.5, 0.5, 0, 1, 1, 0},
		2,
		{ component1, component3 }
	};

	subdomains = { subdomain0, subdomain1 };
}

bool test_mesh_writer_parallel()
{
	fea_io::MeshHeader<int16_t, int16_t, double> header;
	std::vector<fea_io::SubdomainInfo<int16_t, int16_t, double>> all_subdomains;
	get_test_subdomain_info(header, all_subdomains);

	std::vector<fea_io::SubdomainInfo<int16_t, int16_t, double>> subdomain_info_on_this_proc;
	if (fea_io::MPIUtils::get_num_local_ranks() > 1) {
		if (fea_io::MPIUtils::get_local_rank() == 0)
			subdomain_info_on_this_proc = { all_subdomains[0] };
		else if (fea_io::MPIUtils::get_local_rank() == 1)
			subdomain_info_on_this_proc = { all_subdomains[1] };
	}
	else
		subdomain_info_on_this_proc = all_subdomains;

	fea_io::MeshWriter<int16_t, int16_t, double> writer(header,
		fea_io::MeshWriter<int16_t, int16_t, double>::WriteMode::Parallel,
		"test");
	for (auto& si : subdomain_info_on_this_proc)
		writer.queue_subdomain_for_write(si);
	writer.start_parallel_write_phase();

	return true;
}

int main(int argc, char **argv)
{
	fea_io::MPIUtils::init(&argc, &argv);

	// Allow a chance for a debugger to attach
	std::cout << "P: " << fea_io::MPIUtils::get_local_rank() << " PID: " << GET_PID << std::endl;
	if (fea_io::MPIUtils::get_local_rank() == 0) {
		int temp;
		std::cout << "Please input a number: " << std::endl;
		std::cin >> temp;
	}
	MPI_Barrier(MPI_COMM_WORLD);

	RUN_TEST(test_gather_vectors_int);

	RUN_TEST(test_mesh_writer_parallel);

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

	fea_io::MPIUtils::finalize();
}