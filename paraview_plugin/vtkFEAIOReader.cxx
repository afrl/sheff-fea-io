#include "vtkFEAIOReader.h"

#include "vtkCallbackCommand.h"
#include "vtkDataArraySelection.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkExtractTimeSteps.h"
#include "vtkNew.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkMultiPieceDataSet.h"
#include "vtkDataSet.h"
#include "vtkFieldData.h"
#include "vtkCellData.h"
#include "vtkPointData.h"
#include "vtkUnstructuredGrid.h"
#include "vtkCellType.h"
#include "vtkSmartPointer.h"
// VTK arrays used
#include "vtkShortArray.h"
#include "vtkIntArray.h"
#include "vtkLongLongArray.h"
#include "vtkFloatArray.h"
#include "vtkDoubleArray.h"
#include "vtkUnsignedCharArray.h" // For ghost arrays
//
//#include "vtkLongLongArray.h"

#include "ArrayHelpers.h"
#include "ConnectivityHelpers.h"

#include "fea_io/common/Exceptions.h"
#include "fea_io/mesh/MeshReader.h"
#include "fea_io/data/DataReader.h"
#include "fea_io/group/GroupReader.h"

#include <algorithm>
#include <numeric>
#include <math.h>
#include <set>
#include <unordered_set>

vtkStandardNewMacro(vtkFEAIOReader);

#define OUTPUT_DEBUG_MSG
#ifdef OUTPUT_DEBUG_MSG
#define PRINT_DEBUG_STR(str) vtkOutputWindowDisplayDebugText(("Proc" + std::to_string(this->Rank) + " " + str + "\n").c_str());
#else
#define PRINT_DEBUG_STR(str)
#endif

//#define USE_TRY

void vtkFEAIOReader::PrintSelf(ostream & os, vtkIndent indent)
{
    this->Superclass::PrintSelf(os, indent);
    os << indent << "FileName: " << (this->FileName ? this->FileName : "(none)") << "\n";
}

const char * vtkFEAIOReader::GetPartitionArrayName(int PartitionIndex)
{
	ReadMetaInformation();
	return this->PartitionSelection->GetArrayName(PartitionIndex);
}

int vtkFEAIOReader::GetNumberOfPartitionArrays()
{
	ReadMetaInformation();
	return this->PartitionSelection->GetNumberOfArrays();
}

int vtkFEAIOReader::GetPartitionArrayStatus(const char * PartitionName)
{
	return this->PartitionSelection->ArrayIsEnabled(PartitionName);
}

void vtkFEAIOReader::SetPartitionStatus(char * PartitionName, int Status)
{
	if (Status)
		this->PartitionSelection->EnableArray(PartitionName);
	else
		this->PartitionSelection->DisableArray(PartitionName);
}

const char * vtkFEAIOReader::GetComponentArrayName(int ComponentIndex)
{
	ReadMetaInformation();
	return this->ComponentSelection->GetArrayName(ComponentIndex);
}

int vtkFEAIOReader::GetNumberOfComponentArrays()
{
	ReadMetaInformation();
	return this->ComponentSelection->GetNumberOfArrays();
}

int vtkFEAIOReader::GetComponentArrayStatus(const char * ComponentName)
{
	return this->ComponentSelection->ArrayIsEnabled(ComponentName);
}

void vtkFEAIOReader::SetComponentStatus(char * ComponentName, int Status)
{
	if (Status)
		this->ComponentSelection->EnableArray(ComponentName);
	else
		this->ComponentSelection->DisableArray(ComponentName);
}

const char * vtkFEAIOReader::GetPointArrayName(int PointArrayIndex)
{
	ReadMetaInformation();
	return this->PointDataArraySelection->GetArrayName(PointArrayIndex);
}

int vtkFEAIOReader::GetNumberOfPointArrays()
{
	ReadMetaInformation();
	return this->PointDataArraySelection->GetNumberOfArrays();
}

int vtkFEAIOReader::GetPointArrayStatus(const char * PointArrayName)
{
	return this->PointDataArraySelection->ArrayIsEnabled(PointArrayName);
}

void vtkFEAIOReader::SetPointArrayStatus(char * PointArrayName, int Status)
{
	if (Status)
		this->PointDataArraySelection->EnableArray(PointArrayName);
	else
		this->PointDataArraySelection->DisableArray(PointArrayName);
}

const char * vtkFEAIOReader::GetCellArrayName(int CellArrayIndex)
{
	ReadMetaInformation();
	return this->CellDataArraySelection->GetArrayName(CellArrayIndex);
}

int vtkFEAIOReader::GetNumberOfCellArrays()
{
	ReadMetaInformation();
	return this->CellDataArraySelection->GetNumberOfArrays();
}

int vtkFEAIOReader::GetCellArrayStatus(const char * CellArrayName)
{
	std::string StrName(CellArrayName);
	return this->CellDataArraySelection->ArrayIsEnabled(CellArrayName);
}

