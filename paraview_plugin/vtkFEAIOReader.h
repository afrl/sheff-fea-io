#include "vtkMultiBlockDataSetAlgorithm.h"
#include "vtkSmartPointer.h"
#include "vtkIntArray.h"

#include "PVReaderStructs.h"

#include "fea_io/meta/MetaReader.h"
#include "fea_io/mesh/MeshInfoStructs.h"
#include "fea_io/mesh/MeshStructs.h"

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

class vtkFEAIOReader : public vtkMultiBlockDataSetAlgorithm
{
public:
    static vtkFEAIOReader* New();
    vtkTypeMacro(vtkFEAIOReader, vtkMultiBlockDataSetAlgorithm);
    void PrintSelf(ostream& os, vtkIndent indent);

    // Description:
    // Specify file name of fea_io meta file to read
    vtkSetStringMacro(FileName);
    vtkGetStringMacro(FileName);

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
	// Methods that help with selecting the point data to include //
	// Info methods required by ArraySelectionInformationHelper
	const char* GetPointArrayName(int PointArrayIndex);
	int GetNumberOfPointArrays();
	// Returns whether a point array is selected (returns 0/1 for a bool)
	int GetPointArrayStatus(const char* PointArrayName);
	// Method for XML PointArrayStatus property
	void SetPointArrayStatus(char* PointArrayName, int Status);
	////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////
	// Methods that help with selecting the cell data to include //
	// Info methods required by ArraySelectionInformationHelper
	const char* GetCellArrayName(int CellArrayIndex);
	int GetNumberOfCellArrays();
	// Returns whether a cell array is selected (returns 0/1 for a bool)
	int GetCellArrayStatus(const char* CellArrayName);
	// Method for XML CellArrayStatus property
	void SetCellArrayStatus(char* CellArrayName, int Status);
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
	typedef int32_t IndexType;
	typedef vtkIntArray VTKIndexArrayType;
	typedef double CoordType;


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
	// The partitioins needed based on the user's selections
	std::vector<IndexType> AllPartitionsNeeded;
	// The indices (of AllPartitionsNeeded) for the partitions this processor is responsible for
	std::vector<IndexType> MyPartitionIndices;
	std::vector<IndexType> ComponentsNeeded;

	// Selection properties
	vtkDataArraySelection* PartitionSelection;
	vtkDataArraySelection* ComponentSelection;
	vtkDataArraySelection* PointDataArraySelection;
	vtkDataArraySelection* CellDataArraySelection;

	vtkNew<vtkCallbackCommand> SelectionObserver;

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
	double UpdateTime(vtkSmartPointer<vtkInformation>& outInfo,
		vtkSmartPointer<vtkMultiBlockDataSet>& output);

	/// After request data has been called, but a property of the reader (the GUI
	/// check boxes) is changed, then this method is called and will update the
	/// state of the output to reflect the changes to the requested data.
	/// For now, just recreate all the data, but in the future, this could be
	/// optimized to only do the modifications needed.
	//void UpdateOutput();
	
	/// Given a partition, creates a VTK unstructured grid for a specified subcomponent
	void CreateVTKMesh(
		const fea_io::MeshInfo<IndexType, IndexType, CoordType>& meshInfo,
		const fea_io::Partition<IndexType, IndexType, CoordType> &currPartition,
		const fea_io::Subcomponent<IndexType, CoordType>& subcomponent,
		vtkSmartPointer<vtkUnstructuredGrid>& mesh);

	void CreateBlocksForPartitionByComponent(
		vtkSmartPointer<vtkMultiBlockDataSet>& output,
		const fea_io::MeshInfo<IndexType, IndexType, CoordType>& meshInfo,
		const fea_io::Partition<IndexType, IndexType, CoordType> &currPartition,
		const std::vector<ArrayReaderPair>& arrayPairs);

	template<typename ValueType, typename VTKArrayType>
	void CreateDataArrayForSubcomponents_Helper(
		vtkSmartPointer<vtkMultiBlockDataSet>& output,
		const fea_io::Partition<IndexType, IndexType, CoordType> &currPartition,
		const fea_io::ArrayInfo& arrayInfo,
		const std::vector<ValueType>& data);

	template<typename ValueType, typename VTKArrayType>
	void CreateDataArrayForSubcomponents(
		vtkSmartPointer<vtkMultiBlockDataSet>& output,
		const fea_io::Partition<IndexType, IndexType, CoordType> &currPartition,
		const ArrayReaderPair& arrayReaderPair);
};