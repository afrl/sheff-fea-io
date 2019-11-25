#pragma once

#include "Eigen/Geometry"

#include "fea_io/mesh/MeshInfoStructs.h"
#include "fea_io/mesh/MeshStructs.h"

/// Creates an array for quantities that on a partition basis
/// This is really only used for creating arrays for mesh information, since points and cells
/// global IDs, owning partitions, etc. are on a partition basis.  Data is on a subcomponent basis.
template<typename VTKArrayType, typename IndexType, typename ValueType>
VTKArrayType* CreatePartitionArrayForComponent(
	const std::string& arrayName,
	const int64_t numComps,
	const std::vector<ValueType>& partitionValues,
	const std::vector<IndexType>& subcomponentIndices)
{
	return CreatePartitionArrayForComponent<VTKArrayType, IndexType, ValueType>(
		arrayName,
		numComps,
		partitionValues.data(),
		subcomponentIndices);
}

/// Creates an array for quantities that on a partition basis
/// This is really only used for creating arrays for mesh information, since points and cells
/// global IDs, owning partitions, etc. are on a partition basis.  Data is on a subcomponent basis.
template<typename VTKArrayType, typename IndexType, typename ValueType>
VTKArrayType* CreatePartitionArrayForComponent(
	const std::string& arrayName,
	const int64_t numComps,
	const ValueType* partitionValues,
	const std::vector<IndexType>& subcomponentIndices)
{
	VTKArrayType* arr = VTKArrayType::New();
	arr->SetName(arrayName.c_str());
	arr->SetNumberOfComponents(numComps);
	const int64_t numValues = static_cast<int64_t>(subcomponentIndices.size());
	arr->SetNumberOfTuples(numValues);
	for (auto i = 0; i < numValues; i++)
	{
		arr->SetTypedTuple(
			i,
			&partitionValues[numComps*subcomponentIndices[i]]);
	}
	return arr;
}

/// Creates an array for quantities that on a partition basis
/// Assigns every node or element to the same value
/// This is really only used for creating arrays for mesh information, since points and cells
/// global IDs, owning partitions, etc. are on a partition basis.  Data is on a subcomponent basis.
template<typename VTKArrayType, typename IndexType, typename ValueType>
VTKArrayType* CreatePartitionArrayForComponent(
	const std::string& arrayName,
	const ValueType& value,
	const std::vector<IndexType>& subcomponentIndices)
{
	VTKArrayType* arr = VTKArrayType::New();
	arr->SetName(arrayName.c_str());
	arr->SetNumberOfComponents(1);
	const int64_t numValues = static_cast<int64_t>(subcomponentIndices.size());
	arr->SetNumberOfTuples(numValues);
	for (auto i = 0; i < numValues; i++)
	{
		arr->SetTypedTuple(i, &value);
	}
	return arr;
}