void vtkFEAIOReader::SetCellArrayStatus(char * CellArrayName, int Status)
{
	if (Status)
		this->CellDataArraySelection->EnableArray(CellArrayName);
	else
		this->CellDataArraySelection->DisableArray(CellArrayName);
}

vtkFEAIOReader::vtkFEAIOReader()
    : vtkMultiBlockDataSetAlgorithm()
{
	MetaInfoRead = false;
	CurrentOutputVector = nullptr;
    this->FileName = nullptr;
	this->Rank = 0;
    this->SetNumberOfInputPorts(0);

	this->PartitionSelection = vtkDataArraySelection::New();
	this->ComponentSelection = vtkDataArraySelection::New();
	this->PointDataArraySelection = vtkDataArraySelection::New();
	this->CellDataArraySelection = vtkDataArraySelection::New();

	// Setup the selection callback to modify this object when an array
	// selection is changed.
	this->SelectionObserver->SetCallback(&vtkFEAIOReader::SelectionModifiedCallback);
	this->SelectionObserver->SetClientData(this);
	this->PartitionSelection->AddObserver(
		vtkCommand::ModifiedEvent, this->SelectionObserver);
	this->ComponentSelection->AddObserver(
		vtkCommand::ModifiedEvent, this->SelectionObserver);
	this->PointDataArraySelection->AddObserver(
		vtkCommand::ModifiedEvent, this->SelectionObserver);
	this->CellDataArraySelection->AddObserver(
		vtkCommand::ModifiedEvent, this->SelectionObserver);
	this->SetNumberOfInputPorts(0);
	this->SetNumberOfOutputPorts(1);
}

vtkFEAIOReader::~vtkFEAIOReader()
{
    this->SetFileName(0);
}

void vtkFEAIOReader::SelectionModifiedCallback(vtkObject * caller, unsigned long eid, void * clientdata, void * calldata)
{
	static_cast<vtkFEAIOReader*>(clientdata)->Modified();
}

