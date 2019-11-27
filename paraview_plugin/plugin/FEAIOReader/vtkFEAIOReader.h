#pragma once

#include "vtkMultiBlockDataSetAlgorithm.h"
#include "vtkSmartPointer.h"
#include "vtkIntArray.h"
#include "vtkStringArray.h"

#include "FEAIOReadersModule.h" // for export macros

#include "PVReaderStructs.h"

#include <unordered_set>
#include <set>
#include <map>
#include <vector>

class vtkCallbackCommand;
class vtkDataArraySelection;
class vtkUnstructuredGrid;
class vtkPoints;
class vtkIntArray;
class vtkFloatArray;
class vtkDoubleArray;
class vtkThreshold;

// TODO Move this

struct VolumeSelections
{
	vtkDataArraySelection* PointDataArraySelection;
	vtkDataArraySelection* CellDataArraySelection;
};

struct MeshObject
{
	/// For each node in the partition (including ghosts), this contains a bool
	/// indicating if the respective node is active or not.
	std::vector<bool> node_active_maps;
	/// For each element in the partition (including ghosts), this contains a bool
	/// indicating if the respective element is active or not.
	std::vector<bool> element_active_maps;
	/// The file path to the mesh for this LOD for the current time step.
	/// If this is the same as a new timestep, then the mesh does not have to be recreated.
	std::string current_mesh_path;
};

struct LevelOfDetail
{
	int level;
	std::vector<MeshObject> mesh_objects;
};

// END TODO move

namespace fea_io
{
	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	struct MeshInfo;
	template<typename GlobalIndex, typename LocalIndex, typename CoordType>
	struct Partition;
	template<typename LocalIndex, typename CoordType>
	struct Subcomponent;
	struct ArrayInfo;
	struct MeshObject;
	class MetaReader;
	enum class MeshObjectType;
}

class FEAIOREADERS_EXPORT vtkFEAIOReader : public vtkMultiBlockDataSetAlgorithm
{
public:
    static vtkFEAIOReader* New();
    vtkTypeMacro(vtkFEAIOReader, vtkMultiBlockDataSetAlgorithm);
    void PrintSelf(ostream& os, vtkIndent indent);

    // Description:
    // Specify file name of fea_io meta file to read
    vtkSetStringMacro(FileName);
    vtkGetStringMacro(FileName);

	typedef int32_t IndexType;
	typedef vtkIntArray VTKIndexArrayType;
	typedef double CoordType;

	////////////////////////////////////////////////////////////////
	// Method for XML
	// Provides the list of levels of detail for volume meshes
	vtkStringArray* GetVolumeLODList();
	// Method for XML
	// Sets the active level of detail for volume meshes
	void SetVolumeLOD(const char * VolumeLODName);
	////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////
	// Method for XML
	// Provides the list of levels of detail for surface meshes
	vtkStringArray* GetSurfaceLODList();
	// Method for XML
	// Sets the active level of detail for surface meshes
	void SetSurfaceLOD(const char * VolumeLODName);
	////////////////////////////////////////////////////////////////


