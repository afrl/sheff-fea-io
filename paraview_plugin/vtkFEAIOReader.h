#include "vtkMultiBlockDataSetAlgorithm.h"
#include "vtkSmartPointer.h"

#include "fea_io/"
#include "pfecMeshReader.h"
#include "pfecDataReader.h"

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
	int GetPartitionArrayStatus(char* PartitionName); // returns 0/1 for a bool
	// Method for XML PartitionStatus property
	void SetPartitionStatus(char* PartitionName, int Status);
	////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////
	// Methods that help with selecting the domains to include //
	// Info methods required by ArraySelectionInformationHelper
	const char* GetDomainArrayName(int DomainIndex);
	int GetNumberOfDomainArrays();
	int GetDomainArrayStatus(char* DomainName); // returns 0/1 for a bool
	void SetDomainStatus(char* DomainName, int Status);
	////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////
	// Methods that help with selecting the point data to include //
	// Info methods required by ArraySelectionInformationHelper
	const char* GetPointArrayName(int PointArrayIndex);
	int GetNumberOfPointArrays();
	int GetPointArrayStatus(char* PointArrayName); // returns 0/1 for a bool
	// Method for XML PointArrayStatus property
	void SetPointArrayStatus(char* PointArrayName, int Status);
	////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////
	// Methods that help with selecting the cell data to include //
	// Info methods required by ArraySelectionInformationHelper
	const char* GetCellArrayName(int CellArrayIndex);
	int GetNumberOfCellArrays();
	int GetCellArrayStatus(char* CellArrayName); // returns 0/1 for a bool
	// Method for XML CellArrayStatus property
	void SetCellArrayStatus(char* CellArrayName, int Status);
	///////////////////////////////////////////////////////////////

protected:
    vtkPFECReader();
   ~vtkPFECReader();


   int RequestInformation(
      vtkInformation*,
      vtkInformationVector**,
      vtkInformationVector*) VTK_OVERRIDE;

   int RequestData(
      vtkInformation*,
      vtkInformationVector**,
      vtkInformationVector*) VTK_OVERRIDE;

protected:
    char *FileName;
	int Rank;

	// Indicates if the meta file has been read to get the available data
	bool MetaInfoRead;
	// Indicates if the mesh is time dependent
	bool TimeDependentMesh;
	// The time values that are avaible in the data
	std::vector<double> TimestepValues;
	// A map relating the time value to the ID
	std::map<double, int> TimeToIDMap;
	// The partitioins needed based on the user's selections
	std::vector<int> AllPartitionsNeeded;
	// The indices (of AllPartitionsNeeded) for the partitions this processor is responsible for
	std::vector<int> MyPartitionIndices;
	std::vector<int> DomainsNeeded;

	// Selection properties
	// For Partition selection
	std::vector<std::string> PartitionNames;
	std::set<std::string> SelectedPartitions;
	// For Domain selection
	std::vector<std::string> DomainNames;
	std::set<std::string> SelectedDomains;
	// For PointArray selection
	std::vector<std::string> PointArrayNames;
	std::set<std::string> SelectedPointArrays;
	// For CellArray selection
	std::vector<std::string> CellArrayNames;
	std::set<std::string> SelectedCellArrays;

private:
    vtkPFECReader(const vtkPFECReader&) {};
    void operator=(const vtkPFECReader&) {};

	void ReadMetaInformation();

	/// Returns the partitions this processor is responsible for.
	/// This takes into account distributed visualization if in parallel
	/// and which partitions were selected by the user when the data was read in.
	void UpdateMyPartitions(vtkSmartPointer<vtkInformation>& outInfo);
	void UpdateDomains();
	/// Updates the output with the requested time, and returns the closest time ID
	/// in the data to the requested time.
	unsigned int UpdateTime(vtkSmartPointer<vtkInformation>& outInfo,
		vtkSmartPointer<vtkMultiBlockDataSet>& output);

	void GetLocalIndicesNeededForDomain(
		const PFECNativeInterface::PFEC_INT &currPartition,
		const PFECNativeInterface::PFEC_INT &currDomain,
		const PFECNativeInterface::NodeInfo &nodeInfo,
		const PFECNativeInterface::ElementInfo &elementInfo,
		std::map<unsigned int, unsigned int> & nodeOldLocalToNewLocalMap,
		std::vector<unsigned int> & localNodeIndicesUsed,
		std::vector<unsigned int> & localElementIndicesUsed);

	void CreateVTKMesh(
		const PFECNativeInterface::PFEC_INT &currPartition,
		const PFECNativeInterface::NodeInfo &nodeInfo,
		const PFECNativeInterface::ElementInfo &elementInfo,
		const std::map<unsigned int, unsigned int> & nodeOldLocalToNewLocalMap,
		const std::vector<unsigned int> & localNodeIndicesUsed,
		const std::vector<unsigned int> & localElementIndicesUsed,
		vtkSmartPointer<vtkUnstructuredGrid>& mesh);

	void CreateBlockForPartitionWithSingleDomain(vtkSmartPointer<vtkMultiBlockDataSet>& output,
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

	void CreateBlocksForPartitionByDomain(vtkSmartPointer<vtkMultiBlockDataSet>& output,
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