#include "vtkMultiBlockDataSetAlgorithm.h"
#include "vtkSmartPointer.h"

#include "fea_io/meta/MetaReader.h"
#include "fea_io/mesh/MeshReader.h"
#include "fea_io/data/DataReader.h"
#include "fea_io/group/GroupReader.h"

#include <unordered_set>
#include <set>
#include <map>
#include <vector>

class vtkUnstructuredGrid;
class vtkPoints;

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
	int GetPartitionArrayStatus(char* PartitionName);
	// Method for XML PartitionStatus property
	void SetPartitionStatus(char* PartitionName, int Status);
	////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////
	// Methods that help with selecting the components to include //
	// Info methods required by ArraySelectionInformationHelper
	const char* GetComponentArrayName(int ComponentIndex);
	int GetNumberOfComponentArrays();
	// Returns whether a partition is selected (returns 0/1 for a bool)
	int GetComponentArrayStatus(char* ComponentName);
	// Method for XML ComponentStatus property
	void SetComponentStatus(char* ComponentName, int Status);
	////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////
	// Methods that help with selecting the point data to include //
	// Info methods required by ArraySelectionInformationHelper
	const char* GetPointArrayName(int PointArrayIndex);
	int GetNumberOfPointArrays();
	// Returns whether a point array is selected (returns 0/1 for a bool)
	int GetPointArrayStatus(char* PointArrayName);
	// Method for XML PointArrayStatus property
	void SetPointArrayStatus(char* PointArrayName, int Status);
	////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////
	// Methods that help with selecting the cell data to include //
	// Info methods required by ArraySelectionInformationHelper
	const char* GetCellArrayName(int CellArrayIndex);
	int GetNumberOfCellArrays();
	// Returns whether a cell array is selected (returns 0/1 for a bool)
	int GetCellArrayStatus(char* CellArrayName);
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

protected:
	typedef int32_t IndexType;
	typedef double CoordType;

    char *FileName;
	int Rank;

	// Indicates if the meta file has been read to get the available data
	bool MetaInfoRead;
	// Indicates if the mesh is time dependent
	std::string CurrentMeshPath;
	// The time values that are available in the data
	std::vector<double> TimestepValues;
	// A map relating the time value to the ID
	std::map<double, IndexType> TimeToIDMap;
	// The partitioins needed based on the user's selections
	std::vector<IndexType> AllPartitionsNeeded;
	// The indices (of AllPartitionsNeeded) for the partitions this processor is responsible for
	std::vector<IndexType> MyPartitionIndices;
	std::vector<IndexType> ComponentsNeeded;

	// Selection properties
	// For Partition selection
	std::vector<std::string> PartitionNames;
	std::set<std::string> SelectedPartitions;
	// For Component selection
	std::vector<std::string> ComponentNames;
	std::set<std::string> SelectedComponents;
	// For PointArray selection
	std::vector<std::string> PointArrayNames;
	std::set<std::string> SelectedPointArrays;
	// For CellArray selection
	std::vector<std::string> CellArrayNames;
	std::set<std::string> SelectedCellArrays;

private:
	vtkFEAIOReader(const vtkFEAIOReader&) = delete;
    void operator=(const vtkFEAIOReader&) = delete;

	void ReadMetaInformation();

	/// Returns the partitions this processor is responsible for.
	/// This takes into account distributed visualization if in parallel
	/// and which partitions were selected by the user when the data was read in.
	void UpdateMyPartitions(vtkSmartPointer<vtkInformation>& outInfo);
	void UpdateComponents();
	/// Updates the output with the requested time, and returns the closest time ID
	/// in the data to the requested time.
	unsigned int UpdateTime(vtkSmartPointer<vtkInformation>& outInfo,
		vtkSmartPointer<vtkMultiBlockDataSet>& output);
	
	void CreateVTKMesh(
		const fea_io::Partition<IndexType, IndexType, CoordType> &currPartition,
		vtkSmartPointer<vtkUnstructuredGrid>& mesh);

	void CreateBlocksForPartitionByComponent(vtkSmartPointer<vtkMultiBlockDataSet>& output,
		const PFECNativeInterface::NodeInfo &nodeInfo,
		const PFECNativeInterface::ElementInfo &elementInfo,
		const int partitionIndex,
		const PFECNativeInterface::PFEC_INT partID,
		const std::unique_ptr<PFECNativeInterface::DataReader>& orientReaderPtr,
		const std::unique_ptr<PFECNativeInterface::DataReader>& dataReaderPtr,
		const std::vector<PFECNativeInterface::DataMetaData> dataMeta,
		const std::vector<unsigned int> pointDataInds,
		const std::vector<unsigned int> cellDataInds,
		const std::vector<unsigned int> pointDataWithAvgInds);
};