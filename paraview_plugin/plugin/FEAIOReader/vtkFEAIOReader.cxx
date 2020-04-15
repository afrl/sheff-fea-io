#include "vtkFEAIOReader.h"

#include "ArrayHelpers.h"
#include "ConnectivityHelpers.h"

//////////////////
// VTK includes //
//////////////////

#include "vtkCallbackCommand.h"
#include "vtkDataArraySelection.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"
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
#include "vtkCompositeDataIterator.h"
#include "vtkCellIterator.h"
#include "vtkSelection.h"
#include "vtkSelectionNode.h"
#include "vtkExtractSelectedIds.h"
// VTK arrays used
#include "vtkShortArray.h"
#include "vtkIntArray.h"
#include "vtkLongLongArray.h"
#include "vtkFloatArray.h"
#include "vtkDoubleArray.h"
#include "vtkUnsignedCharArray.h" // For ghost arrays

/////////////////////
// fea_io Includes //
/////////////////////

#include "fea_io/meta/MetaReader.h"
#include "fea_io/mesh/MeshInfoStructs.h"
#include "fea_io/mesh/MeshStructs.h"
#include "fea_io/common/Exceptions.h"
#include "fea_io/mesh/MeshReader.h"
#include "fea_io/data/DataReader.h"
#include "fea_io/group/GroupReader.h"
#include "fea_io/common/ReaderHelpers.h"

//////////////////
// STD Includes //
//////////////////

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

///////////////////////////////
// TMP Move to its own files 
///////////////////////////////

struct DataArrayNames
{
	std::set<std::string> allPointArrays;
	std::set<std::string> allCellArrays;
};

static void GetDataArrayNamesHelper(
	const fea_io::MetaReader& metaReader,
	const fea_io::MeshObjectType typeToFind,
	const std::vector<fea_io::LevelOfDetail>& levels_of_detail,
	DataArrayNames& dataNames)
{
	for (auto& lod : levels_of_detail)
	{
		for (auto& meshObject : lod.mesh_objects)
		{
			if (meshObject.type == typeToFind)
			{
				auto dataPaths = meshObject.data_paths;
				for (auto& path : dataPaths)
				{
					fea_io::DataReader dataReader(metaReader.get_adjusted_path(path));
					auto arraysInFile = dataReader.get_types_of_data_in_file();
					for (auto& arrayPair : arraysInFile) {
						if (arrayPair.second == fea_io::ArrayType::Node
							|| arrayPair.second == fea_io::ArrayType::NodeByComponent
							|| arrayPair.second == fea_io::ArrayType::NodeByElement)
						{
							dataNames.allPointArrays.insert(arrayPair.first);
						}
						else if (arrayPair.second == fea_io::ArrayType::Element
							|| arrayPair.second == fea_io::ArrayType::QuadPointByElement)
						{
							dataNames.allCellArrays.insert(arrayPair.first);
						}
					}
				}
			}
		}
	}
}

DataArrayNames GetDataArrayNames(
	const fea_io::MetaReader& metaReader,
	const fea_io::MeshObjectType typeToFind)
{
	DataArrayNames dataNames;
	GetDataArrayNamesHelper(
		metaReader,
		typeToFind,
		metaReader.get_analysis_meta().input_info.levels_of_detail,
		dataNames);
	for (auto& ts : metaReader.get_analysis_meta().timesteps)
	{
		GetDataArrayNamesHelper(
			metaReader,
			typeToFind,
			ts.levels_of_detail,
			dataNames);
	}
	return dataNames;
}

/// Sets up a multiblock data structure in the form:
/// rootObject - holds components (each is a vtkMultiBlockDataSet)
///   |
/// levels of detail (each is a vtkMultiBlockDataSet)
///   |
/// partitions (each is a vtkMultiPieceDataSet)
///   |
/// mesh w/ data (each is a vtkUnstructuredGrid)
void SetupMultiblock(
	vtkSmartPointer<vtkMultiBlockDataSet>& rootObject,
	const std::vector<vtkFEAIOReader::IndexType>& componentsNeeded,
	const int numLevelsOfDetail,
	const int numOfPartitions,
	const fea_io::MeshObjectType type)
{
	// Create the structure first
		// Note: Loop over ComponentsNeeded since the user can select which components to load.
		//       and keeping null blocks at such a high level in the structure is expensive.
	unsigned int componentBlockID = 0;
	for (auto componentID : componentsNeeded)
	{
		// Create a multiblock for eahc component
		vtkSmartPointer<vtkMultiBlockDataSet> componentMultiBlock = vtkSmartPointer<vtkMultiBlockDataSet>::New();
		// Create a block for each partition, whether or not it will be used.  An empty block is cheap.
		// Otherwise, other methods will have to keep track of partition block IDs in addition to partition IDs
		componentMultiBlock->SetNumberOfBlocks(numLevelsOfDetail);
		rootObject->SetBlock(componentBlockID, componentMultiBlock);
		std::string  componentBlockName = "Component" + std::to_string(componentID);
		rootObject->GetMetaData(componentBlockID)->Set(vtkCompositeDataSet::NAME(), componentBlockName.c_str());

		// Create multipiece for each level of detail
		for (unsigned int lodBlockID = 0;
			lodBlockID < componentMultiBlock->GetNumberOfBlocks();
			lodBlockID++)
		{
			vtkSmartPointer<vtkMultiPieceDataSet> lodMultiPiece = vtkSmartPointer<vtkMultiPieceDataSet>::New();
			lodMultiPiece->SetNumberOfPieces(numOfPartitions);
			componentMultiBlock->SetBlock(lodBlockID, lodMultiPiece);
			std::string  lodBlockName = componentBlockName + " LOD" + std::to_string(lodBlockID);
			componentMultiBlock->GetMetaData(lodBlockID)->Set(vtkCompositeDataSet::NAME(), lodBlockName.c_str());

			// Create the mesh for each partition
			for (unsigned int partitionBlockID = 0;
				partitionBlockID < lodMultiPiece->GetNumberOfPieces();
				partitionBlockID++)
			{
				vtkSmartPointer<vtkUnstructuredGrid> mesh = vtkUnstructuredGrid::New();
				lodMultiPiece->SetPiece(partitionBlockID, mesh);
				std::string ugridName = lodBlockName + " Part" + std::to_string(partitionBlockID);
				lodMultiPiece->GetMetaData(partitionBlockID)->Set(vtkCompositeDataSet::NAME(), ugridName);
			}
		}

		componentBlockID++;
	}
}

///////////////////////////////
// END TMP 
///////////////////////////////

void vtkFEAIOReader::PrintSelf(ostream & os, vtkIndent indent)
{
    this->Superclass::PrintSelf(os, indent);
    os << indent << "FileName: " << (this->FileName ? this->FileName : "(none)") << "\n";
}

/////////////////////////////////
// Volume LOD Array Methods 

vtkStringArray * vtkFEAIOReader::GetVolumeLODList()
{
	return this->VolumeLODList;
}