/// Creates three VTK arrays for the local coordinate systems of points/cells
template<typename IndexType, typename CoordType, typename VTKArrayType>
std::array<VTKArrayType*, 3> CreateLocalCoordinateSystems(
	const fea_io::ComponentInfo<IndexType>& componentInfo,
	const fea_io::Subcomponent<IndexType, CoordType>& subcomponent)
{
	// Note: LCS = local coordinate system
	const auto numParametersPerValue = fea_io::get_num_parameters(componentInfo.lcs_parameterization);
	const auto numLCSs = static_cast<decltype(numParametersPerValue)>(subcomponent.lcs_parameters_in_subcomponent.size()) / numParametersPerValue;

	VTKArrayType* localXArray = VTKArrayType::New();
	localXArray->SetName("LocalX");
	localXArray->SetNumberOfComponents(3);
	localXArray->SetNumberOfTuples(numLCSs);

	VTKArrayType* localYArray = VTKArrayType::New();
	localYArray->SetName("LocalY");
	localYArray->SetNumberOfComponents(3);
	localYArray->SetNumberOfTuples(numLCSs);

	VTKArrayType* localZArray = VTKArrayType::New();
	localZArray->SetName("LocalZ");
	localZArray->SetNumberOfComponents(3);
	localZArray->SetNumberOfTuples(numLCSs);

	Eigen::Matrix<CoordType, 3, 3> rotationMatrix;
	for (auto i = 0; i < numLCSs; i++)
	{
		if (componentInfo.lcs_parameterization == fea_io::LCSParameterization::Quaternion)
		{
			Eigen::Quaternion<CoordType> quaternion(
				subcomponent.lcs_parameters_in_subcomponent[numParametersPerValue*i],
				subcomponent.lcs_parameters_in_subcomponent[numParametersPerValue*i + 1],
				subcomponent.lcs_parameters_in_subcomponent[numParametersPerValue*i + 2],
				subcomponent.lcs_parameters_in_subcomponent[numParametersPerValue*i + 3]);
			rotationMatrix = quaternion.toRotationMatrix();
		}
		else if (componentInfo.lcs_parameterization == fea_io::LCSParameterization::AxisAngle)
		{
			Eigen::Matrix<CoordType, 3, 1> axis(
				&subcomponent.lcs_parameters_in_subcomponent[numParametersPerValue*i]);
			Eigen::AngleAxis<CoordType> axisAngle(
				subcomponent.lcs_parameters_in_subcomponent[numParametersPerValue*i + 3],
				axis);
			rotationMatrix = axisAngle.toRotationMatrix();
		}
		localXArray->InsertTuple3(i, rotationMatrix(0, 0), rotationMatrix(1, 0), rotationMatrix(2, 0));
		localYArray->InsertTuple3(i, rotationMatrix(0, 1), rotationMatrix(1, 1), rotationMatrix(2, 1));
		localZArray->InsertTuple3(i, rotationMatrix(0, 2), rotationMatrix(1, 2), rotationMatrix(2, 2));
	}

	return { localXArray, localYArray, localZArray };
}

/// Creates three VTK arrays for the local coordinate systems of cells when the entire component
/// has the same value. Is also used to assign the LCS for materials with no rotation.
template<typename IndexType, typename CoordType, typename VTKArrayType>
std::array<VTKArrayType*, 3> CreateLocalCoordinateSystems(
	const fea_io::LCSParameterization lcsParameterization,
	const std::vector<CoordType>& singleLCSParameters,
	const IndexType numberOfCells)
{
	// Note: LCS = local coordinate system
	const auto numParametersPerValue = fea_io::get_num_parameters(lcsParameterization);
	if (singleLCSParameters.size() != numParametersPerValue)
		throw _MSG_EXCEPT("The number of LCS parameters passed does not match the number expected.");
	const auto numLCSs = numberOfCells;

	VTKArrayType* localXArray = VTKArrayType::New();
	localXArray->SetName("LocalX");
	localXArray->SetNumberOfComponents(3);
	localXArray->SetNumberOfTuples(numLCSs);

	VTKArrayType* localYArray = VTKArrayType::New();
	localYArray->SetName("LocalY");
	localYArray->SetNumberOfComponents(3);
	localYArray->SetNumberOfTuples(numLCSs);

	VTKArrayType* localZArray = VTKArrayType::New();
	localZArray->SetName("LocalZ");
	localZArray->SetNumberOfComponents(3);
	localZArray->SetNumberOfTuples(numLCSs);

	Eigen::Matrix<CoordType, 3, 3> rotationMatrix;
	if (lcsParameterization == fea_io::LCSParameterization::Quaternion)
	{
		Eigen::Quaternion<CoordType> quaternion(
			singleLCSParameters[0],
			singleLCSParameters[1],
			singleLCSParameters[2],
			singleLCSParameters[3]);
		rotationMatrix = quaternion.toRotationMatrix();
	}
	else if (lcsParameterization == fea_io::LCSParameterization::AxisAngle)
	{
		Eigen::Matrix<CoordType, 3, 1> axis(&singleLCSParameters[0]);
		Eigen::AngleAxis<CoordType> axisAngle(singleLCSParameters[3], axis);
		rotationMatrix = axisAngle.toRotationMatrix();
	}

	for (auto i = 0; i < numLCSs; i++)
	{
		localXArray->InsertTuple3(
			i, 
			rotationMatrix(0, 0),
			rotationMatrix(1, 0),
			rotationMatrix(2, 0));
		localYArray->InsertTuple3(
			i,
			rotationMatrix(0, 1),
			rotationMatrix(1, 1),
			rotationMatrix(2, 1));
		localZArray->InsertTuple3(
			i,
			rotationMatrix(0, 2),
			rotationMatrix(1, 2),
			rotationMatrix(2, 2));
	}

	return { localXArray, localYArray, localZArray };
}
