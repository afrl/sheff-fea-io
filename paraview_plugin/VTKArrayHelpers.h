#pragma once

#include <string>
#include <vector>

template<typename PVType, typename ValueType>
PVType* CreateSingleArray(
	const std::string& arrayName,
	const int numComps,
	const std::vector<ValueType>& values,
	const std::vector<unsigned int>& localIndicesUsed)
{
	PVType* arr = PVType::New();
	arr->SetName(arrayName.c_str());
	arr->SetNumberOfComponents(1);
	arr->SetNumberOfTuples(localIndicesUsed.size());
	for (auto i = 0; i < localIndicesUsed.size(); i++)
		arr->SetTuple1(i, values[localIndicesUsed[i]]);
	return arr;
}

/*
template<typename PVType, typename ValueType>
PVType* CreateNodalAveragedArray(const std::string& arrayName,
	const unsigned int numComps,
	const std::vector<std::vector<std::vector<ValueType>>>& nodalByCellData,
	const std::map<unsigned int, unsigned int>& nodeOldLocalToNewLocalMap,
	const std::vector<unsigned int>& localNodeIndicesUsed,
	const std::vector<unsigned int>& localElementIndicesUsed,
	const PFECNativeInterface::ElementInfo& elementInfo)
{
	std::vector<std::vector<ValueType>> averagedData(localNodeIndicesUsed.size());
	std::vector<unsigned int> numEntriesSummed(localNodeIndicesUsed.size());

	// First resize the number of components
	for (auto j = 0; j < averagedData.size(); j++) {
		averagedData[j].resize(numComps);
		for (auto k = 0; k < numComps; k++) averagedData[j][k] = 0.0;
		numEntriesSummed[j] = 0;
	}
	// Average the data
	for (auto elemInd : localElementIndicesUsed)
		for (auto j = 0; j < elementInfo.localConnectivity[elemInd].size(); j++) {
			auto nodeInd = elementInfo.localConnectivity[elemInd][j];
			auto newNodeInd = nodeOldLocalToNewLocalMap.find(nodeInd)->second;
			for (auto k = 0; k < numComps; k++) {
				averagedData[newNodeInd][k] += nodalByCellData[elemInd][j][k];
			}
			numEntriesSummed[newNodeInd] += 1;
		}
	for (auto j = 0; j < averagedData.size(); j++)
		for (auto k = 0; k < numComps; k++)
			averagedData[j][k] /= numEntriesSummed[j];

	// Create the array
	PVType* pointArray = PVType::New();
	pointArray->SetName(arrayName.c_str());
	pointArray->SetNumberOfComponents(numComps);
	pointArray->SetNumberOfTuples(localNodeIndicesUsed.size());
	for (auto j = 0; j < localNodeIndicesUsed.size(); j++) {
		pointArray->SetTuple(j, &averagedData[j][0]);
	}
	return pointArray;
}
*/