	////////////////////////////////////////////////////////////////
	// Methods that help with selecting the partitions to include //
	// Info methods required by ArraySelectionInformationHelper
	const char* GetPartitionArrayName(int PartitionIndex);
	int GetNumberOfPartitionArrays();
	// Returns whether a partition is selected (returns 0/1 for a bool)
	int GetPartitionArrayStatus(const char* PartitionName);
	// Method for XML PartitionStatus property
	void SetPartitionStatus(char* PartitionName, int Status);
	////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////
	// Methods that help with selecting the components to include //
	// Info methods required by ArraySelectionInformationHelper
	const char* GetComponentArrayName(int ComponentIndex);
	int GetNumberOfComponentArrays();
	// Returns whether a partition is selected (returns 0/1 for a bool)
	int GetComponentArrayStatus(const char* ComponentName);
	// Method for XML ComponentStatus property
	void SetComponentStatus(char* ComponentName, int Status);
	////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////
	// Methods that help with selecting the point data to include
	// for the volume meshes.
	// Info methods required by ArraySelectionInformationHelper
	const char* GetVolumePointArrayName(int PointArrayIndex);
	int GetNumberOfVolumePointArrays();
	// Returns whether a point array is selected (returns 0/1 for a bool)
	int GetVolumePointArrayStatus(const char* PointArrayName);
	// Method for XML PointArrayStatus property
	void SetVolumePointArrayStatus(char* PointArrayName, int Status);
	////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////
	// Methods that help with selecting the cell data to include 
	// for the volume meshes.
	// Info methods required by ArraySelectionInformationHelper
	const char* GetVolumeCellArrayName(int CellArrayIndex);
	int GetNumberOfVolumeCellArrays();
	// Returns whether a cell array is selected (returns 0/1 for a bool)
	int GetVolumeCellArrayStatus(const char* CellArrayName);
	// Method for XML CellArrayStatus property
	void SetVolumeCellArrayStatus(char* CellArrayName, int Status);
	///////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////
	// Methods that help with selecting the point data to include 
	// for the surface meshes.
	// Info methods required by ArraySelectionInformationHelper
	const char* GetSurfacePointArrayName(int PointArrayIndex);
	int GetNumberOfSurfacePointArrays();
	// Returns whether a point array is selected (returns 0/1 for a bool)
	int GetSurfacePointArrayStatus(const char* PointArrayName);
	// Method for XML PointArrayStatus property
	void SetSurfacePointArrayStatus(char* PointArrayName, int Status);
	////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////
	// Methods that help with selecting the cell data to include 
	// for the surface meshes.
	// Info methods required by ArraySelectionInformationHelper
	const char* GetSurfaceCellArrayName(int CellArrayIndex);
	int GetNumberOfSurfaceCellArrays();
	// Returns whether a cell array is selected (returns 0/1 for a bool)
	int GetSurfaceCellArrayStatus(const char* CellArrayName);
	// Method for XML CellArrayStatus property
	void SetSurfaceCellArrayStatus(char* CellArrayName, int Status);
	///////////////////////////////////////////////////////////////

protected:
	vtkFEAIOReader();
   ~vtkFEAIOReader();


   int RequestInformation(
      vtkInformation*,
      vtkInformationVector**,
      vtkInformationVector*) VTK_OVERRIDE;

   int RequestData(
      vtkInformation*,
      vtkInformationVector**,
      vtkInformationVector*) VTK_OVERRIDE;

   // Callback registered with the SelectionObserver.
   static void SelectionModifiedCallback(vtkObject* caller,
	   unsigned long eid,
	   void* clientdata,
	   void* calldata);

protected:

	// Note: SetXZStatus gets called when a value is modified and apply is
	// clicked.  How do we change the output?  Check if CurrOutput is non-zero
	// then call an update call that checks what partitions, etc. and removes or
	// adds as neccessary.

    char *FileName;
	int Rank;
	// Indicates if the meta file has been read to get the available data
	bool MetaInfoRead;

	// The output from the last RequestData callback
	vtkInformationVector* CurrentOutputVector;
	// The file path to the mesh for the current time step.
	// If this is the same as a new timestep, then the data does not need to be reloaded.
	std::string CurrentMeshPath;
	// The current time
	double CurrentTime;

	// The time values that are available in the data
	std::vector<double> TimestepValues;
	// The partitions needed based on the user's selections
	std::vector<IndexType> AllPartitionsNeeded;
	// The indices (of AllPartitionsNeeded) for the partitions this processor is responsible for
	std::vector<IndexType> MyPartitionIndices;
	// The components needed based on the user's selections
	std::vector<IndexType> ComponentsNeeded;

	// Selection properties
	vtkStringArray* VolumeLODList;
	vtkStringArray* SurfaceLODList;
	vtkDataArraySelection* PartitionSelection;
	vtkDataArraySelection* ComponentSelection;
	vtkDataArraySelection* VolumePointDataArraySelection;
	vtkDataArraySelection* VolumeCellDataArraySelection;
	vtkDataArraySelection* SurfacePointDataArraySelection;
	vtkDataArraySelection* SurfaceCellDataArraySelection;

	vtkNew<vtkCallbackCommand> SelectionObserver;

	// The multiblock structure containing the mesh with data for the current
	// time step for the volume.  This is actually filtered by VolumeThreshold
	// before it is supplied to the output of this source.
	vtkSmartPointer<vtkMultiBlockDataSet> VolumeMultiBlockStructure;
	vtkSmartPointer<vtkMultiBlockDataSet> OutputVolumeMultiBlockStructure;
	int SelectedVolumeLevel;

