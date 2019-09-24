#pragma once

#ifdef HAVE_MPI
#    include <mpi.h>
#endif

#include <atomic>
#include <iostream>
#include <string>
#include <vector>

namespace fea_io
{
    /// This describes all the variables needed for the MPIUtils.
    /// One instance of this will be created for MPIUtils as a static variable.
    struct MPIUtilsState
    {
        // Indicates if the class is intialized yet
        std::atomic<bool> initialized;
        // Local communicator for all processes that initialize this object
        // with the same "color"
        int local_comm;

        // Process rank in MPI_COMM_WORLD
        int global_rank;

        // Process rank in local_comm
        int local_rank;

        // Number of groups determined by MPI
        int num_groups;

        // This groups key to identify it
        int local_group_key;

        // A vector holding the global ranks for root each group (local_rank 0)
        std::vector<int> root_ranks;

        MPIUtilsState() { reset(); }

        void reset() {
            initialized = false;
#ifdef HAVE_MPI
            local_comm = MPI_COMM_NULL;
#endif
            global_rank = 0;
            local_rank = 0;
            num_groups = 0;
            local_group_key = 0;
            root_ranks.clear();
        }
    };

    class MPIUtils
    {
    public:
        ///
        /// \attention MPIUtils::init() must be called before calling any of these routines!
        ///

        /// FIXME: It is unclear what "is_distributed()" means.  Currently it returns true
        /// if the program was linked with MPI.  It is possible that this should be changed
        /// to return true if the number of ranks is greater than 1.  However, any MPI code
        /// should actually work regardless of ranks == 1 or ranks > 1.  And by not having
        /// separate code for this condition, that is less code to debug/test.
        /// Note, when running the program (either directly with bsamxx, or using mpirun), if
        /// the MPI libraries were linked in, then MPI will be run.  Note, if run on a single
        /// rank, MPI itself will employ shortcuts.
        static bool is_distributed();

        /// Initializes MPI and creates a MPI communicator for the processes that
        /// call this method.  The communicators will be split up by group key.
        /// Every process with the same key will be placed in the same comm.
        ///
        /// Note 1: This function should be called before any other MPIUtil functions are called.
        /// Note 2: if MPI is not configured, then this function will do nothing.
        static void init(int* argc, char*** argv, const int group_key = 0, const bool quiet = true);

        /// Creates a barrier such that each process (in the respective communicator)
        /// will wait until every process gets to.
        static void barrier(const bool use_local_comm = true);

        /// Each process pass the same type of object on this call, and whatever is on the root
        /// process will get distributed out to all of the others so the object passed to this
        /// method will have the same value (even if it is a struct) across all processors.
        /// Specialization for non-fundamental types (i.e. sizeof(T) would not yield the number of bytes needed)
        /// For this specialization, serialization is required, so the object must be convertable to JSON
        template<typename T, typename std::enable_if<!std::is_fundamental<T>::value, T>::type* = nullptr>
        static void update_from_root(T& obj_on_this_proc, const bool use_local_comm = true);

        /// This method will gather a vector from each process to the root process
        /// of the communicator specified.  If use_local_comm is used, then the data
        /// among each group of ranks with the same group_key when init was called
        /// will be sent to the root rank of the respective group.
        /// Specialization for fundamental types (i.e. sizeof(T) would yield the number of bytes needed)
        template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type* = nullptr>
        static std::vector<T> gather_vectors(const std::vector<T>& data_from_this_proc, const bool use_local_comm = true);

        /// This method will gather a vector from each process to the root process
        /// of the communicator specified.  If use_local_comm is used, then the data
        /// among each group of ranks with the same group_key when init was called
        /// will be sent to the root rank of the respective group.
        /// Specialization for non-fundamental types (i.e. sizeof(T) would not yield the number of bytes needed)
        /// For this specialization, serialization is required, so the object must be convertable to JSON
        template<typename T, typename std::enable_if<!std::is_fundamental<T>::value, T>::type* = nullptr>
        static std::vector<T> gather_vectors(const std::vector<T>& data_from_this_proc, const bool use_local_comm = true);

