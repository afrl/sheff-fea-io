#include <functional>
#include <iostream>
#include <iomanip>

#include "fea_io/common/Comparators.h"
#include "fea_io/common/MPIUtils.h"
#include "fea_io/data/DataReader.h"
#include "fea_io/data/DataWriter.h"
#include "fea_io/mesh/MeshParWriter.h"
#include "fea_io/mesh/MeshReader.h"

using json = nlohmann::json;
using namespace fea_io;

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
	if (MPIUtils::get_local_rank() == 0)
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
	if (MPIUtils::get_local_rank() == 0)
		data_on_this_proc = { 0, 1, 2 };
	else if (MPIUtils::get_local_rank() == 1)
		data_on_this_proc = { 3, 4, 5 };
	else
		data_on_this_proc = { 6, 7 };

	// The resulting vector of all the data should be:
	// { 0, 1, 2, 3, 4, 5, 6, 7, 6, 7 ... }
	// Where 6, 7 repeats for N-2 processes
	auto all_data = MPIUtils::gather_vectors(data_on_this_proc);

	int num_procs = MPIUtils::get_num_local_ranks();
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

	if (MPIUtils::get_local_rank() == 0)
		return all_data == expected_data;
	else
		return true;
}

std::vector<DataArrayInfo<int16_t, float>> get_test_data_info()
{
	std::vector<DataArrayInfo<int16_t, float>> data_info = {
		{
			0,
			DataArrayType::Node,
			2,
			"Displacement",
			{"u", "v"}
		},
		{
			1,
			DataArrayType::NodeByComponent,
			3,
			"StressGlobal",
			{"xx", "yy", "xy"}
		},
		{
			2,
			DataArrayType::QuadPointByElement,
			3,
			"StressGlobal",
			{"xx", "yy", "xy"}
		}
	};
	return data_info;
}

std::vector<std::vector<float>> get_part0_data()
{
	std::vector<std::vector<float>> data = {
		{0, 0, 0.5, 0, 0, 0, 0.5, 0, 0, 0, 0.5, 0},
		{1e6, 0, 0, 1e6, 0, 0, 1e6, 0, 0, 1e6, 0, 0, 1e6, 0, 0, 1e6, 0, 0, 1e6, 0, 0, 1e6, 0, 0},
		{1e6, 0, 0, 1e6, 0, 0, 1e6, 0, 0, 1e6, 0, 0, 1e6, 0, 0, 1e6, 0, 0, 1e6, 0, 0, 1e6, 0, 0}
	};
	return data;
}

std::vector<std::vector<float>> get_part1_data()
{
	std::vector<std::vector<float>> data = {
		{0.5, 0, 0.75, 0, 0.5, 0, 0.75, 0, 0.5, 0, 0.75, 0},
		{2e6, 0, 0, 2e6, 0, 0, 2e6, 0, 0, 2e6, 0, 0, 2e6, 0, 0, 2e6, 0, 0, 2e6, 0, 0, 2e6, 0, 0},
		{2e6, 0, 0, 2e6, 0, 0, 2e6, 0, 0, 2e6, 0, 0, 2e6, 0, 0, 2e6, 0, 0, 2e6, 0, 0, 2e6, 0, 0}
	};
	return data;
}

void get_test_info(MeshMetaInfo<int16_t, int16_t, float>& meta_info, std::vector<PartitionInfo<int16_t, int16_t, float>>& partitions)
{
	meta_info = {
		9,
		4,
		2,
		{ {0, 0, 0}, {1, 1, 0} },
		{ { 0, "Component0", LCSAssignment::Component, LCSParameterization::Quaternion },
		  { 1, "Component1", LCSAssignment::Element, LCSParameterization::Quaternion } },
		2,
		{}
	};

	DataTypeSizes data_types;
	data_types.deduce<int16_t, int16_t, float>();

	SubcomponentInfo<int16_t> sc0 = { 0, 4, 1 };
	SubcomponentInfo<int16_t> sc1 = { 0, 4, 1 };
	SubcomponentInfo<int16_t> sc2 = { 1, 4, 1 };
	SubcomponentInfo<int16_t> sc3 = { 1, 4, 1 };

	PartitionInfo<int16_t, int16_t, float> part0 = {
		0,
		6,
		2,
		8,
		Extent<float>(),
		2,
		{ sc0, sc2 }
	};

	PartitionInfo<int16_t, int16_t, float> part1 = {
		1,
		6,
		2,
		8,
		Extent<float>(),
		2,
		{ sc1, sc3 }
	};

	partitions = { part0, part1 };
}