int vtkFEAIOReader::RequestInformation(
	vtkInformation* vtkNotUsed(request),
	vtkInformationVector** vtkNotUsed(inputVector),
	vtkInformationVector* outputVector)
{
	ReadMetaInformation();
	vtkInformation* outInfo = outputVector->GetInformationObject(0);
	outInfo->Set(CAN_HANDLE_PIECE_REQUEST(), 1);
	PRINT_DEBUG_STR("Request Info time step size: " + std::to_string(TimestepValues.size()));
	// Tell ParaView what time steps are availble if there are multiple
	if (TimestepValues.size() > 1) {
		double timeRange[2] = { TimestepValues.front() , TimestepValues.back() };
		outInfo->Set(vtkStreamingDemandDrivenPipeline::TIME_STEPS(), &this->TimestepValues[0], static_cast<int>(TimestepValues.size()));
		PRINT_DEBUG_STR("Set number of time steps to " + std::to_string(TimestepValues.size()));
		outInfo->Set(vtkStreamingDemandDrivenPipeline::TIME_RANGE(), timeRange, 2);
		PRINT_DEBUG_STR("Set time range " + std::to_string(timeRange[0]) + " to " + std::to_string(timeRange[1]));
	}
	else {
		outInfo->Remove(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
		outInfo->Remove(vtkStreamingDemandDrivenPipeline::TIME_RANGE());
	}

	return 1;
}

int vtkFEAIOReader::RequestData(
   vtkInformation* vtkNotUsed(request),
   vtkInformationVector** vtkNotUsed(inputVector),
   vtkInformationVector* outputVector)
{
    if (!this->FileName) {
        vtkErrorMacro("A FileName must be specified.");
        return 1;
    }

	if (TimestepValues.size() == 0) {
		vtkErrorMacro("There was no time steps in the data loaded.");
		return 1;
	}

	CurrentOutputVector = outputVector;

    // Get the info object
    vtkSmartPointer<vtkInformation> outInfo = outputVector->GetInformationObject(0);
	this->UpdateMyPartitions(outInfo);
	this->UpdateComponents();

	// For debug purposes, echo the active properties
	if (this->Rank == 0) PRINT_DEBUG_STR(std::string("Request Data Called."));

    // Get the output
	vtkSmartPointer<vtkMultiBlockDataSet> output =  vtkMultiBlockDataSet::SafeDownCast(outInfo->Get(vtkMultiBlockDataSet::DATA_OBJECT()));
	// Consider checking if the block structure needs to change for subsequent RequestData calls.  Right now, it will
	// just clear the structure and recreate it.
	output->Initialize();

	// Loop over the partitions and read the subcomponents to create the blocks of unstructured grids
	// The tree organization:
	// MultiBlockRoot (each block represents a component or continuous material region)
	// |                          \
    // Block0 (component0)   ...   BlockN (componentN)
	// MulitPiece0 (in Block0)     MulitPieceN (in BlockN)  (each multipiece data set represents the partitions for the domain)
	// |                      \
    // Piece0 (partition0) ...  PieceN (partitionN)
	// UnstructuredGrid         UnstructuredGrid    (this level is the actual mesh for the partition i in component j)
	// Create the structure first
	unsigned int componentBlockID = 0;
	for (auto componentID : this->ComponentsNeeded)
	{
		vtkSmartPointer<vtkMultiBlockDataSet> componentMultiBlock = vtkSmartPointer<vtkMultiBlockDataSet>::New();
		// Create a block for each partition, whether or not it will be used.  An empty block is cheap.
		// Otherwise, other methods will have to keep track of partition block IDs in addition to partition IDs
		componentMultiBlock->SetNumberOfBlocks(static_cast<int>(this->PartitionSelection->GetNumberOfArrays()));
		output->SetBlock(componentBlockID, componentMultiBlock);
		std::string  componentBlockName = "Component " + std::to_string(componentID);
		output->GetMetaData(componentBlockID)->Set(vtkCompositeDataSet::NAME(), componentBlockName.c_str());

		// Create the mesh object for each partition
		for (unsigned int partitionBlockID = 0; partitionBlockID < componentMultiBlock->GetNumberOfBlocks(); partitionBlockID++) {
			vtkSmartPointer<vtkUnstructuredGrid> mesh = vtkUnstructuredGrid::New();
			componentMultiBlock->SetBlock(partitionBlockID, mesh);
			std::string ugridName = "Component" + std::to_string(componentID) + "Part" + std::to_string(partitionBlockID);
			componentMultiBlock->GetMetaData(partitionBlockID)->Set(vtkCompositeDataSet::NAME(), ugridName);
		}

		componentBlockID++;
	}

    // Get the time ID closet to the time requested
	CurrentTime = UpdateTime(outInfo, output);

#ifdef USE_TRY
    try {
#endif

        // Read the meta file and get the mesh for the current timestep
		fea_io::MetaReader metaReader;
		metaReader.open(std::string(FileName));

        // Get the mesh for the current time step
		// TODO use TimeDependentMesh boolean to know if we need to load a new mesh or if we can use the old one.
		auto meshPath = metaReader.get_mesh_path_at_t(CurrentTime);
		fea_io::MeshReader<IndexType, IndexType, CoordType> meshReader(meshPath);
		auto meshInfo = meshReader.get_mesh_info();
		
		// Get the types of arrays in all the data files for this time step and organize the types of data
		auto dataPaths = metaReader.get_data_paths_at_t(CurrentTime);
		std::vector<std::unique_ptr<fea_io::DataReader>> dataReaderPtrs;
		std::vector<ArrayReaderPair> arrayReaders;
		for (auto& path : dataPaths) {
			dataReaderPtrs.push_back(std::make_unique<fea_io::DataReader>(path));
			auto& dataReader = *dataReaderPtrs.back();
			auto arrayInfos = dataReader.get_array_infos();
			for (auto& arrayInfo : arrayInfos) {
				if (this->GetPointArrayStatus(arrayInfo.name.c_str())) {
					// Point data that does not need averaging
					if (arrayInfo.type == fea_io::ArrayType::Node || arrayInfo.type == fea_io::ArrayType::NodeByComponent)
						arrayReaders.push_back({ arrayInfo, ArrayToVTKCategory::Point, &dataReader });
					// Point data that needs averaging, which need to be within domains
					else if (arrayInfo.type == fea_io::ArrayType::NodeByElement)
						arrayReaders.push_back({ arrayInfo, ArrayToVTKCategory::PointAveraged, &dataReader });
				}
				if (this->GetCellArrayStatus(arrayInfo.name.c_str())) {
					// Cell data that does not need averaging
					if (arrayInfo.type == fea_io::ArrayType::Element)
						arrayReaders.push_back({ arrayInfo, ArrayToVTKCategory::Cell, &dataReader });
					// Cell data that needs averaging
					else if (arrayInfo.type == fea_io::ArrayType::QuadPointByElement)
						arrayReaders.push_back({ arrayInfo, ArrayToVTKCategory::CellAveraged, &dataReader });
				}
			}
		}

		// Read what group files exist (Need to finish group implementation)
		//auto groupFilePaths = metaReader.get_group_file_paths(timeID);

		// Though the structure has the components at the top and then partitions, we will
		// loop over partitions then domains for more efficient I/O
		for (auto partitionIndex : this->MyPartitionIndices)
		{
			auto partID = this->AllPartitionsNeeded[partitionIndex];

			// Read the mesh
			auto partition = meshReader.read_partition(partID);

			// This is the main method that creates a mesh with data for each subcomponent
			// in this partition.
			CreateBlocksForPartitionByComponent(output,
				meshInfo,
				partition,
				arrayReaders);
        } // End loop over partitions

#ifdef USE_TRY
    }
    catch (PFECNativeInterface::MsgException e) {
        vtkErrorMacro("Error occured in reading the mesh/data: " + std::string(e.what()));
        return 1;
    }
	catch (std::exception e) {
		vtkErrorMacro("An error occured in the PFECReader plugin: " + std::string(e.what()));
		return 1;
	}
    catch (...) {
        vtkErrorMacro("An unknown exception occured in the PFECReader plugin.");
        return 1;
    }
#endif

    return 1;
}

void vtkFEAIOReader::ReadMetaInformation()
{
	if (this->MetaInfoRead) return;

	if (!this->FileName)
		vtkErrorMacro("A FileName must be specified.");

#ifdef USE_TRY
	try {
#endif
		// Read the meta file
		fea_io::MetaReader metaReader;
		metaReader.open(std::string(FileName));

		// Get the time steps from the meta reader
		TimestepValues = metaReader.get_times();

		// Get the mesh at the first time step to find the partition information
		fea_io::MeshReader<IndexType, IndexType, CoordType> meshReader(metaReader.get_mesh_path_at_t(TimestepValues[0]));
		auto partitionInfo = meshReader.get_partition_info();
		for (auto part : partitionInfo) {
			auto partitionName = std::to_string(part.partition_id);
			this->PartitionSelection->AddArray(partitionName.c_str(), true);
		}
		auto meshInfo = meshReader.get_mesh_info();
		for (auto comp : meshInfo.components) {
			auto compName = std::to_string(comp.id);
			this->ComponentSelection->AddArray(compName.c_str(), true);
		}

		// Add arrays that are not actually in the data, but might be wanted.
		this->PointDataArraySelection->AddArray("GlobalIDs", false);
		this->PointDataArraySelection->AddArray("PartitionIDs", false);
		this->CellDataArraySelection->AddArray("GlobalIDs", false);
		this->CellDataArraySelection->AddArray("PartitionIDs", false);
		this->CellDataArraySelection->AddArray("ComponentIDs", true);
		this->CellDataArraySelection->AddArray("LocalCoordinateSystems", false);
		
		// Get what data exists among all of the time steps
		std::set<std::string> allPointArrays;
		std::set<std::string> allCellArrays;
		for (auto time : TimestepValues) {
			auto dataPaths = metaReader.get_data_paths_at_t(time);
			for (auto& path : dataPaths) {
				fea_io::DataReader dataReader(path);
				auto arraysInFile = dataReader.get_types_of_data_in_file();
				for (auto& arrayPair : arraysInFile) {
					if (arrayPair.second == fea_io::ArrayType::Node
						|| arrayPair.second == fea_io::ArrayType::NodeByComponent
						|| arrayPair.second == fea_io::ArrayType::NodeByElement)
						allPointArrays.insert(arrayPair.first);
					else if (arrayPair.second == fea_io::ArrayType::Element
						|| arrayPair.second == fea_io::ArrayType::QuadPointByElement)
						allCellArrays.insert(arrayPair.first);
				}
			}
		}
		for (auto& pointArrayName : allPointArrays)
			this->PointDataArraySelection->AddArray(pointArrayName.c_str(), true);
		for (auto& cellArrayName : allCellArrays)
			this->CellDataArraySelection->AddArray(cellArrayName.c_str(), true);

#ifdef USE_TRY
	}
	catch (fea_io::MsgException e) {
		vtkErrorMacro(<< e.what());
	}
	catch (...) {
		vtkErrorMacro("Something went wrong when reading the meta information.");
	}
#endif

	this->MetaInfoRead = true;
}

void vtkFEAIOReader::UpdateMyPartitions(vtkSmartPointer<vtkInformation>& outInfo)
{
	// In ParaView, there isn't the concept of partitions.  ParaView has the idea of pieces, which
	// corresponds to processes.  There will likely not be the same number of processors as partitions,
	// so each piece will take several partitions.
	int piece = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER());
	int numberOfPieces = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES());
	this->Rank = piece;

	// Get a vector of the PartIDs for the selected partitions
	this->AllPartitionsNeeded.clear();
	unsigned int partBlockID = 0;
	for (auto i = 0; i < this->PartitionSelection->GetNumberOfArrays(); i++) {
		auto name = this->PartitionSelection->GetArrayName(i);
		if (this->PartitionSelection->ArrayIsEnabled(name))
			this->AllPartitionsNeeded.push_back(atoi(name));
	}

	this->MyPartitionIndices.clear();
	int nominalPartsPerPiece = static_cast<int>(this->AllPartitionsNeeded.size()) / numberOfPieces;
	int remainingNumParts = static_cast<int>(this->AllPartitionsNeeded.size()) % numberOfPieces;
	int myStartPiece = nominalPartsPerPiece * piece + (piece < remainingNumParts ? piece : remainingNumParts);
	int myEndPiece = myStartPiece + nominalPartsPerPiece + (piece < remainingNumParts ? 1 : 0) - 1;
	for (auto i = myStartPiece; i <= myEndPiece; i++) this->MyPartitionIndices.push_back(i);

	// Debug statements
	std::string piecesStr;
	for (auto i = myStartPiece; i <= myEndPiece; i++) piecesStr += " " + std::to_string(i);
	PRINT_DEBUG_STR("Partitions in piece " + std::to_string(piece) + ": " + piecesStr);
}

