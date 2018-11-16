#include "vtkFEAIOReader.h"

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
#include "vtkShortArray.h"
#include "vtkIntArray.h"
#include "vtkLongArray.h"
#include "vtkLongLongArray.h"
#include "vtkFloatArray.h"
#include "vtkDoubleArray.h"
#include "vtkUnstructuredGrid.h"
#include "vtkCellType.h"
#include "vtkUnsignedCharArray.h"

#include "vtkSmartPointer.h"

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
	if (PartitionIndex >= this->PartitionNames.size())
		return NULL;
	else
		return PartitionNames[PartitionIndex].c_str();
}

int vtkFEAIOReader::GetNumberOfPartitionArrays()
{
	ReadMetaInformation();
	return static_cast<int>(this->PartitionNames.size());
}

int vtkFEAIOReader::GetPartitionArrayStatus(char * PartitionName)
{
	std::string PartStr(PartitionName);
	if (this->SelectedPartitions.find(PartStr) != this->SelectedPartitions.end())
		return 1;
	else
		return 0;
}

void vtkFEAIOReader::SetPartitionStatus(char * PartitionName, int Status)
{
	std::string PartStr(PartitionName);
	// Remove (not going to import)
	if (Status == 0)
		this->SelectedPartitions.erase(PartStr);
	// Add
	else
		this->SelectedPartitions.insert(PartStr);
}

const char * vtkFEAIOReader::GetComponentArrayName(int ComponentIndex)
{
	ReadMetaInformation();
	if (ComponentIndex >= this->ComponentNames.size())
		return NULL;
	else
		return ComponentNames[ComponentIndex].c_str();
}

int vtkFEAIOReader::GetNumberOfComponentArrays()
{
	ReadMetaInformation();
	return static_cast<int>(this->ComponentNames.size());
}

int vtkFEAIOReader::GetComponentArrayStatus(char * ComponentName)
{
	std::string ComponentStr(ComponentName);
	if (this->SelectedComponents.find(ComponentStr) != this->SelectedComponents.end())
		return 1;
	else
		return 0;
}

void vtkFEAIOReader::SetComponentStatus(char * ComponentName, int Status)
{
	std::string ComponentStr(ComponentName);
	// Remove (not going to import)
	if (Status == 0)
		this->SelectedComponents.erase(ComponentStr);
	// Add
	else
		this->SelectedComponents.insert(ComponentStr);
}

const char * vtkFEAIOReader::GetPointArrayName(int PointArrayIndex)
{
	ReadMetaInformation();
	if (PointArrayIndex >= this->PointArrayNames.size())
		return NULL;
	else
		return PointArrayNames[PointArrayIndex].c_str();
}

int vtkFEAIOReader::GetNumberOfPointArrays()
{
	ReadMetaInformation();
	return static_cast<int>(this->PointArrayNames.size());
}

int vtkFEAIOReader::GetPointArrayStatus(char * PointArrayName)
{
	std::string StrName(PointArrayName);
	if (this->SelectedPointArrays.find(StrName) != this->SelectedPointArrays.end())
		return 1;
	else
		return 0;
}

void vtkFEAIOReader::SetPointArrayStatus(char * PointArrayName, int Status)
{
	std::string StrName(PointArrayName);
	//PRINT_DEBUG_STR(std::string("Setting " + StrName + "(" + std::to_string(PointArrayIndex) + ") to " + std::to_string(Status)));
	// Remove (not going to import)
	if (Status == 0)
		this->SelectedPointArrays.erase(StrName);
	// Add
	else
		this->SelectedPointArrays.insert(StrName);
}

const char * vtkFEAIOReader::GetCellArrayName(int CellArrayIndex)
{
	ReadMetaInformation();
	if (CellArrayIndex >= this->CellArrayNames.size())
		return NULL;
	else
		return CellArrayNames[CellArrayIndex].c_str();
}

int vtkFEAIOReader::GetNumberOfCellArrays()
{
	ReadMetaInformation();
	return static_cast<int>(this->CellArrayNames.size());
}

