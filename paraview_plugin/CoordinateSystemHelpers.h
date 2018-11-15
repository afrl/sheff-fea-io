#pragma once


/* TODO: this might be more useful than the next function, but consider what to do with it.
void CreateOrientSinglePerCellArray(
	const std::vector<std::vector<std::array<double, 4>>>& nodalByCellData,
	const std::map<unsigned int, unsigned int>& nodeOldLocalToNewLocalMap,
	const std::vector<unsigned int>& localNodeIndicesUsed,
	const std::vector<unsigned int>& localElementIndicesUsed,
	const PFECNativeInterface::ElementInfo& elementInfo,
	const bool needX,
	const bool needY,
	const bool needZ,
	vtkSmartPointer<vtkUnstructuredGrid>& mesh)
{
	vtkFloatArray* cellArrayX;
	vtkFloatArray* cellArrayY;
	vtkFloatArray* cellArrayZ;
	if (needX) {
		cellArrayX = vtkFloatArray::New();
		cellArrayX->SetName("CellLocalX");
		cellArrayX->SetNumberOfComponents(3);
		cellArrayX->SetNumberOfTuples(localElementIndicesUsed.size());
	}
	if (needY) {
		cellArrayY = vtkFloatArray::New();
		cellArrayY->SetName("CellLocalY");
		cellArrayY->SetNumberOfComponents(3);
		cellArrayY->SetNumberOfTuples(localElementIndicesUsed.size());
	}
	if (needZ) {
		cellArrayZ = vtkFloatArray::New();
		cellArrayZ->SetName("CellLocalZ");
		cellArrayZ->SetNumberOfComponents(3);
		cellArrayZ->SetNumberOfTuples(localElementIndicesUsed.size());
	}
	Rotation r;
	GeneralVec localXVec(3); localXVec << 1.0, 0.0, 0.0;
	GeneralVec localYVec(3); localYVec << 0.0, 1.0, 0.0;
	GeneralVec localZVec(3); localZVec << 0.0, 0.0, 1.0;
	GeneralVec globalVec(3);
	for (auto i = 0; i < localElementIndicesUsed.size(); i++) {
		if (nodalByCellData[localElementIndicesUsed[i]].size() > 0)
			r.set_quaterion(nodalByCellData[localElementIndicesUsed[i]][0]);
		else r.zero_self();
		if (needX) {
			r.transform_3D_1st_Order_Tensor(localXVec, globalVec);
			cellArrayX->SetTuple(i, globalVec.data());
		}
		if (needY) {
			r.transform_3D_1st_Order_Tensor(localYVec, globalVec);
			cellArrayY->SetTuple(i, globalVec.data());
		}
		if (needZ) {
			r.transform_3D_1st_Order_Tensor(localZVec, globalVec);
			cellArrayZ->SetTuple(i, globalVec.data());
		}
	}
	if (needX) mesh->GetCellData()->AddArray(cellArrayX);
	if (needY) mesh->GetCellData()->AddArray(cellArrayY);
	if (needZ) mesh->GetCellData()->AddArray(cellArrayZ);
}
*/

/* TODO: there is no longer a need to average like this, but the different coordinate
		 systems need to be implemented and converted to x, y, z vectors.  Additionally,
		 consider just creating coordinate systems at the element by averaging parameters
		 from nodes/quads for simplicity of visualization.
void CreateOrientAveragedArray(
	const std::vector<std::vector<std::array<double, 4>>>& nodalByCellData,
	const std::map<unsigned int, unsigned int>& nodeOldLocalToNewLocalMap,
	const std::vector<unsigned int>& localNodeIndicesUsed,
	const std::vector<unsigned int>& localElementIndicesUsed,
	const PFECNativeInterface::ElementInfo& elementInfo,
	const bool needX,
	const bool needY,
	const bool needZ,
	vtkSmartPointer<vtkUnstructuredGrid>& mesh)
{
	std::vector< std::array<double, 4> > averagedData(localNodeIndicesUsed.size());
	std::vector<unsigned int> numEntriesSummed(localNodeIndicesUsed.size());
	const unsigned int numComps = 4;

	// First resize the number of components
	for (auto j = 0; j < averagedData.size(); j++) {
		for (auto k = 0; k < numComps; k++) averagedData[j][k] = 0.0;
		numEntriesSummed[j] = 0;
	}

	// Average the data
	for (auto elemInd : localElementIndicesUsed)
		for (auto j = 0; j < elementInfo.localConnectivity[elemInd].size(); j++) {
			unsigned int nodeInd = elementInfo.localConnectivity[elemInd][j];
			auto newNodeInd = nodeOldLocalToNewLocalMap.find(nodeInd)->second;
			if (nodalByCellData[elemInd].size() > 1) {
				for (auto k = 0; k < numComps; k++)
					averagedData[newNodeInd][k] += nodalByCellData[elemInd][j][k];
				numEntriesSummed[newNodeInd] += 1;
			}
			else if (nodalByCellData[elemInd].size() == 1) {
				for (auto k = 0; k < numComps; k++)
					averagedData[newNodeInd][k] += nodalByCellData[elemInd][0][k];
				numEntriesSummed[newNodeInd] += 1;
			}
		}
	for (auto j = 0; j < averagedData.size(); j++) {
		if (numEntriesSummed[j] > 0)
			for (auto k = 0; k < numComps; k++)
				averagedData[j][k] /= numEntriesSummed[j];
	}

	// Create the array
	Rotation r;
	GeneralVec localXVec(3); localXVec << 1.0, 0.0, 0.0;
	GeneralVec localYVec(3); localYVec << 0.0, 1.0, 0.0;
	GeneralVec localZVec(3); localZVec << 0.0, 0.0, 1.0;
	GeneralVec globalVec(3);
	vtkFloatArray* arrayX;
	vtkFloatArray* arrayY;
	vtkFloatArray* arrayZ;
	if (needX) {
		arrayX = vtkFloatArray::New();
		arrayX->SetName("PointLocalX");
		arrayX->SetNumberOfComponents(3);
		arrayX->SetNumberOfTuples(localNodeIndicesUsed.size());
	}
	if (needY) {
		arrayY = vtkFloatArray::New();
		arrayY->SetName("PointLocalY");
		arrayY->SetNumberOfComponents(3);
		arrayY->SetNumberOfTuples(localNodeIndicesUsed.size());
	}
	if (needZ) {
		arrayZ = vtkFloatArray::New();
		arrayZ->SetName("PointLocalZ");
		arrayZ->SetNumberOfComponents(3);
		arrayZ->SetNumberOfTuples(localNodeIndicesUsed.size());
	}
	for (auto j = 0; j < localNodeIndicesUsed.size(); j++) {
		r.set_quaterion(averagedData[j]);
		if (needX) {
			r.transform_3D_1st_Order_Tensor(localXVec, globalVec);
			arrayX->SetTuple(j, globalVec.data());
		}
		if (needY) {
			r.transform_3D_1st_Order_Tensor(localYVec, globalVec);
			arrayY->SetTuple(j, globalVec.data());
		}
		if (needZ) {
			r.transform_3D_1st_Order_Tensor(localZVec, globalVec);
			arrayZ->SetTuple(j, globalVec.data());
		}
	}
	if (needX) mesh->GetPointData()->AddArray(arrayX);
	if (needY) mesh->GetPointData()->AddArray(arrayY);
	if (needZ) mesh->GetPointData()->AddArray(arrayZ);
}
*/