auto get_part0()
{
	Partition<int16_t, int16_t, float> partition = {
		0,
		{ 0, 1, 3, 4, 6, 7 },
		{ 0, 0, 0, 0, 0, 0 },
		{ 0, 1, 0.5, 1, 0, 0.5, 0.5, 0.5, 0, 0, 0.5, 0 },
		{ 0, 2 },
		{ 0, 0 },
		{ ElementType::Q2D4N, ElementType::Q2D4N },
		{ {0, 2, 3, 1}, {2, 4, 5, 3} },
		{ 
			{ 0, {0, 1, 2, 3}, {0}, {1, 0, 0, 0} },
			{ 1, {2, 3, 4, 5}, {1}, {1, 0, 0, 0} },
		}
	};
	return partition;
}

auto get_part1()
{
	Partition<int16_t, int16_t, float> partition = {
		1,
		{ 1, 2, 4, 5, 7, 8 },
		{ 0, 1, 0, 1, 0, 1 },
		{ 0.5, 1, 1, 1, 0.5, 0.5, 1, 0.5, 0.5, 0, 1, 0 },
		{ 1, 3 },
		{ 1, 1 },
		{ ElementType::Q2D4N, ElementType::Q2D4N },
		{ { 0, 2, 3, 1 }, { 2, 4, 5, 3 } },
		{
			{ 0, {0, 1, 2, 3}, {0}, {1, 0, 0, 0} },
			{ 1, {2, 3, 4, 5}, {1}, {1, 0, 0, 0} },
		}
	};
	return partition;
}

bool test_mesh_writer_serial()
{
	if (MPIUtils::get_local_rank() == 0) {
		MeshMetaInfo<int16_t, int16_t, float> meta_info;
		std::vector<PartitionInfo<int16_t, int16_t, float>> all_partitions;
		get_test_info(meta_info, all_partitions);

		MeshWriter<int16_t, int16_t, float> writer;
		writer.initialize("test_serial", meta_info);

		writer.write_partition(get_part0());
		writer.write_partition(get_part1());
	}

	return true;
}

bool test_mesh_writer_parallel()
{
	MeshMetaInfo<int16_t, int16_t, float> meta_info;
	std::vector<PartitionInfo<int16_t, int16_t, float>> all_partitions;
	get_test_info(meta_info, all_partitions);

	std::vector<PartitionInfo<int16_t, int16_t, float>> part_info_on_this_proc;
	if (MPIUtils::get_num_local_ranks() > 1) {
		if (MPIUtils::get_local_rank() == 0)
			part_info_on_this_proc = { all_partitions[0] };
		else if (MPIUtils::get_local_rank() == 1)
			part_info_on_this_proc = { all_partitions[1] };
	}
	else
		part_info_on_this_proc = all_partitions;

	MeshParWriter<int16_t, int16_t, float> writer;
	writer.initialize("test_parallel", meta_info);
	for (auto& pi : part_info_on_this_proc)
		writer.queue_partition_for_write(pi);
	writer.start_parallel_write_phase();

	if (MPIUtils::get_num_local_ranks() > 1) {
		if (MPIUtils::get_local_rank() == 0)
			writer.write_partition(get_part0());
		else if (MPIUtils::get_local_rank() == 1)
			writer.write_partition(get_part1());
	}
	else {
		writer.write_partition(get_part0());
		writer.write_partition(get_part1());
	}

	return true;
}

bool test_mesh_reader()
{
	if (MPIUtils::get_local_rank() == 0) {
		MeshReader<int32_t, int32_t, double> reader("test_serial");
		auto part0 = reader.read_partition(0);
		auto part1 = reader.read_partition(1);
		return (part0 == get_part0())
			&& (part1 == get_part1());
	}

	return true;
}

bool test_data_writer()
{
	if (MPIUtils::get_local_rank() == 0) {
		MeshMetaInfo<int16_t, int16_t, float> meta_info;
		std::vector<PartitionInfo<int16_t, int16_t, float>> all_partitions;
		get_test_info(meta_info, all_partitions);
		auto data_info = get_test_data_info();

		DataWriter<int16_t, int16_t, float> writer;
		writer.initialize("test_serial_data", meta_info, all_partitions, {8, 8}, data_info);

		auto data0 = get_part0_data();
		auto data1 = get_part1_data();

		for (auto i = 0; i < data0.size(); i++) {
			writer.write(0, i, static_cast<int16_t>(data0[i].size()) / data_info[i].num_comps, &data0[i][0]);
			writer.write(1, i, static_cast<int16_t>(data1[i].size()) / data_info[i].num_comps, &data1[i][0]);
		}
	}

	return true;
}

bool test_data_reader()
{
	if (MPIUtils::get_local_rank() == 0) {
		DataReader<int32_t, int32_t, double> reader("test_serial_data");
		auto data0_0 = reader.read(0, 0);
		auto data0_1 = reader.read(0, 1);
		auto data0_2 = reader.read(0, 2);
		auto data1_0 = reader.read(1, 0);
		auto data1_1 = reader.read(1, 1);
		auto data1_2 = reader.read(1, 2);

		auto data0 = get_part0_data();
		auto data1 = get_part1_data();

		return compare_vec(data0_0, data0[0])
			&& compare_vec(data0_1, data0[1])
			&& compare_vec(data0_2, data0[2])
			&& compare_vec(data1_0, data1[0])
			&& compare_vec(data1_1, data1[1])
			&& compare_vec(data1_2, data1[2]);
	}

	return true;
}