void vtkFEAIOReader::SetVolumeLOD(const char * VolumeLODName)
{
	PRINT_DEBUG_STR("Set volume lod: " + std::string(VolumeLODName));
	std::string selectedVolumeLOD(VolumeLODName);
	for (auto i = 0; i < this->VolumeLODList->GetNumberOfValues(); i++)
	{
		if (this->VolumeLODList->GetValue(i) == selectedVolumeLOD)
		{
			if (SelectedVolumeLevel != i)
			{
				this->Modified();
				SelectedVolumeLevel = i;
			}
		}
	}
}

/////////////////////////////////
// Surface LOD Array Methods 

vtkStringArray * vtkFEAIOReader::GetSurfaceLODList()
{
	return this->SurfaceLODList;
}

void vtkFEAIOReader::SetSurfaceLOD(const char * SurfaceLODName)
{
	PRINT_DEBUG_STR("Set surface lod: " + std::string(SurfaceLODName));
	std::string selectedSurfaceLOD(SurfaceLODName);
	for (auto i = 0; i < this->SurfaceLODList->GetNumberOfValues(); i++)
	{
		if (this->SurfaceLODList->GetValue(i) == selectedSurfaceLOD)
		{
			if (SelectedSurfaceLevel != i)
			{
				this->Modified();
				SelectedSurfaceLevel = i;
			}
		}
	}
}


/////////////////////////////////
// Partition Array Methods 

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
	{
		this->PartitionSelection->EnableArray(PartitionName);
	}
	else
	{
		this->PartitionSelection->DisableArray(PartitionName);
	}
}

/////////////////////////////////
// Component Array Methods 

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
	{
		this->ComponentSelection->EnableArray(ComponentName);
	}
	else
	{
		this->ComponentSelection->DisableArray(ComponentName);
	}
}

const char * vtkFEAIOReader::GetVolumePointArrayName(int PointArrayIndex)
{
	ReadMetaInformation();
	return this->VolumePointDataArraySelection->GetArrayName(PointArrayIndex);
}

int vtkFEAIOReader::GetNumberOfVolumePointArrays()
{
	ReadMetaInformation();
	return this->VolumePointDataArraySelection->GetNumberOfArrays();
}

int vtkFEAIOReader::GetVolumePointArrayStatus(const char * PointArrayName)
{
	return this->VolumePointDataArraySelection->ArrayIsEnabled(PointArrayName);
}

void vtkFEAIOReader::SetVolumePointArrayStatus(char * PointArrayName, int Status)
{
	if (Status)
	{
		this->VolumePointDataArraySelection->EnableArray(PointArrayName);
	}
	else
	{
		this->VolumePointDataArraySelection->DisableArray(PointArrayName);
	}
}

const char * vtkFEAIOReader::GetVolumeCellArrayName(int CellArrayIndex)
{
	ReadMetaInformation();
	return this->VolumeCellDataArraySelection->GetArrayName(CellArrayIndex);
}

int vtkFEAIOReader::GetNumberOfVolumeCellArrays()
{
	ReadMetaInformation();
	return this->VolumeCellDataArraySelection->GetNumberOfArrays();
}

int vtkFEAIOReader::GetVolumeCellArrayStatus(const char * CellArrayName)
{
	std::string StrName(CellArrayName);
	return this->VolumeCellDataArraySelection->ArrayIsEnabled(CellArrayName);
}

void vtkFEAIOReader::SetVolumeCellArrayStatus(char * CellArrayName, int Status)
{
	if (Status)
	{
		this->VolumeCellDataArraySelection->EnableArray(CellArrayName);
	}
	else
	{
		this->VolumeCellDataArraySelection->DisableArray(CellArrayName);
	}
}

const char * vtkFEAIOReader::GetSurfacePointArrayName(int PointArrayIndex)
{
	ReadMetaInformation();
	return this->SurfacePointDataArraySelection->GetArrayName(PointArrayIndex);
}

int vtkFEAIOReader::GetNumberOfSurfacePointArrays()
{
	ReadMetaInformation();
	return this->SurfacePointDataArraySelection->GetNumberOfArrays();
}

int vtkFEAIOReader::GetSurfacePointArrayStatus(const char * PointArrayName)
{
	return this->SurfacePointDataArraySelection->ArrayIsEnabled(PointArrayName);
}

void vtkFEAIOReader::SetSurfacePointArrayStatus(char * PointArrayName, int Status)
{
	if (Status)
	{
		this->SurfacePointDataArraySelection->EnableArray(PointArrayName);
	}
	else
	{
		this->SurfacePointDataArraySelection->DisableArray(PointArrayName);
	}
}

const char * vtkFEAIOReader::GetSurfaceCellArrayName(int CellArrayIndex)
{
	ReadMetaInformation();
	return this->SurfaceCellDataArraySelection->GetArrayName(CellArrayIndex);
}

int vtkFEAIOReader::GetNumberOfSurfaceCellArrays()
{
	ReadMetaInformation();
	return this->SurfaceCellDataArraySelection->GetNumberOfArrays();
}

int vtkFEAIOReader::GetSurfaceCellArrayStatus(const char * CellArrayName)
{
	std::string StrName(CellArrayName);
	return this->SurfaceCellDataArraySelection->ArrayIsEnabled(CellArrayName);
}

void vtkFEAIOReader::SetSurfaceCellArrayStatus(char * CellArrayName, int Status)
{
	if (Status)
	{
		this->SurfaceCellDataArraySelection->EnableArray(CellArrayName);
	}
	else
	{
		this->SurfaceCellDataArraySelection->DisableArray(CellArrayName);
	}
}

vtkFEAIOReader::vtkFEAIOReader()
    : vtkMultiBlockDataSetAlgorithm()
{
	MetaInfoRead = false;
	CurrentOutputVector = nullptr;
    this->FileName = nullptr;
	this->Rank = 0;

	this->VolumeLODList = vtkStringArray::New();
	this->SurfaceLODList = vtkStringArray::New();
	this->PartitionSelection = vtkDataArraySelection::New();
	this->ComponentSelection = vtkDataArraySelection::New();
	this->VolumePointDataArraySelection = vtkDataArraySelection::New();
	this->VolumeCellDataArraySelection = vtkDataArraySelection::New();
	this->SurfacePointDataArraySelection = vtkDataArraySelection::New();
	this->SurfaceCellDataArraySelection = vtkDataArraySelection::New();

	// Setup the selection callback to modify this object when an array
	// selection is changed.  This allows the pipeline and render window
	// to update when the changes are applied in the GUI by the user.
	this->SelectionObserver->SetCallback(&vtkFEAIOReader::SelectionModifiedCallback);
	this->SelectionObserver->SetClientData(this);
	this->VolumeLODList->AddObserver(
		vtkCommand::ModifiedEvent, this->SelectionObserver);
	this->SurfaceLODList->AddObserver(
		vtkCommand::ModifiedEvent, this->SelectionObserver);
	this->PartitionSelection->AddObserver(
		vtkCommand::ModifiedEvent, this->SelectionObserver);
	this->ComponentSelection->AddObserver(
		vtkCommand::ModifiedEvent, this->SelectionObserver);
	this->VolumePointDataArraySelection->AddObserver(
		vtkCommand::ModifiedEvent, this->SelectionObserver);
	this->VolumeCellDataArraySelection->AddObserver(
		vtkCommand::ModifiedEvent, this->SelectionObserver);
	this->SurfacePointDataArraySelection->AddObserver(
		vtkCommand::ModifiedEvent, this->SelectionObserver);
	this->SurfaceCellDataArraySelection->AddObserver(
		vtkCommand::ModifiedEvent, this->SelectionObserver);
	this->SetNumberOfInputPorts(0);
	this->SetNumberOfOutputPorts(2);

	VolumeMultiBlockStructure = vtkMultiBlockDataSet::New();
	OutputVolumeMultiBlockStructure = VolumeMultiBlockStructure->NewInstance();
	SelectedVolumeLevel = 0;

	SurfaceMultiBlockStructure = vtkMultiBlockDataSet::New();
	OutputSurfaceMultiBlockStructure = SurfaceMultiBlockStructure->NewInstance();
	SelectedSurfaceLevel = 0;
}