        /// This method will gather a vector from each process and delivers it to every process
        /// of the communicator specified.  If use_local_comm is used, then the data
        /// among each group of ranks with the same group_key when init was called
        /// will be sent to the root rank of the respective group.
        /// Specialization for fundamental types (i.e. sizeof(T) would yield the number of bytes needed)
        template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type* = nullptr>
        static std::vector<T> all_gather_vectors(const std::vector<T>& data_from_this_proc, const bool use_local_comm = true);

        /// This method will scatter a vector from each process and delievers it to every process
        /// of the communicator specified.  If use_local_comm is used, then the data
        /// among each group of ranks with the same group_key when init was called
        /// will be sent to the root rank of the respective group.
        /// Specialization for non-fundamental types (i.e. sizeof(T) would not yield the number of bytes needed)
        /// For this specialization, serialization is required, so the object must be convertable to JSON
        template<typename T, typename std::enable_if<!std::is_fundamental<T>::value, T>::type* = nullptr>
        static std::vector<T> all_gather_vectors(const std::vector<T>& data_from_this_proc, const bool use_local_comm = true);

        template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type* = nullptr>
        static T sum_across_ranks(const T & value, const bool use_local_comm = true);

        /// Sends data from root of one group to the root of another group.  This allows
        /// two programs to communicate with each other using this interface.
        /// Note: does a blocking send
        /// Note: only applicable to fundamental types.
        template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type* = nullptr>
        static void send_to_group(const std::vector<T> data, int target_group_key);

        /// The receive call that complements send_to_group
        template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type* = nullptr>
        static void receive_from_group(std::vector<T>& data, int target_group_key);

        /// An accessor method to get global rank
        static const int get_global_rank() { return get_state().global_rank; }

        /// An accessor method to get local rank
        static const int get_local_rank() { return get_state().local_rank; }

        /// Returns the local communicator for the processors that call init with the same group_key
        static const int get_local_comm() { return get_state().local_comm; }

        /// Returns the number of ranks in the local communicator
        static const int get_num_local_ranks();

        static void finalize();

        // Prohibit construction of this class
        MPIUtils() = delete;
        MPIUtils(MPIUtils const&) = delete;
        void operator=(MPIUtils const&) = delete;

    private:

        static MPIUtilsState& get_state() {
            static MPIUtilsState state;
            return state;
        }
    };

#ifndef NDEBUG
    // FIXME: originally get_state() below was from fea_io... how to prevent that from happening in the future?...
    //        Note, this occurs a number of times in this file...  Also, why "____::MPIUtils::" at all?
#    define VERIFY_INIT( func_name ) \
         { \
             if( !get_state().initialized ) { \
                 throw _UTIL_MSG_EXCEPT( "MPIUtils::" + std::string( func_name ) + "() called before MPIUtils::init()." ); \
             } \
         }
#else
#    define VERIFY_INIT( func_name )
#endif    

    inline bool MPIUtils::is_distributed()
    {
#ifdef HAVE_MPI
        return true;
#else
        return false;
#endif
    }

