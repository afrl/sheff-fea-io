#pragma once

#include <nlohmann/json.hpp>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

namespace fea_io
{
	using json = nlohmann::json;

	class MPIUtils
	{
	public:
		// Specialization for primative types (i.e. sizeof(T) would yield the number of bytes needed)
		template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type* = nullptr>
		static std::vector<T> gather_vectors(const std::vector<T>& data_from_this_proc);

		// Specialization for non-primative types (i.e. sizeof(T) would not yield the number of bytes needed)
		// For this specialization, serialization is required, so the object must be convertable to JSON
		template<typename T, typename std::enable_if<not std::is_fundamental<T>::value, T>::type* = nullptr>
		static std::vector<T> gather_vectors(const std::vector<T>& data_from_this_proc);
	};

	template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type* = nullptr>
	inline static std::vector<T> MPIUtils::gather_vectors(const std::vector<T>& data_from_this_proc)
	{
		// Get information about the MPI communicator
		int comm_size;
		MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
		int comm_rank;
		MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);

		const int size_of_type = sizeof(T);
		const int num_values_for_this_proc = static_cast<int>(data_from_this_proc.size());
		const int num_bytes_for_this_proc = num_values_for_this_proc * size_of_type;

		if (comm_rank == 0) {
			// Figure out the size of the data on each rank that will be communciated
			std::vector<int> num_values_for_each_proc(comm_size);
			MPI_Gather(&num_values_for_this_proc, 1, MPI_INT,
				&num_values_for_each_proc[0], 1, MPI_INT,
				0, MPI_COMM_WORLD);

			// Compute the number of bytes to expect from each rank and 
			// the offset in the buffer for each rank.
			std::vector<int> recv_counts(comm_size);
			std::vector<int> recv_offsets(comm_size + 1);
			int offset = 0;
			for (auto i = 0; i < comm_size; i++) {
				recv_counts[i] = num_values_for_each_proc[i] * size_of_type;
				recv_offsets[i] = offset;
				offset += recv_counts[i];
			}
			recv_offsets.back() = offset;

			// Allocate a buffer to put the data in
			int num_total_values_needed = 0;
			for (auto num_values : num_values_for_each_proc)
				num_total_values_needed += num_values;
			std::vector<T> recv_buffer(num_total_values_needed);

			// Note: num_bytes_for_this_proc, recv_counts, and recv_offsets must all be
			// in terms of the bytes (not number of values)
			MPI_Gatherv(&data_from_this_proc[0], num_bytes_for_this_proc, MPI_CHAR,
				&recv_buffer[0], &recv_counts[0], &recv_offsets[0], MPI_CHAR,
				0, MPI_COMM_WORLD);

			return recv_buffer;
		}
		else {
			// Figure out the size of the data on each rank that will be communciated
			MPI_Gather(&num_values_for_this_proc, 1, MPI_INT,
				nullptr, 1, MPI_INT,
				0, MPI_COMM_WORLD);

			// Send the data to the root
			MPI_Gatherv(&data_from_this_proc[0], num_bytes_for_this_proc, MPI_CHAR,
				nullptr, nullptr, nullptr, MPI_CHAR,
				0, MPI_COMM_WORLD);

			// Just return an empty container for all by the root rank
			return std::vector<T>();
		}
	}

	template<typename T, typename std::enable_if<not std::is_fundamental<T>::value, T>::type* = nullptr>
	inline static std::vector<T> MPIUtils::gather_vectors(const std::vector<T>& data_from_this_proc)
	{
		// Get information about the MPI communicator
		int comm_size;
		MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
		int comm_rank;
		MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);

		std::vector<int> num_bytes_for_each_object;
		num_bytes_for_each_object.reserve(data_from_this_proc.size());
		std::vector<std::uint8_t> serialized_data;
		for (auto& obj : data_from_this_proc) {
			// Create a JSON array from the subdomain info objects
			json j = obj;
			// Serialize the JSON data using CBOR (concise bindary object
			// representation), which will be communicated over MPI
			std::vector<std::uint8_t> serialized_object = json::to_cbor(j);

			// Add the serialzied object to the vector of data that will be sent from this rank
			num_bytes_for_each_object.push_back(static_cast<int>(serialized_object.size()));
			serialized_data.reserve(serialized_data.size() + distance(serialized_object.begin(), serialized_object.end()));
			serialized_data.insert(serialized_data.end(), serialized_object.begin(), serialized_object.end());
		}


		if (comm_rank == 0) {
			// Get all of the serialized data
			auto all_serialized_data = MPIUtils::gather_vectors(serialized_data);
			// Get the number of bytes for each object
			auto all_serialized_sizes = MPIUtils::gather_vectors(num_bytes_for_each_object);
			// Deserialize the data
			std::vector<T> deserialized_data;
			deserialized_data.reserve(all_serialized_sizes.size());
			int offset = 0;
			for (auto& object_size : all_serialized_sizes) {
				std::vector<char> serialized_object(all_serialized_data.begin() + offset, all_serialized_data.begin() + offset + object_size);
				json j = json::from_cbor(serialized_object);
				T obj = j;
				deserialized_data.push_back(obj);
				offset += object_size;
			}
			return deserialized_data;
		}
		else {
			// Send the serialized data to the root
			MPIUtils::gather_vectors(serialized_data);
			MPIUtils::gather_vectors(num_bytes_for_each_object);
			return std::vector<T>();
		}
	}
}