vtkFEAIOReader::~vtkFEAIOReader()
{
    this->SetFileName(0);
}

void vtkFEAIOReader::SelectionModifiedCallback(
	vtkObject * caller,
	unsigned long eid,
	void * clientdata,
	void * calldata)
{
	static_cast<vtkFEAIOReader*>(clientdata)->Modified();
}

int vtkFEAIOReader::RequestInformation(
	vtkInformation* vtkNotUsed(request),
	vtkInformationVector** vtkNotUsed(inputVector),
	vtkInformationVector* outputVector)
{
	ReadMetaInformation();

	for (auto i = 0; i < outputVector->GetNumberOfInformationObjects(); i++)
	{
		outputVector->GetInformationObject(i)->Set(CAN_HANDLE_PIECE_REQUEST(), 1);
	}

	PRINT_DEBUG_STR("Request Info time step size: " + std::to_string(TimestepValues.size()));
	// Tell ParaView what time steps are availble if there are multiple
	if (TimestepValues.size() > 1)
	{
		double timeRange[2] = { TimestepValues.front() , TimestepValues.back() };

		for (auto i = 0; i < outputVector->GetNumberOfInformationObjects(); i++)
		{
			outputVector->GetInformationObject(i)->Set(
				vtkStreamingDemandDrivenPipeline::TIME_STEPS(),
				&this->TimestepValues[0],
				static_cast<int>(TimestepValues.size()));

			outputVector->GetInformationObject(i)->Set(
				vtkStreamingDemandDrivenPipeline::TIME_RANGE(),
				timeRange,
				2);
		}
		PRINT_DEBUG_STR("Set number of time steps to " + std::to_string(TimestepValues.size()));
		PRINT_DEBUG_STR("Set time range " + std::to_string(timeRange[0]) + " to " + std::to_string(timeRange[1]));
	}
	else
	{
		// If there is not more than 1 time step, then indicate that this data
		// not time dependant.
		for (auto i = 0; i < outputVector->GetNumberOfInformationObjects(); i++)
		{
			outputVector->GetInformationObject(i)->Remove(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
			outputVector->GetInformationObject(i)->Remove(vtkStreamingDemandDrivenPipeline::TIME_RANGE());
		}
	}

	return 1;
}

int vtkFEAIOReader::RequestData(
   vtkInformation* vtkNotUsed(request),
   vtkInformationVector** vtkNotUsed(inputVector),
   vtkInformationVector* outputVector)
{
    if (!this->FileName)
	{
        vtkErrorMacro("A FileName must be specified.");
        return 1;
    }

	if (TimestepValues.size() == 0)
	{
		vtkErrorMacro("There was no time steps in the data loaded.");
		return 1;
	}

	CurrentOutputVector = outputVector;

	// For debug purposes, echo the active properties
	if (this->Rank == 0)
	{
		PRINT_DEBUG_STR(std::string("Request Data Called."));
	}

	this->UpdateComponents();

#ifdef USE_TRY
    try {
#endif

        // Read the meta file and get the current timestep
		fea_io::MetaReader metaReader;
		metaReader.open(std::string(FileName));
		auto& analysisMetaInfo = metaReader.get_analysis_meta();
		auto numLevelsOfDetail = analysisMetaInfo.levels_of_detail_names.size();

		// Create volume output
		{
			// Get the info object
			vtkSmartPointer<vtkInformation> outInfo = outputVector->GetInformationObject(0);
			this->UpdateMyPartitions(outInfo);

			// Consider checking if the block structure needs to change for subsequent RequestData calls.
			// Right now, it will just clear the structure and recreate it.
			this->VolumeMultiBlockStructure->Initialize();

			// Get the time ID closet to the time requested
			auto timePair = this->UpdateTime(
				analysisMetaInfo,
				outInfo,
				this->VolumeMultiBlockStructure);
			auto currentTimeIndex = timePair.first;
			CurrentTime = timePair.second;

			auto volumeType = fea_io::MeshObjectType::Volume;
			SetupAndReadMultiblock(
				currentTimeIndex,
				numLevelsOfDetail,
				&metaReader,
				&volumeType,
				this->VolumePointDataArraySelection,
				this->VolumeCellDataArraySelection,
				this->VolumeMultiBlockStructure);

			UpdateOutput(
				this->VolumeMultiBlockStructure,
				this->OutputVolumeMultiBlockStructure,
				this->SelectedVolumeLevel);

			outInfo->Set(vtkMultiBlockDataSet::DATA_OBJECT(), OutputVolumeMultiBlockStructure);
		}

		// Create surface output
		{
			// Get the info object
			vtkSmartPointer<vtkInformation> outInfo = outputVector->GetInformationObject(1);
			this->UpdateMyPartitions(outInfo);

			// Consider checking if the block structure needs to change for subsequent RequestData calls.
			// Right now, it will just clear the structure and recreate it.
			this->SurfaceMultiBlockStructure->Initialize();

			// Get the time ID closet to the time requested
			auto timePair = this->UpdateTime(
				analysisMetaInfo,
				outInfo,
				this->SurfaceMultiBlockStructure);
			auto currentTimeIndex = timePair.first;
			CurrentTime = timePair.second;

			auto surfaceType = fea_io::MeshObjectType::Surface;
			SetupAndReadMultiblock(
				currentTimeIndex,
				numLevelsOfDetail,
				&metaReader,
				&surfaceType,
				this->SurfacePointDataArraySelection,
				this->SurfaceCellDataArraySelection,
				this->SurfaceMultiBlockStructure);

			UpdateOutput(
				this->SurfaceMultiBlockStructure,
				this->OutputSurfaceMultiBlockStructure,
				this->SelectedSurfaceLevel);

			outInfo->Set(vtkMultiBlockDataSet::DATA_OBJECT(), OutputSurfaceMultiBlockStructure);
		}

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
	if (this->MetaInfoRead)
	{
		return;
	}

	if (!this->FileName)
	{
		vtkErrorMacro("A FileName must be specified.");
	}

#ifdef USE_TRY
	try {
#endif
		// Read the meta file
		fea_io::MetaReader metaReader;
		metaReader.open(std::string(FileName));
		auto& analysisMetaInfo = metaReader.get_analysis_meta();

		// Get the time steps from the meta reader
		TimestepValues.clear();
		if (analysisMetaInfo.timesteps.size() > 0)
		{
			for (auto& ts : analysisMetaInfo.timesteps)
			{
				TimestepValues.push_back(ts.time);
			}
		}
		else
		{
			TimestepValues.push_back(0.0);
		}

		//VolumeLevelsOfDetail.resize(analysisMetaInfo.levels_of_detail_names.size());
		auto lodNamesSize = analysisMetaInfo.levels_of_detail_names.size();
		this->VolumeLODList->SetNumberOfValues(lodNamesSize);
		this->SurfaceLODList->SetNumberOfValues(lodNamesSize);
		for (auto i = 0; i < lodNamesSize; i++)
		{
			this->VolumeLODList->SetValue(i,
				analysisMetaInfo.levels_of_detail_names[i]);
			this->SurfaceLODList->SetValue(i,
				analysisMetaInfo.levels_of_detail_names[i]);
		}

		// Get the mesh at the first time step to find the partition information
		fea_io::MeshObject meshObject;
		if (analysisMetaInfo.timesteps.size() == 0)
		{
			meshObject = analysisMetaInfo.input_info.levels_of_detail[0].mesh_objects[0];
		}
		else
		{
			meshObject = analysisMetaInfo.timesteps[0].levels_of_detail[0].mesh_objects[0];
		}
		fea_io::MeshReader<IndexType, IndexType, CoordType> meshReader(
			metaReader.get_adjusted_path(meshObject.mesh_path));
		auto partitionInfo = meshReader.get_partition_info();
		for (auto part : partitionInfo)
		{
			auto partitionName = std::to_string(part.partition_id);
			this->PartitionSelection->AddArray(partitionName.c_str(), true);
		}
		auto meshInfo = meshReader.get_mesh_info();
		for (auto comp : meshInfo.components)
		{
			auto compName = std::to_string(comp.id);
			this->ComponentSelection->AddArray(compName.c_str(), true);
		}

		// Add arrays that are not actually in the data, but might be wanted.
		this->VolumePointDataArraySelection->AddArray("GlobalIDs", false);
		this->VolumePointDataArraySelection->AddArray("PartitionIDs", false);
		this->VolumeCellDataArraySelection->AddArray("GlobalIDs", false);
		this->VolumeCellDataArraySelection->AddArray("PartitionIDs", false);
		this->VolumeCellDataArraySelection->AddArray("ComponentIDs", true);
		this->VolumeCellDataArraySelection->AddArray("LocalCoordinateSystems", false);
		
		// Get what data exists in the files (assumes the same data exists for all
		// mesh objects of the same type)
		auto volumeDataNames = GetDataArrayNames(metaReader, fea_io::MeshObjectType::Volume);
		for (auto& pointArrayName : volumeDataNames.allPointArrays)
		{
			this->VolumePointDataArraySelection->AddArray(pointArrayName.c_str(), true);
		}
		for (auto& cellArrayName : volumeDataNames.allCellArrays)
		{
			this->VolumeCellDataArraySelection->AddArray(cellArrayName.c_str(), true);
		}
		auto surfaceDataNames = GetDataArrayNames(metaReader, fea_io::MeshObjectType::Surface);
		for (auto& pointArrayName : surfaceDataNames.allPointArrays)
		{
			this->SurfacePointDataArraySelection->AddArray(pointArrayName.c_str(), true);
		}
		for (auto& cellArrayName : surfaceDataNames.allCellArrays)
		{
			this->SurfaceCellDataArraySelection->AddArray(cellArrayName.c_str(), true);
		}

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
	for (auto i = 0; i < this->PartitionSelection->GetNumberOfArrays(); i++)
	{
		auto name = this->PartitionSelection->GetArrayName(i);
		if (this->PartitionSelection->ArrayIsEnabled(name))
		{
			this->AllPartitionsNeeded.push_back(atoi(name));
		}
	}

	this->MyPartitionIndices.clear();
	int nominalPartsPerPiece = static_cast<int>(this->AllPartitionsNeeded.size()) / numberOfPieces;
	int remainingNumParts = static_cast<int>(this->AllPartitionsNeeded.size()) % numberOfPieces;
	int myStartPiece = nominalPartsPerPiece * piece + (piece < remainingNumParts ? piece : remainingNumParts);
	int myEndPiece = myStartPiece + nominalPartsPerPiece + (piece < remainingNumParts ? 1 : 0) - 1;
	for (auto i = myStartPiece; i <= myEndPiece; i++) this->MyPartitionIndices.push_back(i);

	// Debug statements
	std::string piecesStr;
	for (auto i = myStartPiece; i <= myEndPiece; i++)
	{
		piecesStr += " " + std::to_string(i);
	}
	PRINT_DEBUG_STR("Partitions in piece " + std::to_string(piece) + ": " + piecesStr);
}

void vtkFEAIOReader::UpdateComponents()
{
	this->ComponentsNeeded.clear();
	for (auto i = 0; i < this->ComponentSelection->GetNumberOfArrays(); i++)
	{
		auto name = this->ComponentSelection->GetArrayName(i);
		if (this->ComponentSelection->ArrayIsEnabled(name))
		{
			this->ComponentsNeeded.push_back(atoi(name));
		}
	}
}

std::pair<int, double> vtkFEAIOReader::UpdateTime(
	const fea_io::AnalysisMetaInfo& analysisMetaInfo,
	vtkSmartPointer<vtkInformation>& outInfo,
	vtkSmartPointer<vtkMultiBlockDataSet>& output)
{
	if (analysisMetaInfo.timesteps.size() == 0)
	{
		return { -1, 0.0 };
	}
	else if (outInfo->Has(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP()))
	{
		// Get the requested time step (only supports a single time step)
		double requestedTime = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP());
		// In theory, we could interpolate between steps if they share the same mesh, but this is not
		// general.  Time steps can have different meshes.  Instead, we will just pick the time step
		// closest to the one requested.
		double closestTime = 0.0;
		int closestTimeIndex = 0;
		double minDifference = -1;
		for (auto i = 0; i < TimestepValues.size(); i++)
		{
			if (minDifference < 0.0 || abs(TimestepValues[i] - requestedTime) < minDifference)
			{
				minDifference = abs(TimestepValues[i] - requestedTime);
				closestTime = TimestepValues[i];
				closestTimeIndex = i;
			}
		}
		output->GetInformation()->Set(vtkDataObject::DATA_TIME_STEP(), requestedTime);
		PRINT_DEBUG_STR("Requested time: " + std::to_string(requestedTime) + " gave time in data of " + std::to_string(closestTime));
		return { closestTimeIndex, closestTime };
	}
	else
	{
		return { 0, 0.0 };
	}
}

std::vector<ArrayReaderPair> vtkFEAIOReader::GetDataArrayReaders(
	const fea_io::MetaReader * metaReaderPtr,
	const fea_io::MeshObject * meshObjectPtr,
	vtkDataArraySelection* PointDataArraySelection,
	vtkDataArraySelection* CellDataArraySelection,
	std::vector<std::unique_ptr<fea_io::DataReader>>& dataReaderPtrs)
{
	// Get the types of arrays in all the data files for this time step and organize the types of data
	auto dataPaths = meshObjectPtr->data_paths;
	std::vector<ArrayReaderPair> arrayReaders;
	for (auto& path : dataPaths)
	{
		dataReaderPtrs.push_back(
			std::make_unique<fea_io::DataReader>(metaReaderPtr->get_adjusted_path(path)));
		auto& dataReader = *dataReaderPtrs.back();
		auto arrayInfos = dataReader.get_array_infos();
		for (auto& arrayInfo : arrayInfos)
		{
			if (PointDataArraySelection->ArrayIsEnabled(arrayInfo.name.c_str()))
			{
				// Point data that does not need averaging
				if (arrayInfo.type == fea_io::ArrayType::Node
					|| arrayInfo.type == fea_io::ArrayType::NodeByComponent)
				{
					arrayReaders.push_back({
						std::make_unique<fea_io::ArrayInfo>(arrayInfo),
						ArrayToVTKCategory::Point,
						&dataReader });
				}
				// Point data that needs averaging, which need to be within domains
				else if (arrayInfo.type == fea_io::ArrayType::NodeByElement)
				{
					arrayReaders.push_back({
						std::make_unique<fea_io::ArrayInfo>(arrayInfo),
						ArrayToVTKCategory::PointAveraged,
						&dataReader });
				}
			}
			if (CellDataArraySelection->ArrayIsEnabled(arrayInfo.name.c_str()))
			{
				// Cell data that does not need averaging
				if (arrayInfo.type == fea_io::ArrayType::Element)
				{
					arrayReaders.push_back({
						std::make_unique<fea_io::ArrayInfo>(arrayInfo),
						ArrayToVTKCategory::Cell,
						&dataReader });
				}
				// Cell data that needs averaging
				else if (arrayInfo.type == fea_io::ArrayType::QuadPointByElement)
				{
					arrayReaders.push_back({
						std::make_unique<fea_io::ArrayInfo>(arrayInfo),
						ArrayToVTKCategory::CellAveraged,
						&dataReader });
				}
			}
		}
	}

	return arrayReaders;
}

void vtkFEAIOReader::CreateVTKMesh(
	const fea_io::MeshInfo<IndexType, IndexType, CoordType>* meshInfoPtr,
	const fea_io::Partition<IndexType, IndexType, CoordType>* currPartitionPtr,
	const fea_io::Subcomponent<IndexType, CoordType>* subcomponentPtr,
	vtkSmartPointer<vtkUnstructuredGrid>& mesh)
{
	auto& meshInfo = *meshInfoPtr;
	auto& currPartition = *currPartitionPtr;
	auto& subcomponent = *subcomponentPtr;

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
	if (!pointGhostArray)
	{
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
		{
			pointGhostArray->SetValue(
				vtkPointNumber,
				pointGhostArray->GetValue(vtkPointNumber) | vtkDataSetAttributes::DUPLICATEPOINT);
		}
	}

	// Create a map from the partition numbering to the subcomponent numbering
	std::map<IndexType, IndexType> partToSubcompNodeMap;
	for (auto i = 0; i < numPoints; i++)
	{
		partToSubcompNodeMap[subcomponent.nodes_in_subcomponent[i]] = i;
	}

	// Create the cells
	for (auto vtkElementNumber = 0; vtkElementNumber < numCells; vtkElementNumber++)
	{
		const IndexType indexInPartition = subcomponent.elements_in_subcomponent[vtkElementNumber];

		// Get connectivity
		unsigned int idIndex = 0;
		const auto& conn = currPartition.element_connectivities[indexInPartition];
		std::vector<vtkIdType> ids(conn.size());
		// Convert the connectivity to be relative to the subcomponent numbering
		for (const unsigned int partitionNodeNumber : conn)
		{
			ids[idIndex] = partToSubcompNodeMap[partitionNodeNumber];
			idIndex++;
		}

		// Insert a new VTK cell
		auto const elemType = currPartition.element_types[indexInPartition];
		auto ctIterator = cellTypeMap.find(elemType);
		if (ctIterator == cellTypeMap.end())
			vtkErrorMacro(<< "Element " + std::to_string(indexInPartition) + " (partition index) with type "
				<< static_cast<char>(elemType)
				<< " could not be converted to a VTK type.");
		VTKCellType ct = ctIterator->second;
		std::vector<vtkIdType> idsReordered = fea_io::get_to_vtk_reordering(meshInfo.connectivity_scheme, elemType, ids);
		if (idsReordered.size() == 0)
		{
			vtkErrorMacro("VTK mapping not found.");
		}
		if (elemType != fea_io::ElementType::POLYHEDRON)
		{
			mesh->InsertNextCell(ct, idsReordered.size(), &(idsReordered[0]));
		}
		else
		{
			// VTK requires a different interface for polyhedron cells
			//  ( cell type, number of faces, face stream )
			mesh->InsertNextCell(ct, idsReordered[0], &(idsReordered[1]));
		}
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
		{
			cellGhostArray->SetValue(
				vtkElementNumber,
				cellGhostArray->GetValue(vtkElementNumber) | vtkDataSetAttributes::DUPLICATECELL);
		}
	}
}

void vtkFEAIOReader::CreateBlocksForPartitionByComponent(
	vtkSmartPointer<vtkMultiBlockDataSet>& output,
	const int currLevelOfDetailBlockID,
	const std::vector<short>& cellRelativeLevels,
	const fea_io::MeshInfo<IndexType, IndexType, CoordType>* meshInfoPtr,
	const fea_io::Partition<IndexType, IndexType, CoordType>* currPartitionPtr,
	vtkDataArraySelection* PointDataArraySelection,
	vtkDataArraySelection* CellDataArraySelection,
	const std::vector<ArrayReaderPair>& arrayPairs)
{
	auto& meshInfo = *meshInfoPtr;
	auto& currPartition = *currPartitionPtr;

	// Create the meshes
	unsigned int componentBlockID = 0;
	for (auto componentID : this->ComponentsNeeded)
	{
		// Find the subcomponent of the current component in the current partition
		auto find_iter = std::find_if(currPartition.subcomponents.begin(),
			currPartition.subcomponents.end(),
			[&componentID](const fea_io::Subcomponent<IndexType, CoordType>& subcomp) -> bool
			{
				return subcomp.component_id == componentID;
			});
		// If the component does not have a subcomponent on this partition, then move on to the next component
		if (find_iter == currPartition.subcomponents.end())
			continue;
		auto& subcomponent = *find_iter;

		// Get the VTK UnstructuredGrid container and fill it
		auto componentMultiBlock = vtkMultiBlockDataSet::SafeDownCast(output->GetBlock(componentBlockID));
		auto lodMultiPiece = vtkMultiPieceDataSet::SafeDownCast(componentMultiBlock->GetBlock(currLevelOfDetailBlockID));
		vtkSmartPointer<vtkUnstructuredGrid> mesh = vtkUnstructuredGrid::SafeDownCast(
			lodMultiPiece->GetPiece(currPartition.partition_id));
		mesh->Initialize();
		this->CreateVTKMesh(&meshInfo, &currPartition, &subcomponent, mesh);

		// Create bool array for active elements
		if (cellRelativeLevels.empty())
		{
			// Set all to true (1)
			mesh->GetCellData()->AddArray(
				CreatePartitionArrayForComponent<vtkShortArray>(
					"RelativeLevel",
					static_cast<short>(currLevelOfDetailBlockID),
					subcomponent.elements_in_subcomponent));
		}
		else
		{
			mesh->GetCellData()->AddArray(
				CreatePartitionArrayForComponent<vtkShortArray>(
					"RelativeLevel",
					1,
					cellRelativeLevels,
					subcomponent.elements_in_subcomponent));
		}

		// Create point array for global numbers
		if (PointDataArraySelection->ArrayIsEnabled("GlobalIDs"))
		{
			mesh->GetPointData()->AddArray(
				CreatePartitionArrayForComponent<VTKIndexArrayType>(
					"PointGlobalIDs",
					1,
					currPartition.node_global_ids,
					subcomponent.nodes_in_subcomponent));
		}

		// Create point array for owning partition
		if (PointDataArraySelection->ArrayIsEnabled("PartitionIDs"))
		{
			mesh->GetPointData()->AddArray(
				CreatePartitionArrayForComponent<VTKIndexArrayType>(
					"PointPartitionIDs",
					1,
					currPartition.node_parent_partitions,
					subcomponent.nodes_in_subcomponent));
		}

		// Create cell array for global numbers
		if (CellDataArraySelection->ArrayIsEnabled("GlobalIDs"))
		{
			mesh->GetCellData()->AddArray(
				CreatePartitionArrayForComponent<VTKIndexArrayType>(
					"CellGlobalIDs", 
					1,
					currPartition.element_global_ids,
					subcomponent.elements_in_subcomponent));
		}

		// Create cell array for owning partition
		if (CellDataArraySelection->ArrayIsEnabled("PartitionIDs"))
		{
			mesh->GetCellData()->AddArray(
				CreatePartitionArrayForComponent<VTKIndexArrayType>(
					"CellPartitionIDs",
					1,
					currPartition.element_parent_partitions,
					subcomponent.elements_in_subcomponent));
		}

		// Create cell array for domain
		if (CellDataArraySelection->ArrayIsEnabled("ComponentIDs"))
		{
			mesh->GetCellData()->AddArray(
				CreatePartitionArrayForComponent<VTKIndexArrayType>(
					"CellComponentIDs",
					subcomponent.component_id,
					subcomponent.elements_in_subcomponent));
		}

		// Create vectors for the local coordinate systems
		if (PointDataArraySelection->ArrayIsEnabled("LocalCoordinateSystems")
			|| CellDataArraySelection->ArrayIsEnabled("LocalCoordinateSystems")) {
			// Find the corresponding component in meshInfo for this subcomponent
			auto findIter = std::find_if(meshInfo.components.begin(), meshInfo.components.end(),
				[id = subcomponent.component_id](const auto& c) -> bool {
				return c.id == id;
			});
			if (findIter == meshInfo.components.end())
			{
				throw _MSG_EXCEPT("Could not find the component corresponding to the given subcomponent.");
			}
			const fea_io::ComponentInfo<IndexType>& componentInfo = (*findIter);

			if (componentInfo.lcs_assignment == fea_io::LCSAssignment::Node) {
				auto lcsArrays = CreateLocalCoordinateSystems<IndexType, CoordType, vtkFloatArray>(
					componentInfo,
					subcomponent);
				if (lcsArrays[0]->GetNumberOfTuples() != mesh->GetNumberOfPoints())
				{
					throw _MSG_EXCEPT_C("The number lcs parameters for subcomponent " +
						std::to_string(subcomponent.component_id) + " in partition " +
						std::to_string(currPartition.partition_id) +
						" did not match the number of points.");
				}
				mesh->GetPointData()->AddArray(lcsArrays[0]);
				mesh->GetPointData()->AddArray(lcsArrays[1]);
				mesh->GetPointData()->AddArray(lcsArrays[2]);
			}
			else if (componentInfo.lcs_assignment == fea_io::LCSAssignment::Element) {
				auto lcsArrays = CreateLocalCoordinateSystems<IndexType, CoordType, vtkFloatArray>(
					componentInfo,
					subcomponent);
				if (lcsArrays[0]->GetNumberOfTuples() != mesh->GetNumberOfCells())
				{
					throw _MSG_EXCEPT_C("The number lcs parameters for subcomponent " +
						std::to_string(subcomponent.component_id) +
						" in partition " + std::to_string(currPartition.partition_id) +
						" did not match the number of cells.");
				}
				mesh->GetCellData()->AddArray(lcsArrays[0]);
				mesh->GetCellData()->AddArray(lcsArrays[1]);
				mesh->GetCellData()->AddArray(lcsArrays[2]);
			}
			else if (componentInfo.lcs_assignment == fea_io::LCSAssignment::Component) {
				auto lcsArrays = CreateLocalCoordinateSystems<IndexType, CoordType, vtkFloatArray>(
					componentInfo.lcs_parameterization,
					subcomponent.lcs_parameters_in_subcomponent,
					mesh->GetNumberOfCells());
				mesh->GetCellData()->AddArray(lcsArrays[0]);
				mesh->GetCellData()->AddArray(lcsArrays[1]);
				mesh->GetCellData()->AddArray(lcsArrays[2]);
			}
			else if (componentInfo.lcs_assignment == fea_io::LCSAssignment::None)
			{
				auto lcsArrays = CreateLocalCoordinateSystems<IndexType, CoordType, vtkFloatArray>(
					fea_io::LCSParameterization::Quaternion,
					{1, 0, 0, 0},
					mesh->GetNumberOfCells());
				mesh->GetCellData()->AddArray(lcsArrays[0]);
				mesh->GetCellData()->AddArray(lcsArrays[1]);
				mesh->GetCellData()->AddArray(lcsArrays[2]);
			}
		}

		componentBlockID++;
	} // End loop over domains

	// Create data arrays
	for (auto& arrayPair : arrayPairs)
	{
		if (arrayPair.arrayInfo->data_type == fea_io::ArrayDataType::Integral)
		{
			// Handle various integral precisions
			if (arrayPair.arrayInfo->data_type_num_bytes == 2)
				CreateDataArrayForSubcomponents<int16_t, vtkShortArray>(output,
					currLevelOfDetailBlockID, &currPartition, arrayPair);
			else if (arrayPair.arrayInfo->data_type_num_bytes == 4)
				CreateDataArrayForSubcomponents<int32_t, vtkIntArray>(output,
					currLevelOfDetailBlockID, &currPartition, arrayPair);
			else if (arrayPair.arrayInfo->data_type_num_bytes == 8)
				CreateDataArrayForSubcomponents<int64_t, vtkLongLongArray> (output,
					currLevelOfDetailBlockID, &currPartition, arrayPair);
		}
		else if (arrayPair.arrayInfo->data_type == fea_io::ArrayDataType::FloatingPoint)
		{
			// Handle various floating point precisions
			if (arrayPair.arrayInfo->data_type_num_bytes == 4)
				CreateDataArrayForSubcomponents<float, vtkFloatArray>(output,
					currLevelOfDetailBlockID, &currPartition, arrayPair);
			else
				CreateDataArrayForSubcomponents<double, vtkDoubleArray>(output,
					currLevelOfDetailBlockID, &currPartition, arrayPair);
		}
		else
		{
			throw _MSG_EXCEPT_C("Data type not supported.");
		}
	}
}

template<typename ValueType, typename VTKArrayType>
inline void vtkFEAIOReader::CreateDataArrayForSubcomponents_Helper(
	vtkSmartPointer<vtkMultiBlockDataSet>& output,
	const int currLevelOfDetailBlockID,
	const fea_io::Partition<IndexType, IndexType, CoordType>* currPartitionPtr,
	const fea_io::ArrayInfo* arrayInfoPtr,
	const std::vector<ValueType>& data)
{
	auto& currPartition = *currPartitionPtr;
	auto& arrayInfo = *arrayInfoPtr;

	unsigned int componentBlockID = 0;
	std::size_t nodeByComponentOffset = 0;
	std::vector<IndexType> componentToDataIndexMap;
	for (auto componentID : this->ComponentsNeeded)
	{
		// Find the subcomponent of the current component in the current partition
		auto find_iter = std::find_if(currPartition.subcomponents.begin(),
			currPartition.subcomponents.end(),
			[&componentID](const fea_io::Subcomponent<IndexType, CoordType>& subcomp) -> bool
			{
				return subcomp.component_id == componentID;
			});
		// If the component does not have a subcomponent on this partition, then move on to the next component
		if (find_iter == currPartition.subcomponents.end())
			continue;
		const fea_io::Subcomponent<IndexType, CoordType>& subcomponent = *find_iter;

		// Get the VTK mesh from the multiblock data structure
		auto componentMultiBlock = vtkMultiBlockDataSet::SafeDownCast(output->GetBlock(componentBlockID));
		auto lodMultiPiece = vtkMultiPieceDataSet::SafeDownCast(componentMultiBlock->GetBlock(currLevelOfDetailBlockID));
		vtkSmartPointer<vtkUnstructuredGrid> mesh = vtkUnstructuredGrid::SafeDownCast(
			lodMultiPiece->GetPiece(currPartition.partition_id));

		if (arrayInfo.type == fea_io::ArrayType::Node) {
			mesh->GetPointData()->AddArray(
				CreatePartitionArrayForComponent<VTKArrayType>(
					arrayInfo.name,
					arrayInfo.num_comps,
					data,
					subcomponent.nodes_in_subcomponent));
		}
		else if (arrayInfo.type == fea_io::ArrayType::Element || arrayInfo.type == fea_io::ArrayType::QuadPointByElement) {
			// Note: ArrayType::QuadPointByElement data has been averaged to be one per a cell in CreateDataArrayForSubcomponents
			mesh->GetCellData()->AddArray(
				CreatePartitionArrayForComponent<VTKArrayType>(
					arrayInfo.name,
					arrayInfo.num_comps,
					data,
					subcomponent.elements_in_subcomponent));
		}
		else if (arrayInfo.type == fea_io::ArrayType::NodeByComponent) {
			// Note: the subcomponents were written sequentially to the block of data for this partition, so generate the relevant indices
			componentToDataIndexMap.resize(subcomponent.nodes_in_subcomponent.size());
			std::iota(componentToDataIndexMap.begin(), componentToDataIndexMap.end(), static_cast<int>(nodeByComponentOffset));
			mesh->GetPointData()->AddArray(
				CreatePartitionArrayForComponent<VTKArrayType>(
					arrayInfo.name,
					arrayInfo.num_comps,
					data,
					componentToDataIndexMap));
			nodeByComponentOffset += subcomponent.nodes_in_subcomponent.size();
		}
		else
			throw _MSG_EXCEPT_C("Array types requiring averaging have not been implemented yet.");

		componentBlockID++;
	}
}

template<typename ValueType, typename VTKArrayType>
void vtkFEAIOReader::CreateDataArrayForSubcomponents(
	vtkSmartPointer<vtkMultiBlockDataSet>& output,
	const int currLevelOfDetailBlockID,
	const fea_io::Partition<IndexType, IndexType, CoordType>* currPartitionPtr,
	const ArrayReaderPair & arrayReaderPair)
{
	auto& currPartition = *currPartitionPtr;

	std::vector<ValueType> data;
	// For Quad point data, we need to average to cells and the read call is different
	if (arrayReaderPair.arrayInfo->type == fea_io::ArrayType::QuadPointByElement) {
		std::vector<int16_t> numQuadsPerElem;
		arrayReaderPair.readerPtr->read_quad(currPartition.partition_id, arrayReaderPair.arrayInfo->name, data, numQuadsPerElem);

		// Average the quad point data to be one per cell
		std::size_t currOffset = 0;
		std::vector<ValueType> averagedData(currPartition.element_global_ids.size() * arrayReaderPair.arrayInfo->num_comps, 0);
		for (auto elemInd = 0; elemInd < numQuadsPerElem.size(); elemInd++) {
			for (auto quadInd = 0; quadInd < numQuadsPerElem[elemInd]; quadInd++)
				for (auto i = 0; i < arrayReaderPair.arrayInfo->num_comps; i++)
					averagedData[arrayReaderPair.arrayInfo->num_comps * elemInd + i] += data[currOffset + arrayReaderPair.arrayInfo->num_comps * quadInd + i] / numQuadsPerElem[elemInd];
			currOffset += arrayReaderPair.arrayInfo->num_comps * numQuadsPerElem[elemInd];
		}

		CreateDataArrayForSubcomponents_Helper<ValueType, VTKArrayType>(
			output,
			currLevelOfDetailBlockID,
			&currPartition,
			arrayReaderPair.arrayInfo.get(),
			averagedData);
	}
	else {
		arrayReaderPair.readerPtr->read(currPartition.partition_id, arrayReaderPair.arrayInfo->name, data);
		CreateDataArrayForSubcomponents_Helper<ValueType, VTKArrayType>(
			output,
			currLevelOfDetailBlockID,
			&currPartition,
			arrayReaderPair.arrayInfo.get(),
			data);
	}
}

void vtkFEAIOReader::SetupAndReadMultiblock(
	const int currTimeIndex,
	const int numLevelsOfDetail,
	const fea_io::MetaReader * metaReaderPtr,
	const fea_io::MeshObjectType * typePtr,
	vtkDataArraySelection* PointDataArraySelection,
	vtkDataArraySelection* CellDataArraySelection,
	vtkSmartPointer<vtkMultiBlockDataSet>& output)
{

	// Create the unstructured grid for each partition of each mesh object for each
	// level of detail for each component.
	SetupMultiblock(output,
		this->ComponentsNeeded,
		numLevelsOfDetail,
		this->PartitionSelection->GetNumberOfArrays(),
		*typePtr);

	// Get the levels of detail in reverse order in order to determine the cells in the
	// parent level that should be skipped
	auto relevantLoDsSorted =
		fea_io::get_reversed_lods<IndexType, IndexType, CoordType>(*metaReaderPtr, currTimeIndex);
	auto lodMeshObjects = fea_io::get_mesh_obj_for_each_level(
		relevantLoDsSorted,
		*typePtr);

	std::vector<fea_io::MeshInfo<IndexType, IndexType, CoordType>> meshInfoByLevel;
	std::vector<std::vector<ArrayReaderPair>> arrayReadersByLevel;
	std::vector<std::unique_ptr<fea_io::DataReader>> dataReaderPtrs;
	for (auto i = 0; i < lodMeshObjects.size(); i++)
	{
		if (lodMeshObjects[i].mesh_path != "")
		{
			fea_io::MeshReader<IndexType, IndexType, CoordType> meshReader(
				metaReaderPtr->get_adjusted_path(lodMeshObjects[i].mesh_path));
			meshInfoByLevel.push_back(meshReader.get_mesh_info());

			arrayReadersByLevel.push_back(this->GetDataArrayReaders(
				metaReaderPtr,
				&lodMeshObjects[i],
				PointDataArraySelection,
				CellDataArraySelection,
				dataReaderPtrs));
		}
		else
		{
			arrayReadersByLevel.push_back({});
		}
	}

	// Read what group files exist (Need to finish group implementation)
	//auto groupFilePaths = metaReader.get_group_file_paths(timeID);

	// Though the structure has the components at the top and then partitions, we will
	// loop over partitions then domains for more efficient I/O
	for (auto partitionIndex : this->MyPartitionIndices)
	{
		auto partID = this->AllPartitionsNeeded[partitionIndex];

		std::vector<short> lastParentElementRelativeLevels;
		for (auto i = 0; i < lodMeshObjects.size(); i++)
		{
			if (lodMeshObjects[i].mesh_path != "")
			{
				auto lodWithPart = fea_io::read_level_of_detail_helper<IndexType, IndexType, CoordType>(
					*metaReaderPtr,
					relevantLoDsSorted,
					lodMeshObjects,
					i, // index of relevantLoDsSorted to read
					partID,
					*typePtr);

				// This is the main method that creates a mesh with data for each subcomponent
				// in this partition.
				CreateBlocksForPartitionByComponent(
					output,
					relevantLoDsSorted[i].level,
					lastParentElementRelativeLevels,
					&meshInfoByLevel[i],
					&lodWithPart.mesh_object_part,
					PointDataArraySelection,
					CellDataArraySelection,
					arrayReadersByLevel[i]);

				lastParentElementRelativeLevels.swap(lodWithPart.parent_element_relative_levels);
			}
			else
			{
				lastParentElementRelativeLevels.clear();
			}
		}
	} // End loop over partitions
}

void vtkFEAIOReader::UpdateOutput(
	vtkSmartPointer<vtkMultiBlockDataSet>& input, 
	vtkSmartPointer<vtkMultiBlockDataSet>& output,
	const int selectedLevel)
{
	// Filter the volume mesh based on the level of detail selected
	output->CopyStructure(input);
	auto blockIter = input->NewIterator();
	blockIter->SkipEmptyNodesOn();
	for (blockIter->InitTraversal(); !blockIter->IsDoneWithTraversal(); blockIter->GoToNextItem())
	{
		auto dataObject = blockIter->GetCurrentDataObject();
		if (dataObject->IsA("vtkUnstructuredGrid"))
		{
			auto unstructuredGrid = vtkUnstructuredGrid::SafeDownCast(dataObject);

			auto relativeLevelData = vtkShortArray::SafeDownCast(
				unstructuredGrid->GetCellData()->GetArray("RelativeLevel"));

			// Get the ids of cells to keep
			// It must be between 0 and the selected level
			// OR equal to -(selected level + 1)
			vtkSmartPointer<vtkIdTypeArray> cellIdsToKeep = vtkSmartPointer<vtkIdTypeArray>::New();
			cellIdsToKeep->SetNumberOfComponents(1);
			auto cellIter = unstructuredGrid->NewCellIterator();
			for (cellIter->InitTraversal(); !cellIter->IsDoneWithTraversal(); cellIter->GoToNextCell())
			{
				auto cellId = cellIter->GetCellId();
				auto relativeLevel = relativeLevelData->GetValue(cellId);
				if ((relativeLevel >= 0 && relativeLevel <= selectedLevel)
					|| (relativeLevel == -(selectedLevel + 1)))
				{
					cellIdsToKeep->InsertNextValue(cellId);
				}
			}
			cellIter->Delete();

			output->SetDataSet(blockIter, unstructuredGrid->New());

			// Extract the cells for this block from the input and put them
			// in the new grid created in the output
			{
				vtkSmartPointer<vtkSelectionNode> selectionNode =
					vtkSmartPointer<vtkSelectionNode>::New();
				selectionNode->SetFieldType(vtkSelectionNode::CELL);
				selectionNode->SetContentType(vtkSelectionNode::INDICES);
				selectionNode->SetSelectionList(cellIdsToKeep);

				vtkSmartPointer<vtkSelection> selection =
					vtkSmartPointer<vtkSelection>::New();
				selection->AddNode(selectionNode);

				vtkSmartPointer<vtkExtractSelectedIds> extractSelectedIds =
					vtkSmartPointer<vtkExtractSelectedIds>::New();
				extractSelectedIds->SetInputDataObject(0, unstructuredGrid);
				extractSelectedIds->SetInputData(1, selection);
				extractSelectedIds->SetOutput(output->GetDataSet(blockIter));
				extractSelectedIds->Update();
			}
		}
		else
		{
			vtkErrorMacro(<< "This implementation only works with composite data sets of vtkUnstructuredGrid, but "
				<< blockIter->GetCurrentDataObject()->GetClassName()
				<< "was encountered.");
		}
	}
	blockIter->Delete();
}