int vtkFEAIOReader::GetCellArrayStatus(char * CellArrayName)
{
	std::string StrName(CellArrayName);
	if (this->SelectedCellArrays.find(StrName) != this->SelectedCellArrays.end())
		return 1;
	else
		return 0;
}

void vtkFEAIOReader::SetCellArrayStatus(char * CellArrayName, int Status)
{
	std::string StrName(CellArrayName);
	// Remove (not going to import)
	if (Status == 0)
		this->SelectedCellArrays.erase(StrName);
	// Add
	else
		this->SelectedCellArrays.insert(StrName);
}

vtkFEAIOReader::vtkFEAIOReader()
    : vtkMultiBlockDataSetAlgorithm()
{
	MetaInfoRead = false;
    this->FileName = 0;
	this->Rank = 0;
    this->SetNumberOfInputPorts(0);
}

vtkFEAIOReader::~vtkFEAIOReader()
{
    this->SetFileName(0);
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

    // Get the info object
    vtkSmartPointer<vtkInformation> outInfo = outputVector->GetInformationObject(0);
	this->UpdateMyPartitions(outInfo);
	this->UpdateComponents();

	// For debug purposes, echo the active properties
	if (this->Rank == 0) PRINT_DEBUG_STR(std::string("Request Data Called."));
	PRINT_DEBUG_STR(std::string("Active Point Arrays:"));
	for (auto i : this->SelectedPointArrays)
		PRINT_DEBUG_STR(i);

    // Get the output
    vtkSmartPointer<vtkMultiBlockDataSet> output =
        vtkMultiBlockDataSet::SafeDownCast(outInfo->Get(vtkMultiBlockDataSet::DATA_OBJECT()));

	// Loop over the partitions and read the subcomponents to create the blocks of unstructured grids
	// The tree organization:
	// MultiBlockRoot (each block represents a component or continuous material region)
	// |                          \
    // Block0 (component0)   ...   BlockN (componentN)
	// MulitPiece0 (in Block0)     MulitPieceN (in BlockN)  (each multipiece data set represents the partitions for the domain)
	// |                      \
    // Piece0 (partition0) ...  PieceN (partitionN)
	// UnstructuredGrid         UnstructuredGrid    (this level is the actual mesh for the partition i in material j)
	// Create the structure first
	unsigned int domainBlockID = 0;
	for (auto domainID : this->ComponentsNeeded)
	{
		vtkSmartPointer<vtkMultiBlockDataSet> domainMultiBlock = vtkSmartPointer<vtkMultiBlockDataSet>::New();
		domainMultiBlock->SetNumberOfBlocks(static_cast<int>(this->AllPartitionsNeeded.size()));
		output->SetBlock(domainBlockID, domainMultiBlock);
		std::string  domainBlockName = "Component " + std::to_string(domainID);
		output->GetMetaData(domainBlockID)->Set(vtkCompositeDataSet::NAME(), domainBlockName.c_str());
		domainBlockID++;
	}

    // Get the time ID closet to the time requested
	double time = UpdateTime(outInfo, output);

#ifdef USE_TRY
    try {
#endif

        // Read the meta file and get the mesh for the current timestep
		fea_io::MetaReader metaReader;
		metaReader.open(std::string(FileName));

        // Get the mesh for the current time step
		// TODO use TimeDependentMesh boolean to know if we need to load a new mesh or if we can use the old one.
		auto meshPath = metaReader.get_mesh_path_at_t(time);
		fea_io::MeshReader<IndexType, IndexType, CoordType> meshReader(meshPath);
		auto meshInfo = meshReader.get_mesh_info();
		
		// Get the types of arrays in all the data files for this time step and organize the types of data
		auto dataPaths = metaReader.get_data_paths_at_t(time);
		std::vector<std::unique_ptr<fea_io::DataReader>> dataReaderPtrs;
		std::vector<ArrayReaderPair> arrayReaders;
		for (auto& path : dataPaths) {
			dataReaderPtrs.push_back(std::make_unique<fea_io::DataReader>(path));
			auto& dataReader = *dataReaderPtrs.back();
			auto arrayInfos = dataReader.get_array_infos();
			for (auto arrayInfo : arrayInfos) {
				if (this->SelectedPointArrays.find(arrayInfo.name) != this->SelectedPointArrays.end()) {
					// Point data that does not need averaging
					if (arrayInfo.type == fea_io::ArrayType::Node || arrayInfo.type == fea_io::ArrayType::NodeByComponent)
						arrayReaders.push_back({
								arrayInfo,
								ArrayToVTKCategory::Point,
								&dataReader
							});
					// Point data that needs averaging, which need to be within domains
					else if (arrayInfo.type == fea_io::ArrayType::NodeByElement)
						arrayReaders.push_back({
								arrayInfo,
								ArrayToVTKCategory::PointAveraged,
								&dataReader
							});
				}
				if (this->SelectedCellArrays.find(arrayInfo.name) != this->SelectedCellArrays.end()) {
					// Cell data that does not need averaging
					if (arrayInfo.type == fea_io::ArrayType::Element)
						arrayReaders.push_back({
								arrayInfo,
								ArrayToVTKCategory::Cell,
								&dataReader
							});
					// Cell data that needs averaging
					else if (arrayInfo.type == fea_io::ArrayType::QuadPointByElement)
						arrayReaders.push_back({
								arrayInfo,
								ArrayToVTKCategory::CellAveraged,
								&dataReader
							});
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
		for (auto part : partitionInfo)
			this->PartitionNames.push_back(std::to_string(part.partition_id));
		auto meshInfo = meshReader.get_mesh_info();
		for (auto comp : meshInfo.components)
			this->ComponentNames.push_back(std::to_string(comp.id));

		// Add arrays that are not actually in the data, but might be wanted.
		this->PointArrayNames.push_back("GlobalIDs");
		this->PointArrayNames.push_back("PartitionIDs");
		this->CellArrayNames.push_back("GlobalIDs");
		this->CellArrayNames.push_back("PartitionIDs");
		this->CellArrayNames.push_back("ComponentIDs");
		this->CellArrayNames.push_back("LocalX");
		this->CellArrayNames.push_back("LocalY");
		this->CellArrayNames.push_back("LocalZ");

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
			this->PointArrayNames.push_back(pointArrayName);
		for (auto& cellArrayName : allCellArrays)
			this->CellArrayNames.push_back(cellArrayName);

		// By default, mark all partitions and components as selected
		for (auto option : this->PartitionNames) this->SelectedPartitions.insert(option);
		for (auto option : this->ComponentNames) this->SelectedComponents.insert(option);
		// By default, mark all the data in the data file as selected
		for (auto option : allPointArrays) this->SelectedPointArrays.insert(option);
		for (auto option : allCellArrays) this->SelectedCellArrays.insert(option);
		// Mark component ID's since they are often useful
		this->SelectedCellArrays.insert("ComponentIDs");

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
	for (auto partIDStr : this->SelectedPartitions)
		this->AllPartitionsNeeded.push_back(atoi(partIDStr.c_str()));

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
	for (auto domainIDStr : this->SelectedComponents) {
		const int domainID = atoi(domainIDStr.c_str());
		this->ComponentsNeeded.push_back(domainID);
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
	const fea_io::Partition<IndexType, IndexType, CoordType> &currPartition,
	const IndexType subcomponentIndex,
	vtkSmartPointer<vtkUnstructuredGrid>& mesh)
{
	auto& subcomponent = currPartition.subcomponents[subcomponentIndex];
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
	/* TODO update
	// Orientation data
	std::vector< std::vector< std::array<double, 4> > > orientData;
	if (orientReaderPtr != nullptr)
		orientReaderPtr->read_part_orientation_data(partID, 0, orientData);
	PRINT_DEBUG_STR("Orient Data Size for Partition " + std::to_string(partID) + ": " + std::to_string(orientData.size()));

	// Create the meshes
	unsigned int domainBlockID = 0;
	for (auto domainID : this->ComponentsNeeded)
	{
		// Figure out which elements and nodes are needed for this domain
		std::vector<unsigned int> localNodeIndicesUsed;
		std::vector<unsigned int> localElementIndicesUsed;
		std::map<unsigned int, unsigned int> nodeOldLocalToNewLocalMap;
		this->GetLocalIndicesNeededForComponent(partID, domainID, // input
			nodeInfo, elementInfo, // input
			nodeOldLocalToNewLocalMap, // output
			localNodeIndicesUsed, localElementIndicesUsed); // output

															// Create the VTK UnstructuredGrid container and fill it
		vtkSmartPointer<vtkUnstructuredGrid> mesh = vtkSmartPointer<vtkUnstructuredGrid>::New();
		this->CreateVTKMesh(partID, // input
			nodeInfo, elementInfo, // input
			nodeOldLocalToNewLocalMap, // input
			localNodeIndicesUsed, localElementIndicesUsed, // input
			mesh); // output

		// Insert the mesh into the multiblock container
		auto domainMultiBlock = vtkMultiBlockDataSet::SafeDownCast(output->GetBlock(domainBlockID));
		domainMultiBlock->SetBlock(partitionIndex, mesh);
		std::string ugridName = "Component" + std::to_string(domainID) + "Part" + std::to_string(partID);
		domainMultiBlock->GetMetaData(partitionIndex)->Set(vtkCompositeDataSet::NAME(), ugridName);

		// Create point array for global numbers
		if (this->SelectedPointArrays.find("GlobalIDs") != this->SelectedPointArrays.end())
			mesh->GetPointData()->AddArray(
				CreateSingleArray<vtkLongArray>("PointGlobalIDs", nodeInfo.nodeIndices, localNodeIndicesUsed));

		// Create point array for owning partition
		if (this->SelectedPointArrays.find("PartitionIDs") != this->SelectedPointArrays.end())
			mesh->GetPointData()->AddArray(
				CreateSingleArray<vtkShortArray>("PointPartitionIDs", nodeInfo.partitions, localNodeIndicesUsed));

		// Create cell array for global numbers
		if (this->SelectedCellArrays.find("GlobalIDs") != this->SelectedCellArrays.end())
			mesh->GetCellData()->AddArray(
				CreateSingleArray<vtkLongArray>("CellGlobalIDs", elementInfo.globalIndices, localElementIndicesUsed));

		// Create cell array for owning partition
		if (this->SelectedCellArrays.find("PartitionIDs") != this->SelectedCellArrays.end())
			mesh->GetCellData()->AddArray(
				CreateSingleArray<vtkShortArray>("CellPartitionIDs", elementInfo.partitions, localElementIndicesUsed));

		// Create cell array for domain
		if (this->SelectedCellArrays.find("ComponentIDs") != this->SelectedCellArrays.end())
			mesh->GetCellData()->AddArray(
				CreateSingleArray<vtkShortArray>("CellComponentIDs", elementInfo.domains, localElementIndicesUsed));


		// Create cell array for local X vector
		bool needX = this->SelectedCellArrays.find("LocalX") != this->SelectedCellArrays.end() && orientData.size() > 0;
		bool needY = this->SelectedCellArrays.find("LocalY") != this->SelectedCellArrays.end() && orientData.size() > 0;
		bool needZ = this->SelectedCellArrays.find("LocalZ") != this->SelectedCellArrays.end() && orientData.size() > 0;
		if (needX || needY || needZ)
			CreateOrientAveragedArray(orientData,
				nodeOldLocalToNewLocalMap, localNodeIndicesUsed, localElementIndicesUsed,
				elementInfo, needX, needY, needZ,
				mesh);

		domainBlockID++;
	} // End loop over domains

	  // We don't need orientation data any more
	orientData.clear();

	// Nodal data
	std::vector<std::vector<std::vector<double>>> nodalData;
	nodalData.resize(pointDataInds.size());
	for (auto i = 0; i < pointDataInds.size(); i++) {
		dataReaderPtr->read_part_unique_data(partID, dataMeta[pointDataInds[i]].id, nodalData[i]);
	}
	// Cell data
	std::vector< std::vector< std::vector<double> > > cellData;
	cellData.resize(cellDataInds.size());
	for (auto i = 0; i < cellDataInds.size(); i++)
		dataReaderPtr->read_part_unique_data(partID, dataMeta[cellDataInds[i]].id, cellData[i]);
	// Nodal by cell data that needs to be averaged first within domains
	std::vector< std::vector< std::vector< std::vector<double> > > > nodalByCellData;
	nodalByCellData.resize(pointDataWithAvgInds.size());
	for (auto i = 0; i < pointDataWithAvgInds.size(); i++)
		dataReaderPtr->read_part_by_elem_data(partID, dataMeta[pointDataWithAvgInds[i]].id, nodalByCellData[i]);
	domainBlockID = 0;
	for (auto domainID : this->ComponentsNeeded)
	{
		// Figure out which elements and nodes are needed for this domain
		std::vector<unsigned int> localNodeIndicesUsed;
		std::vector<unsigned int> localElementIndicesUsed;
		std::map<unsigned int, unsigned int> nodeOldLocalToNewLocalMap;
		this->GetLocalIndicesNeededForComponent(partID, domainID, // input
			nodeInfo, elementInfo, // input
			nodeOldLocalToNewLocalMap, // output
			localNodeIndicesUsed, localElementIndicesUsed); // output

															// Get the mesh
		auto domainMultiBlock = vtkMultiBlockDataSet::SafeDownCast(output->GetBlock(domainBlockID));
		auto mesh = vtkUnstructuredGrid::SafeDownCast(domainMultiBlock->GetBlock(partitionIndex));

		// Insert the nodal data
		for (auto i = 0; i < pointDataInds.size(); i++) {
			vtkFloatArray* pointArray = vtkFloatArray::New();
			auto dataInd = pointDataInds[i];
			PRINT_DEBUG_STR("Inserting Point Data for Component " + std::to_string(domainID) + ": " + dataMeta[dataInd].name);
			pointArray->SetName(dataMeta[dataInd].name.c_str());
			pointArray->SetNumberOfComponents(dataMeta[dataInd].num_comps);
			pointArray->SetNumberOfTuples(localNodeIndicesUsed.size());
			for (auto j = 0; j < localNodeIndicesUsed.size(); j++)
				pointArray->SetTuple(j, &nodalData[i][localNodeIndicesUsed[j]][0]);
			mesh->GetPointData()->AddArray(pointArray);
		}

		// Insert the cell data
		for (auto i = 0; i < cellDataInds.size(); i++) {
			vtkFloatArray* cellArray = vtkFloatArray::New();
			auto dataInd = cellDataInds[i];
			PRINT_DEBUG_STR("Insert Cell Data for Component " + std::to_string(domainID) + ": " + dataMeta[dataInd].name);
			cellArray->SetName(dataMeta[dataInd].name.c_str());
			cellArray->SetNumberOfComponents(dataMeta[dataInd].num_comps);
			cellArray->SetNumberOfTuples(localElementIndicesUsed.size());
			for (auto j = 0; j < localElementIndicesUsed.size(); j++)
				cellArray->SetTuple(j, &cellData[i][localElementIndicesUsed[j]][0]);
			mesh->GetCellData()->AddArray(cellArray);
		}

		// For each nodal by cell data average for this domain and partition then create
		// a point data array
		std::vector< std::vector<double> > averagedData(localNodeIndicesUsed.size());
		std::vector<unsigned int> numEntriesSummed(localNodeIndicesUsed.size());
		for (auto i = 0; i < pointDataWithAvgInds.size(); i++) {
			auto dataInd = pointDataWithAvgInds[i];
			PRINT_DEBUG_STR("Averaging Data: " + dataMeta[dataInd].name);
			mesh->GetPointData()->AddArray(
				CreateNodalAveragedArray<vtkFloatArray>(dataMeta[dataInd].name.c_str(),
					dataMeta[dataInd].num_comps,
					nodalByCellData[i],
					nodeOldLocalToNewLocalMap,
					localNodeIndicesUsed,
					localElementIndicesUsed,
					elementInfo));
		}

		domainBlockID++;
	} // End loop over domains
	nodalData.clear();
	cellData.clear();
	*/
}