	vtkSmartPointer<vtkMultiBlockDataSet> SurfaceMultiBlockStructure;
	vtkSmartPointer<vtkMultiBlockDataSet> OutputSurfaceMultiBlockStructure;
	int SelectedSurfaceLevel;

private:
	vtkFEAIOReader(const vtkFEAIOReader&) = delete;
    void operator=(const vtkFEAIOReader&) = delete;

	void ReadMetaInformation();

	/// Returns the partitions this processor is responsible for.
	/// This takes into account distributed visualization if in parallel
	/// and which partitions were selected by the user when the data was read in.
	void UpdateMyPartitions(vtkSmartPointer<vtkInformation>& outInfo);
	void UpdateComponents();
	/// Updates the output with the requested time, and returns the closest time
	/// in the data to the requested time.
	std::pair<int, double> UpdateTime(
		vtkSmartPointer<vtkInformation>& outInfo,
		vtkSmartPointer<vtkMultiBlockDataSet>& output);

	/// After request data has been called, but a property of the reader (the GUI
	/// check boxes) is changed, then this method is called and will update the
	/// state of the output to reflect the changes to the requested data.
	/// For now, just recreate all the data, but in the future, this could be
	/// optimized to only do the modifications needed.
	//void UpdateOutput();

	/// \param dataReaderPtrs - output parameter that holds the data readers that the
	///    array reader pairs have weak pointers to.  Be sure the dataReaderPtrs outlives
	///    the return type.
	std::vector<ArrayReaderPair> GetDataArrayReaders(
		const fea_io::MetaReader* metaReader,
		const fea_io::MeshObject* meshObject,
		std::vector<std::unique_ptr<fea_io::DataReader>>& dataReaderPtrs);
	
	/// Given a partition, creates a VTK unstructured grid for a specified subcomponent
	void CreateVTKMesh(
		const fea_io::MeshInfo<IndexType, IndexType, CoordType>* meshInfoPtr,
		const fea_io::Partition<IndexType, IndexType, CoordType>* currPartitionPtr,
		const fea_io::Subcomponent<IndexType, CoordType>* subcomponentPtr,
		vtkSmartPointer<vtkUnstructuredGrid>& mesh);

	/// \param cellRelativeLevels - if there are multiple levels of detail, the vector can
	///    be non-empty.  It will contain a level indicator for each cell in the parent
	///    level of detail that will either be this level (meaning the cell should be replaced
	///    with this level) or the parent level (meaning the cell should remain).  Note: the
	///    plugin decides at what level is plotted.
	void CreateBlocksForPartitionByComponent(
		vtkSmartPointer<vtkMultiBlockDataSet>& output,
		const int currLevelOfDetailBlockID,
		const std::vector<short>& cellRelativeLevels,
		const fea_io::MeshInfo<IndexType, IndexType, CoordType>* meshInfoPtr,
		const fea_io::Partition<IndexType, IndexType, CoordType>* currPartitionPtr,
		vtkDataArraySelection* PointDataArraySelection,
		vtkDataArraySelection* CellDataArraySelection,
		const std::vector<ArrayReaderPair>& arrayPairs);

	template<typename ValueType, typename VTKArrayType>
	void CreateDataArrayForSubcomponents_Helper(
		vtkSmartPointer<vtkMultiBlockDataSet>& output,
		const int currLevelOfDetailBlockID,
		const fea_io::Partition<IndexType, IndexType, CoordType>* currPartitionPtr,
		const fea_io::ArrayInfo* arrayInfoPtr,
		const std::vector<ValueType>& data);

	template<typename ValueType, typename VTKArrayType>
	void CreateDataArrayForSubcomponents(
		vtkSmartPointer<vtkMultiBlockDataSet>& output,
		const int currLevelOfDetailBlockID,
		const fea_io::Partition<IndexType, IndexType, CoordType>* currPartition,
		const ArrayReaderPair& arrayReaderPair);

	void SetupAndReadMultiblock(
		const int currTimeIndex,
		const int numLevelsOfDetail,
		const fea_io::MetaReader * metaReaderPtr,
		const fea_io::MeshObjectType * typePtr,
		vtkDataArraySelection* PointDataArraySelection,
		vtkDataArraySelection* CellDataArraySelection,
		vtkSmartPointer<vtkMultiBlockDataSet>& output);

	void UpdateOutput(
		vtkSmartPointer<vtkMultiBlockDataSet>& input,
		vtkSmartPointer<vtkMultiBlockDataSet>& output,
		const int selectedLevel);
};