void vtkFEAIOReader::UpdateComponents()
{
	this->ComponentsNeeded.clear();
	for (auto i = 0; i < this->ComponentSelection->GetNumberOfArrays(); i++) {
		auto name = this->ComponentSelection->GetArrayName(i);
		if (this->ComponentSelection->ArrayIsEnabled(name))
			this->ComponentsNeeded.push_back(atoi(name));
	}
}

double vtkFEAIOReader::UpdateTime(vtkSmartPointer<vtkInformation>& outInfo, vtkSmartPointer<vtkMultiBlockDataSet>& output)
{
	if (outInfo->Has(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP())) {
		// Get the requested time step (only supports a single time step)
		double requestedTime = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP());
		// In theory, we could interpolate between steps if they share the same mesh, but this is not
		// general.  Time steps can have different meshes.  Instead, we will just pick the time step
		// closest to the one requested.
		double closestTime = 0.0;
		double minDifference = -1;
		for (auto i = 0; i < TimestepValues.size(); i++) {
			if (minDifference < 0.0 || abs(TimestepValues[i] - requestedTime) < minDifference) {
				minDifference = abs(TimestepValues[i] - requestedTime);
				closestTime = TimestepValues[i];
			}
		}
		output->GetInformation()->Set(vtkDataObject::DATA_TIME_STEP(), requestedTime);
		PRINT_DEBUG_STR("Requested time: " + std::to_string(requestedTime) + " gave time in data of " + std::to_string(closestTime));
		return closestTime;
	}
	return 0;
}

