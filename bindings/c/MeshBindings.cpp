#include "../MeshWriter.h"

using namespace fea_io;

extern "C" {

	typedef int GlobalIndex;
	typedef int LocalIndex;
	typedef double CoordType;

	void* make_mesh_writer();

	// Parallel writer c bindings
	// see https://stackoverflow.com/questions/21407166/using-boost-graph-library-bgl-in-fortran-program

	void* make_mesh_parallel_writer()
	{
	}

	void initialize(
		void* mesh_writer,
		const char* file_path,
		const GlobalIndex total_num_nodes,
		const GlobalIndex total_num_elements,
		const LocalIndex dimensionality,
		const CoordType* extent,
		const LocalIndex* component_ids,
		const char** component_names,
		const char* component_lcs_assignments,
		const char* component_lcs_parameterizations,
		const LocalIndex total_num_partitions)
	{
	}

	void queue_partition_for_write(
		void* mesh_writer,
		const LocalIndex partition_id,
		const LocalIndex num_nodes,
		const LocalIndex num_elements,
		const LocalIndex total_num_connectivity_entries,
		const CoordType* extent,
		const LocalIndex num_components,
		const LocalIndex* component_ids,
		const LocalIndex* num_nodes_in_subcomponents,
		const LocalIndex* num_elements_in_subcomponents)
	{
	}

	void start_parallel_write_phase(void* mesh_writer)
	{
	}

	void write_partition(
		void* mesh_writer,
		const LocalIndex partition_id,
		const GlobalIndex* node_global_ids,
		const LocalIndex* node_parent_partitions,
		const CoordType* node_coords,
		const GlobalIndex* element_global_ids,
		const LocalIndex* element_parent_partitions,
		const ElementType* element_types,
		const LocalIndex** element_connectivities,
		const LocalIndex* component_ids,
		const LocalIndex** nodes_in_subcomponents,
		const LocalIndex** elements_in_subcomponents,
		const CoordType** lcs_parameters_in_subcomponents);

	void close(void* mesh_writer)
	{
	}
}