    inline void MPIUtils::init(int* argc, char*** argv, const int group_key, const bool quiet)
    {
#ifndef HAVE_MPI
        return;
#else
        MPIUtilsState& state = get_state();

        // If the object has already been initialized, just return
        if (state.initialized == true) {
            // If init() is called more than once, there probably is a logic error in the code that
            // should be addressed (as opposed to ignoring the call)... Thoughts?
            throw _UTIL_MSG_EXCEPT("MPIUtils::init() already called.");
            //return;
        }

        // Initialize MPI, if it isn't already.  (If this code is being used in a library, then perhaps PETSc init might
        // be called previously... thus this check.)
        int is_initialized = false;
        MPI_Initialized(&is_initialized);

        if (!is_initialized) {
            MPI_Init(argc, argv);
        }

        // FIXME: what are MPI groups and are they handled correctly here?  Also, how many total ranks are there?  Is this stored somewhere?

        // Get rank
        MPI_Comm_rank(MPI_COMM_WORLD, &state.global_rank);
        std::cout << "rank is: " << state.global_rank << "\n";

        // Get group handle for all_process_group from Comm_World
        // Each process started by the mpiexec must make the split call
        // Each program will have a "color" as it is called (called the
        // group key in this context), which is an identifying integer.
        // Every process with the same "color" will be put in the same group.
        // As a result, each program can have its own communicator.
        // Ordering will be
        state.local_group_key = group_key;
        MPI_Comm_split(MPI_COMM_WORLD, group_key, state.global_rank, &state.local_comm);

        // Get the resulting group for the communicator
        MPI_Group local_group;
        MPI_Comm_group(state.local_comm, &local_group);

        // Now we can get the rank of the process in its group.
        // This is more useful as each program will have operations
        // for its group based on its rank.
        MPI_Group_rank(local_group, &state.local_rank);

        // Free the group since we have the rank now
        MPI_Group_free(&local_group);

        // Figure out how many groups there are by taking a
        // sum of numbers from every process, with the root
        // (rank 0) of each group submitting 1 and the rest 0
        int group_contrib = 0;
        if (state.local_rank == 0)
        {
            group_contrib = 1;
        }
        MPI_Allreduce(&group_contrib, &state.num_groups, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

        // Output the number of groups
        if (state.global_rank == 0 && quiet == false)
        {
            std::cout << state.num_groups << " groups were created by MPI_Comm_split.\n";
        }

        // Now lets get the ranks of the root processes of the other groups
        for (int i = 0; i < state.num_groups; i++) {
            // A summation is done to to find the root global rank of each each group.
            // If i is not the current group key or the local rank is not 0,
            // then just contribute 0 to the sum.  Otherwise contribute the
            // global_rank of the process.
            int rank_term = 0;
            if (i == group_key && state.local_rank == 0)
            {
                rank_term = state.global_rank;
            }

            // Do the reduce, and save result
            int result = 0;
            MPI_Allreduce(&rank_term, &result, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
            state.root_ranks.push_back(result);
        }

        // Output the ranks of the root of each group
        if (state.global_rank == 0 && quiet == false)
        {
            std::cout << "Global ranks of the root of each group:\n";
            for (unsigned int i = 0; i < state.root_ranks.size(); i++)
            {
                std::cout << "[" << i << "] " << state.root_ranks[i] << std::endl;
            }
        }

        // Set the flag in the base class that indicates if the class is initialized
        state.initialized = true;
#endif
    }

    inline void MPIUtils::barrier(const bool use_local_comm)
    {
#ifdef HAVE_MPI
        VERIFY_INIT("barrier");
        // Select which comm to use for all MPI operations in this call
        MPI_Comm comm = use_local_comm ? get_state().local_comm : MPI_COMM_WORLD;
        MPI_Barrier(comm);
#endif
    }

    template<typename T, typename std::enable_if<!std::is_fundamental<T>::value, T>::type*>
    inline void MPIUtils::update_from_root(T & obj_on_this_proc, const bool use_local_comm)
    {
#ifdef USE_MPI
        const MPIUtilsState& state = get_state();
        if (!state.initialized)
        {
            throw _MSG_EXCEPT("An mpi call was called in fea_io before fea_io::MPIUtils::init was called.  Be sure to call init() early.");
        }

        // Select which comm to use for all MPI operations in this call
        MPI_Comm comm = use_local_comm ? state.local_comm : MPI_COMM_WORLD;
        // Get information about the MPI communicator
        int comm_size;
        MPI_Comm_size(comm, &comm_size);
        int comm_rank;
        MPI_Comm_rank(comm, &comm_rank);

        std::vector<std::uint8_t> serialized_object;
        int serialized_object_size;
        if (comm_rank == 0)
        {
            // Create a JSON array from the object
            json j = obj_on_this_proc;
            // Serialize the JSON data using CBOR (concise bindary object
            // representation), which will be communicated over MPI
            serialized_object = json::to_cbor(j);
            serialized_object_size = static_cast<int>(serialized_object.size());
            // First broadcast the size
            MPI_Bcast(&serialized_object_size, 1, MPI_INT, 0, comm);
            // Now broadcast the data
            MPI_Bcast(&serialized_object[0], serialized_object_size, MPI_CHAR, 0, comm);
        }
        else
        {
            // First get the serialized object size
            MPI_Bcast(&serialized_object_size, 1, MPI_INT, 0, comm);
            serialized_object.resize(serialized_object_size);
            // Now get the serialized data
            MPI_Bcast(&serialized_object[0], serialized_object_size, MPI_CHAR, 0, comm);
            // Finally, deserialize the data
            json j = json::from_cbor(serialized_object);
            obj_on_this_proc = j;
        }
#endif
    }

    template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type*>
    inline std::vector<T> MPIUtils::gather_vectors(const std::vector<T>& data_from_this_proc, const bool use_local_comm)
    {
#ifdef HAVE_MPI
        VERIFY_INIT("gather_vectors");

        // Select which comm to use for all MPI operations in this call
        MPI_Comm comm = use_local_comm ? get_state().local_comm : MPI_COMM_WORLD;
        // Get information about the MPI communicator
        int comm_size;
        MPI_Comm_size(comm, &comm_size);
        int comm_rank;
        MPI_Comm_rank(comm, &comm_rank);

        const int size_of_type = sizeof(T);
        const int num_values_for_this_proc = static_cast<int>(data_from_this_proc.size());
        const int num_bytes_for_this_proc = num_values_for_this_proc * size_of_type;

        if (comm_rank == 0) {
            // Figure out the size of the data on each rank that will be communciated
            std::vector<int> num_values_for_each_proc(comm_size);
            MPI_Gather(&num_values_for_this_proc, 1, MPI_INT,
                &num_values_for_each_proc[0], 1, MPI_INT,
                0, comm);

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
                0, comm);

            return recv_buffer;
        }
        else {
            // Figure out the size of the data on each rank that will be communciated
            MPI_Gather(&num_values_for_this_proc, 1, MPI_INT,
                nullptr, 1, MPI_INT,
                0, comm);

            // Send the data to the root
            MPI_Gatherv(&data_from_this_proc[0], num_bytes_for_this_proc, MPI_CHAR,
                nullptr, nullptr, nullptr, MPI_CHAR,
                0, comm);

            // Just return an empty container for all by the root rank
            return std::vector<T>();
        }
#else
        return data_from_this_proc;
#endif
    }

    template<typename T, typename std::enable_if<!std::is_fundamental<T>::value, T>::type*>
    inline std::vector<T> MPIUtils::gather_vectors(const std::vector<T>& data_from_this_proc, const bool use_local_comm)
    {
#ifdef USE_MPI
        const MPIUtilsState& state = get_state();
        if (!state.initialized)
        {
            throw _MSG_EXCEPT("An mpi call was called in fea_io before fea_io::MPIUtils::init was called.  Be sure to call init() early.");
        }

        // Select which comm to use for all MPI operations in this call
        MPI_Comm comm = use_local_comm ? state.local_comm : MPI_COMM_WORLD;
        // Get information about the MPI communicator
        int comm_size;
        MPI_Comm_size(comm, &comm_size);
        int comm_rank;
        MPI_Comm_rank(comm, &comm_rank);

        std::vector<int> num_bytes_for_each_object;
        num_bytes_for_each_object.reserve(data_from_this_proc.size());
        std::vector<std::uint8_t> serialized_data;
        for (auto& obj : data_from_this_proc)
        {
            // Create a JSON array from the objects
            json j = obj;
            // Serialize the JSON data using CBOR (concise bindary object
            // representation), which will be communicated over MPI
            std::vector<std::uint8_t> serialized_object = json::to_cbor(j);

            // Add the serialzied object to the vector of data that will be sent from this rank
            num_bytes_for_each_object.push_back(static_cast<int>(serialized_object.size()));
            serialized_data.reserve(serialized_data.size() + distance(serialized_object.begin(), serialized_object.end()));
            serialized_data.insert(serialized_data.end(), serialized_object.begin(), serialized_object.end());
        }

        if (comm_rank == 0)
        {
            // Get all of the serialized data
            auto all_serialized_data = MPIUtils::gather_vectors(serialized_data, use_local_comm);
            // Get the number of bytes for each object
            auto all_serialized_sizes = MPIUtils::gather_vectors(num_bytes_for_each_object, use_local_comm);
            // Deserialize the data
            std::vector<T> deserialized_data;
            deserialized_data.reserve(all_serialized_sizes.size());
            int offset = 0;
            for (auto& object_size : all_serialized_sizes)
            {
                std::vector<char> serialized_object(all_serialized_data.begin() + offset, all_serialized_data.begin() + offset + object_size);
                json j = json::from_cbor(serialized_object);
                T obj = j;
                deserialized_data.push_back(obj);
                offset += object_size;
            }
            return deserialized_data;
        }
        else
        {
            // Send the serialized data to the root
            MPIUtils::gather_vectors(serialized_data, use_local_comm);
            MPIUtils::gather_vectors(num_bytes_for_each_object, use_local_comm);
            return std::vector<T>();
        }
#else
        return data_from_this_proc;
#endif
    }

    template <typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type *>
    inline std::vector<T> MPIUtils::all_gather_vectors(const std::vector<T> &data_from_this_proc, const bool use_local_comm)
    {
#ifdef USE_MPI
        const MPIUtilsState &state = get_state();
        if (!state.initialized)
        {
            throw _MSG_EXCEPT("An mpi call was called in fea_io before fea_io::MPIUtils::init was called.  Be sure to call init() early.");
        }

        // Select which comm to use for all MPI operations in this call
        MPI_Comm comm = use_local_comm ? state.local_comm : MPI_COMM_WORLD;
        // Get information about the MPI communicator
        int comm_size;
        MPI_Comm_size(comm, &comm_size);
        int comm_rank;
        MPI_Comm_rank(comm, &comm_rank);

        const int size_of_type = sizeof(T);
        const int num_values_for_this_proc = static_cast<int>(data_from_this_proc.size());
        const int num_bytes_for_this_proc = num_values_for_this_proc * size_of_type;

        // Figure out the size of the data on each rank that will be communciated
        std::vector<int> num_values_for_each_proc(comm_size);
        MPI_Gather(&num_values_for_this_proc, 1, MPI_INT,
                   &num_values_for_each_proc[0], 1, MPI_INT,
                   0, comm);

        // Compute the number of bytes to expect from each rank and
        // the offset in the buffer for each rank.
        std::vector<int> recv_counts(comm_size);
        std::vector<int> recv_offsets(comm_size + 1);
        int offset = 0;
        for (auto i = 0; i < comm_size; i++)
        {
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
        MPI_Allgatherv(&data_from_this_proc[0], num_bytes_for_this_proc, MPI_CHAR,
                       &recv_buffer[0], &recv_counts[0], &recv_offsets[0], MPI_CHAR,
                       comm);

        return recv_buffer;
#else
        return data_from_this_proc;
#endif
    }

    template<typename T, typename std::enable_if<!std::is_fundamental<T>::value, T>::type*>
    inline std::vector<T> MPIUtils::all_gather_vectors(const std::vector<T>& data_from_this_proc, const bool use_local_comm)
    {
#ifdef USE_MPI
        const MPIUtilsState& state = get_state();
        if (!state.initialized)
        {
            throw _MSG_EXCEPT("An mpi call was called in fea_io before fea_io::MPIUtils::init was called.  Be sure to call init() early.");
        }

        // Select which comm to use for all MPI operations in this call
        MPI_Comm comm = use_local_comm ? state.local_comm : MPI_COMM_WORLD;
        // Get information about the MPI communicator
        int comm_size;
        MPI_Comm_size(comm, &comm_size);
        int comm_rank;
        MPI_Comm_rank(comm, &comm_rank);

        std::vector<int> num_bytes_for_each_object;
        num_bytes_for_each_object.reserve(data_from_this_proc.size());
        std::vector<std::uint8_t> serialized_data;
        for (auto& obj : data_from_this_proc)
        {
            // Create a JSON array from the objects
            json j = obj;
            // Serialize the JSON data using CBOR (concise bindary object
            // representation), which will be communicated over MPI
            std::vector<std::uint8_t> serialized_object = json::to_cbor(j);

            // Add the serialzied object to the vector of data that will be sent from this rank
            num_bytes_for_each_object.push_back(static_cast<int>(serialized_object.size()));
            serialized_data.reserve(serialized_data.size() + distance(serialized_object.begin(), serialized_object.end()));
            serialized_data.insert(serialized_data.end(), serialized_object.begin(), serialized_object.end());
        }

        // Get all of the serialized data
        auto all_serialized_data = MPIUtils::all_gather_vectors(serialized_data, use_local_comm);
        // Get the number of bytes for each object
        auto all_serialized_sizes = MPIUtils::all_gather_vectors(num_bytes_for_each_object, use_local_comm);
        // Deserialize the data
        std::vector<T> deserialized_data;
        deserialized_data.reserve(all_serialized_sizes.size());
        int offset = 0;
        for (auto& object_size : all_serialized_sizes)
        {
            std::vector<char> serialized_object(all_serialized_data.begin() + offset, all_serialized_data.begin() + offset + object_size);
            json j = json::from_cbor(serialized_object);
            T obj = j;
            deserialized_data.push_back(obj);
            offset += object_size;
        }
        return deserialized_data;
#else
        return data_from_this_proc;
#endif
    }

    template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type*>
    inline T MPIUtils::sum_across_ranks(const T& value, const bool use_local_comm)
    {
#ifdef HAVE_MPI
        VERIFY_INIT("sum_across_ranks");
        // TODO implement
#else
        return value;
#endif
    }

    template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type*>
    inline void MPIUtils::send_to_group(const std::vector<T> data, int target_group_key)
    {
#ifdef HAVE_MPI
        VERIFY_INIT("send_to_group");
        const MPIUtilsState& state = get_state();
        // Only let the local root send information
        if (state.local_rank != 0)
        {
            return;
        }

        // Send as a char array.  It is up to the other program to receive with the correct
        // templated type.
        MPI_Send(&data[0], data.size() * sizeof(T), MPI_CHAR, state.root_ranks[target_group_key], 0, MPI_COMM_WORLD);
#endif
    }

    template<typename T, typename std::enable_if<std::is_fundamental<T>::value, T>::type*>
    inline void MPIUtils::receive_from_group(std::vector<T>& data, int target_group_key)
    {
#ifdef HAVE_MPI
        VERIFY_INIT("receive_from_group");
        const MPIUtilsState& state = get_state();
        // Only let the local root send information
        if (state.local_rank != 0)
        {
            return;
        }

        // Probe the message to see the size of the data it is about to receive
        MPI_Status status;
        MPI_Probe(state.root_ranks[target_group_key], 0, MPI_COMM_WORLD, &status);

        // Get the count from the status
        int num_bytes = 0;
        MPI_Get_count(&status, MPI_CHAR, &num_bytes);
        int num_values = num_bytes / sizeof(T);
        data.resize(num_values);

        // Do the receive
        MPI_Recv(&data[0], data.size() * sizeof(T), MPI_CHAR, state.root_ranks[target_group_key], 0, MPI_COMM_WORLD, &status);
#endif
    }

    inline const int MPIUtils::get_num_local_ranks()
    {
#ifdef HAVE_MPI
        VERIFY_INIT("get_num_local_ranks");
        int comm_size;
        MPI_Comm_size(get_state().local_comm, &comm_size);
        return comm_size;
#else
        return 1;
#endif
    }

    inline void MPIUtils::finalize()
    {
#ifdef HAVE_MPI
        VERIFY_INIT("finalize");
        get_state().reset();

        MPI_Finalize();
#endif
    }
}

#undef VERIFY_INIT