void vtkFEAIOReader::CreateVTKMesh(
	const fea_io::MeshInfo<IndexType, IndexType, CoordType>& meshInfo,
	const fea_io::Partition<IndexType, IndexType, CoordType>& currPartition,
	const fea_io::Subcomponent<IndexType, CoordType>& subcomponent,
	vtkSmartPointer<vtkUnstructuredGrid>& mesh)
{
	const IndexType numPoints = static_cast<IndexType>(subcomponent.nodes_in_subcomponent.size());
	const IndexType numCells = static_cast<IndexType>(subcomponent.elements_in_subcomponent.size());

	// Create points and preallocate
	vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
	points->Allocate(currPartition.node_global_ids.size());
	mesh->SetPoints(points);
	mesh->Allocate(currPartition.element_global_ids.size());

	// Get conversions to VTK (cell type)
	auto cellTypeMap = fea_io::get_to_vtk_cell_map(meshInfo.connectivity_scheme);

	// Create the points
	for (auto vtkPointNumber = 0; vtkPointNumber < numPoints; vtkPointNumber++) {
		const IndexType indexInPartition = subcomponent.nodes_in_subcomponent[vtkPointNumber];
		const double* coords = &currPartition.node_coords[meshInfo.dimensionality * indexInPartition];
		points->InsertNextPoint(coords[0], (meshInfo.dimensionality > 1 ? coords[1] : 0.0), (meshInfo.dimensionality > 2 ? coords[2] : 0.0));
	}

	// Create point ghost information
	vtkSmartPointer<vtkUnsignedCharArray> pointGhostArray =
		vtkArrayDownCast<vtkUnsignedCharArray>(
			mesh->GetPointData()->GetArray(vtkDataSetAttributes::GhostArrayName()));
	if (!pointGhostArray) {
		pointGhostArray.TakeReference(vtkUnsignedCharArray::New());
		pointGhostArray->SetName(vtkDataSetAttributes::GhostArrayName());
		pointGhostArray->SetNumberOfTuples(numPoints);
		pointGhostArray->FillValue(0);
		mesh->GetPointData()->AddArray(pointGhostArray);
	}
	for (auto vtkPointNumber = 0; vtkPointNumber < numPoints; vtkPointNumber++)
	{
		const IndexType indexInPartition = subcomponent.nodes_in_subcomponent[vtkPointNumber];
		// If the node is not owned by the current partition, then flag it as a duplicate ghost point
		if (currPartition.partition_id != currPartition.node_parent_partitions[indexInPartition])
			pointGhostArray->SetValue(vtkPointNumber, pointGhostArray->GetValue(vtkPointNumber) | vtkDataSetAttributes::DUPLICATEPOINT);
	}

	// Create a map from the partition numbering to the subcomponent numbering
	std::map<IndexType, IndexType> partToSubcompNodeMap;
	for (auto i = 0; i < numPoints; i++)
		partToSubcompNodeMap[subcomponent.nodes_in_subcomponent[i]] = i;

	// Create the cells
	for (auto vtkElementNumber = 0; vtkElementNumber < numCells; vtkElementNumber++)
	{
		const IndexType indexInPartition = subcomponent.elements_in_subcomponent[vtkElementNumber];

		// Get connectivity
		unsigned int idIndex = 0;
		const auto& conn = currPartition.element_connectivities[indexInPartition];
		std::vector<vtkIdType> ids(conn.size());
		// Convert the connectivity to be relative to the subcomponent numbering
		for (const unsigned int partitionNodeNumber : conn) {
			ids[idIndex] = partToSubcompNodeMap[partitionNodeNumber];
			idIndex++;
		}

		// Insert a new VTK cell
		auto const elemType = currPartition.element_types[indexInPartition];
		auto ctIterator = cellTypeMap.find(elemType);
		if (ctIterator == cellTypeMap.end())
			vtkErrorMacro(<< "Element " + std::to_string(indexInPartition) + " (partition index) with type " << static_cast<char>(elemType) << " could not be converted to a VTK type.");
		VTKCellType ct = ctIterator->second;
		std::vector<vtkIdType> idsReordered = fea_io::get_to_vtk_reordering(meshInfo.connectivity_scheme, elemType, ids);
		if (idsReordered.size() == 0) vtkErrorMacro("VTK mapping not found.");
		mesh->InsertNextCell(ct, idsReordered.size(), &(idsReordered[0]));
	}

	// Create point ghost information
	vtkSmartPointer<vtkUnsignedCharArray> cellGhostArray =
		vtkArrayDownCast<vtkUnsignedCharArray>(
			mesh->GetCellData()->GetArray(vtkDataSetAttributes::GhostArrayName()));
	if (!cellGhostArray) {
		cellGhostArray.TakeReference(vtkUnsignedCharArray::New());
		cellGhostArray->SetName(vtkDataSetAttributes::GhostArrayName());
		cellGhostArray->SetNumberOfTuples(numCells);
		cellGhostArray->FillValue(0);
		mesh->GetCellData()->AddArray(cellGhostArray);
	}
	for (auto vtkElementNumber = 0; vtkElementNumber < numCells; vtkElementNumber++)
	{
		const IndexType indexInPartition = subcomponent.elements_in_subcomponent[vtkElementNumber];
		// If the cell is not owned by the current partition, then flag it as a duplicate ghost cell
		if (currPartition.partition_id != currPartition.element_parent_partitions[indexInPartition])
			cellGhostArray->SetValue(vtkElementNumber, cellGhostArray->GetValue(vtkElementNumber) | vtkDataSetAttributes::DUPLICATECELL);
	}
}