void illustrative_serial()
{
	// This sets the types to be used for all the variables in the mesh
	// The file will actually write/read with the provided precision, and
	// one could write with one level of precision and read with another
	// (a warning will be output if you write with a higher precision than
	// what you are reading).
	//      Global indices   Local indices   Coordinates
	//                  \          |          / 
	typedef MeshWriter<int64_t, int32_t, double> MeshWriter;

	// Construct a writer object and tell it the high-level information
	// about the mesh.  Note: we could specialize a class for serial and
	// one for parallel writing.  It would simplify how much you need to
	// give it for the serial case.  Would that be preferrable?
	MeshWriter writer;
	writer.initialize(
		"/some/filepath/a_cool_mesh", // Where to put the json/binary files
		9, // total # nodes
		4, // total # elements
		2, // dimensionality
		{ { 0, 0, 0}, {1, 1, 0} }, // extent
		{ 0, 1 }, // component IDs
		{ "Component0", "Component1" }, // component names
		{ LCSAssignment::Component, LCSAssignment::Element }, // level at which local
		// coordinate systems are assigned
		{ LCSParameterization::Quaternion, LCSParameterization::Quaternion }, // type
		// of parameterization used for the local coordinate sytem
		2); // # subdomains
	//writer.write_partition(/* A bunch of parameters describing the mesh
	//					   and local coordinate systems.*/);
}


void illustrative_parallel()
{
	// This sets the types to be used for all the variables in the mesh
	// The file will actually write/read with the provided precision, and
	// one could write with one level of precision and read with another
	// (a warning will be output if you write with a higher precision than
	// what you are reading).
	//      Global indices   Local indices   Coordinates
	//                  \          |          / 
	typedef MeshParWriter<int64_t, int32_t, double> MeshWriter;

	// Construct a writer object and tell it the high-level information
	// about the mesh.  Note: we could specialize a class for serial and
	// one for parallel writing.  It would simplify how much you need to
	// give it for the serial case.  Would that be preferrable?
	MeshWriter writer;
	writer.initialize(
		"/some/filepath/a_cool_mesh", // Where to put the json/binary files
		9, // total # nodes
		4, // total # elements
		2, // dimensionality
		{ { 0, 0, 0}, {1, 1, 0} }, // extent
		{ 0, 1 }, // component IDs
		{ "Component0", "Component1" }, // component names
		{ LCSAssignment::Component, LCSAssignment::Element }, // level at which local
		// coordinate systems are assigned
		{ LCSParameterization::Quaternion, LCSParameterization::Quaternion }, // type
		// of parameterization used for the local coordinate sytem
		2); // # subdomains

	// Before you write, you have to queue all the subdomains so the
	// processors can figure out how to preallocate the file and write
	// in parallel.  Each processor would call this for each subdomain
	// they are responsible for writing.  This means 4 processors could
	// write however many subdomains they want, or you can have each processor
	// write one subdomain each.
	//writer.queue_partition_for_write(/* Several parameters describing
	//								 high-level info about the subdomain */);
	
	// You call this to trigger the MPI communciation and preallocation of
	// the file.  It could be automatically called on the first write call.
	// Would that be preferred?
	writer.start_parallel_write_phase();

	// Each processor calls this for each subdomain they should write
	//writer.write_partition(/* A bunch of parameters describing the mesh
	//					   and local coordinate systems.*/);
}

int main(int argc, char **argv)
{
	MPIUtils::init(&argc, &argv);

	// Allow a chance for a debugger to attach
	std::cout << "P: " << MPIUtils::get_local_rank() << " PID: " << GET_PID << std::endl;
	if (MPIUtils::get_local_rank() == 0) {
		int temp;
		std::cout << "Please input a number: " << std::endl;
		std::cin >> temp;
	}
	MPI_Barrier(MPI_COMM_WORLD);

	RUN_TEST(test_gather_vectors_int);
	RUN_TEST(test_mesh_writer_serial);
	RUN_TEST(test_mesh_writer_parallel);
	RUN_TEST(test_mesh_reader);
	RUN_TEST(test_data_writer);
	RUN_TEST(test_data_reader);
	
	/*
	json j;
	j["meta_info"] = meta_info;
	j["data_type_sizes"] = data_types;
	j["subdomains"] = subdomains;

	std::cout << std::setw(4) << j << std::endl;

	// Test conversions
	MeshMetaInfo<int16_t, int16_t, double> meta_info_check = j["meta_info"];
	assert(meta_info == meta_info_check);
	*/

	auto rank = MPIUtils::get_local_rank();
	MPIUtils::finalize();
	if (rank == 0) system("pause");
}