void vtkFEAIOReader::CreateBlocksForPartitionByComponent(vtkSmartPointer<vtkMultiBlockDataSet>& output, const fea_io::MeshInfo<IndexType, IndexType, CoordType>& meshInfo, const fea_io::Partition<IndexType, IndexType, CoordType>& currPartition, const std::vector<ArrayReaderPair>& arrayPairs)
{
	// Create the meshes
	unsigned int componentBlockID = 0;
	for (auto componentID : this->ComponentsNeeded)
	{
		// Find the subcomponent of the current component in the current partition
		auto find_iter = std::find_if(currPartition.subcomponents.begin(),
			currPartition.subcomponents.end(),
			[&componentID](const fea_io::Subcomponent<IndexType, CoordType>& subcomp) -> bool { return subcomp.component_id == componentID; });
		// If the component does not have a subcomponent on this partition, then move on to the next component
		if (find_iter == currPartition.subcomponents.end())
			continue;
		auto& subcomponent = *find_iter;

		// Get the VTK UnstructuredGrid container and fill it
		auto componentMultiBlock = vtkMultiBlockDataSet::SafeDownCast(output->GetBlock(componentBlockID));
		vtkSmartPointer<vtkUnstructuredGrid> mesh = vtkUnstructuredGrid::SafeDownCast(componentMultiBlock->GetBlock(currPartition.partition_id));
		mesh->Initialize();
		this->CreateVTKMesh(meshInfo, currPartition, subcomponent, mesh);

		// Create point array for global numbers
		if (this->PointDataArraySelection->ArrayIsEnabled("GlobalIDs"))
			mesh->GetPointData()->AddArray(
				CreatePartitionArrayForComponent<VTKIndexArrayType>("PointGlobalIDs", 1, currPartition.node_global_ids, subcomponent.nodes_in_subcomponent));
		
		// Create point array for owning partition
		if (this->PointDataArraySelection->ArrayIsEnabled("PartitionIDs"))
			mesh->GetPointData()->AddArray(
				CreatePartitionArrayForComponent<VTKIndexArrayType>("PointPartitionIDs", 1, currPartition.node_parent_partitions, subcomponent.nodes_in_subcomponent));

		// Create cell array for global numbers
		if (this->CellDataArraySelection->ArrayIsEnabled("GlobalIDs"))
			mesh->GetCellData()->AddArray(
				CreatePartitionArrayForComponent<VTKIndexArrayType>("CellGlobalIDs", 1, currPartition.element_global_ids, subcomponent.elements_in_subcomponent));

		// Create cell array for owning partition
		if (this->CellDataArraySelection->ArrayIsEnabled("PartitionIDs"))
			mesh->GetCellData()->AddArray(
				CreatePartitionArrayForComponent<VTKIndexArrayType>("CellPartitionIDs", 1, currPartition.element_parent_partitions, subcomponent.elements_in_subcomponent));

		// Create cell array for domain
		if (this->CellDataArraySelection->ArrayIsEnabled("ComponentIDs"))
			mesh->GetCellData()->AddArray(
				CreatePartitionArrayForComponent<VTKIndexArrayType>("CellComponentIDs", subcomponent.component_id, subcomponent.elements_in_subcomponent));

		// Create vectors for the local coordinate systems
		if (this->PointDataArraySelection->ArrayIsEnabled("LocalCoordinateSystems") || this->CellDataArraySelection->ArrayIsEnabled("LocalCoordinateSystems")) {
			// Find the corresponding component in meshInfo for this subcomponent
			auto findIter = std::find_if(meshInfo.components.begin(), meshInfo.components.end(),
				[id = subcomponent.component_id](const auto& c) -> bool {
				return c.id == id;
			});
			if (findIter == meshInfo.components.end())
				throw _MSG_EXCEPT("Could not find the component corresponding to the given subcomponent.");
			const fea_io::ComponentInfo<IndexType>& componentInfo = (*findIter);

			if (componentInfo.lcs_assignment == fea_io::LCSAssignment::Node) {
				auto lcsArrays = CreateLocalCoordinateSystems<IndexType, CoordType, vtkFloatArray>(componentInfo, subcomponent);
				if (lcsArrays[0]->GetNumberOfTuples() != mesh->GetNumberOfPoints())
					throw _MSG_EXCEPT_C("The number lcs parameters for subcomponent " + std::to_string(subcomponent.component_id) + " in partition " + std::to_string(currPartition.partition_id) + " did not match the number of points.");
				mesh->GetPointData()->AddArray(lcsArrays[0]);
				mesh->GetPointData()->AddArray(lcsArrays[1]);
				mesh->GetPointData()->AddArray(lcsArrays[2]);
			}
			else if (componentInfo.lcs_assignment == fea_io::LCSAssignment::Element) {
				auto lcsArrays = CreateLocalCoordinateSystems<IndexType, CoordType, vtkFloatArray>(componentInfo, subcomponent);
				if (lcsArrays[0]->GetNumberOfTuples() != mesh->GetNumberOfCells())
					throw _MSG_EXCEPT_C("The number lcs parameters for subcomponent " + std::to_string(subcomponent.component_id) + " in partition " + std::to_string(currPartition.partition_id) + " did not match the number of cells.");
				mesh->GetCellData()->AddArray(lcsArrays[0]);
				mesh->GetCellData()->AddArray(lcsArrays[1]);
				mesh->GetCellData()->AddArray(lcsArrays[2]);
			}
			else if (componentInfo.lcs_assignment == fea_io::LCSAssignment::Component) {
				auto lcsArrays = CreateLocalCoordinateSystems<IndexType, CoordType, vtkFloatArray>(componentInfo.lcs_parameterization, subcomponent.lcs_parameters_in_subcomponent, mesh->GetNumberOfCells());
				mesh->GetCellData()->AddArray(lcsArrays[0]);
				mesh->GetCellData()->AddArray(lcsArrays[1]);
				mesh->GetCellData()->AddArray(lcsArrays[2]);
			}
			else if (componentInfo.lcs_assignment == fea_io::LCSAssignment::None) {
				auto lcsArrays = CreateLocalCoordinateSystems<IndexType, CoordType, vtkFloatArray>(fea_io::LCSParameterization::Quaternion, {1, 0, 0, 0}, mesh->GetNumberOfCells());
				mesh->GetCellData()->AddArray(lcsArrays[0]);
				mesh->GetCellData()->AddArray(lcsArrays[1]);
				mesh->GetCellData()->AddArray(lcsArrays[2]);
			}
		}

		componentBlockID++;
	} // End loop over domains

	// Create data arrays
	for (auto& arrayPair : arrayPairs) {
		if (arrayPair.arrayInfo.data_type == fea_io::ArrayDataType::Integral) {
			// Handle various integral precisions
			if (arrayPair.arrayInfo.data_type_num_bytes == 2)
				CreateDataArrayForSubcomponents<int16_t, vtkShortArray>(output, currPartition, arrayPair);
			else if (arrayPair.arrayInfo.data_type_num_bytes == 4)
				CreateDataArrayForSubcomponents<int32_t, vtkIntArray>(output, currPartition, arrayPair);
			else if (arrayPair.arrayInfo.data_type_num_bytes == 8)
				CreateDataArrayForSubcomponents<int64_t, vtkLongLongArray> (output, currPartition, arrayPair);
		}
		else if (arrayPair.arrayInfo.data_type == fea_io::ArrayDataType::FloatingPoint) {
			// Handle various floating point precisions
			if (arrayPair.arrayInfo.data_type_num_bytes == 4)
				CreateDataArrayForSubcomponents<float, vtkFloatArray>(output, currPartition, arrayPair);
			else
				CreateDataArrayForSubcomponents<double, vtkDoubleArray>(output, currPartition, arrayPair);
		}
		else
			throw _MSG_EXCEPT_C("Data type not supported.");
	}
}

template<typename ValueType, typename VTKArrayType>
inline void vtkFEAIOReader::CreateDataArrayForSubcomponents_Helper(vtkSmartPointer<vtkMultiBlockDataSet>& output, const fea_io::Partition<IndexType, IndexType, CoordType>& currPartition, const fea_io::ArrayInfo & arrayInfo, const std::vector<ValueType>& data)
{
	unsigned int componentBlockID = 0;
	std::size_t nodeByComponentOffset = 0;
	std::vector<IndexType> componentToDataIndexMap;
	for (auto componentID : this->ComponentsNeeded)
	{
		// Find the subcomponent of the current component in the current partition
		auto find_iter = std::find_if(currPartition.subcomponents.begin(),
			currPartition.subcomponents.end(),
			[&componentID](const fea_io::Subcomponent<IndexType, CoordType>& subcomp) -> bool { return subcomp.component_id == componentID; });
		// If the component does not have a subcomponent on this partition, then move on to the next component
		if (find_iter == currPartition.subcomponents.end())
			continue;
		const fea_io::Subcomponent<IndexType, CoordType>& subcomponent = *find_iter;

		// Get the VTK mesh from the multiblock data structure
		auto componentMultiBlock = vtkMultiBlockDataSet::SafeDownCast(output->GetBlock(componentBlockID));
		auto mesh = vtkUnstructuredGrid::SafeDownCast(componentMultiBlock->GetBlock(currPartition.partition_id));

		if (arrayInfo.type == fea_io::ArrayType::Node) {
			mesh->GetPointData()->AddArray(
				CreatePartitionArrayForComponent<VTKArrayType>(arrayInfo.name, arrayInfo.num_comps, data, subcomponent.nodes_in_subcomponent));
		}
		else if (arrayInfo.type == fea_io::ArrayType::Element || arrayInfo.type == fea_io::ArrayType::QuadPointByElement) {
			// Note: ArrayType::QuadPointByElement data has been averaged to be one per a cell in CreateDataArrayForSubcomponents
			mesh->GetCellData()->AddArray(
				CreatePartitionArrayForComponent<VTKArrayType>(arrayInfo.name, arrayInfo.num_comps, data, subcomponent.elements_in_subcomponent));
		}
		else if (arrayInfo.type == fea_io::ArrayType::NodeByComponent) {
			// Note: the subcomponents were written sequentially to the block of data for this partition, so generate the relevant indices
			componentToDataIndexMap.resize(subcomponent.nodes_in_subcomponent.size());
			std::iota(componentToDataIndexMap.begin(), componentToDataIndexMap.end(), static_cast<int>(nodeByComponentOffset));
			mesh->GetPointData()->AddArray(
				CreatePartitionArrayForComponent<VTKArrayType>(arrayInfo.name, arrayInfo.num_comps, data, componentToDataIndexMap));
			nodeByComponentOffset += subcomponent.nodes_in_subcomponent.size();
		}
		else
			throw _MSG_EXCEPT_C("Array types requiring averaging have not been implemented yet.");

		componentBlockID++;
	}
}

template<typename ValueType, typename VTKArrayType>
void vtkFEAIOReader::CreateDataArrayForSubcomponents(vtkSmartPointer<vtkMultiBlockDataSet>& output, const fea_io::Partition<IndexType, IndexType, CoordType>& currPartition, const ArrayReaderPair & arrayReaderPair)
{
	std::vector<ValueType> data;
	// For Quad point data, we need to average to cells and the read call is different
	if (arrayReaderPair.arrayInfo.type == fea_io::ArrayType::QuadPointByElement) {
		std::vector<int16_t> numQuadsPerElem;
		arrayReaderPair.readerPtr->read_quad(currPartition.partition_id, arrayReaderPair.arrayInfo.name, data, numQuadsPerElem);

		// Average the quad point data to be one per cell
		std::size_t currOffset = 0;
		std::vector<ValueType> averagedData(currPartition.element_global_ids.size() * arrayReaderPair.arrayInfo.num_comps, 0);
		for (auto elemInd = 0; elemInd < numQuadsPerElem.size(); elemInd++) {
			for (auto quadInd = 0; quadInd < numQuadsPerElem[elemInd]; quadInd++)
				for (auto i = 0; i < arrayReaderPair.arrayInfo.num_comps; i++)
					averagedData[arrayReaderPair.arrayInfo.num_comps * elemInd + i] += data[currOffset + arrayReaderPair.arrayInfo.num_comps * quadInd + i] / numQuadsPerElem[elemInd];
			currOffset += arrayReaderPair.arrayInfo.num_comps * numQuadsPerElem[elemInd];
		}

		CreateDataArrayForSubcomponents_Helper<ValueType, VTKArrayType>(output, currPartition, arrayReaderPair.arrayInfo, averagedData);
	}
	else {
		arrayReaderPair.readerPtr->read(currPartition.partition_id, arrayReaderPair.arrayInfo.name, data);
		CreateDataArrayForSubcomponents_Helper<ValueType, VTKArrayType>(output, currPartition, arrayReaderPair.arrayInfo, data);
	}
}

