#pragma once

#include "vtkCellType.h"
#include "vtkType.h"

#include "fea_io/mesh/MeshEnums.h"

namespace fea_io
{
	// TODO implement other number schemes here and have the ordering scheme indicated in the mesh JSON file

	auto get_to_vtk_cell_map(const fea_io::ConnectivityScheme scheme)
	{
		std::map<fea_io::ElementType, VTKCellType> typeMap;
		if (scheme == fea_io::ConnectivityScheme::FlexMesh){
			// 2D Elements
			typeMap[fea_io::ElementType::T2D3N] = VTK_TRIANGLE;
			typeMap[fea_io::ElementType::Q2D4N] = VTK_QUAD;
			typeMap[fea_io::ElementType::C2D4N] = VTK_QUAD;
			typeMap[fea_io::ElementType::T2D6N] = VTK_QUADRATIC_TRIANGLE; // Needs renumbering
			typeMap[fea_io::ElementType::Q2D8N] = VTK_QUADRATIC_QUAD; // Needs renumbering
			typeMap[fea_io::ElementType::C2D6N] = VTK_POLYGON; // Needs renumbering
			// 3D Elements
			// Linear
			typeMap[fea_io::ElementType::T3D4N] = VTK_TETRA;
			typeMap[fea_io::ElementType::P3D5N] = VTK_PYRAMID;
			typeMap[fea_io::ElementType::W3D6N] = VTK_WEDGE; // Needs renumbering
			typeMap[fea_io::ElementType::H3D8N] = VTK_HEXAHEDRON;
			typeMap[fea_io::ElementType::Q3D4N] = VTK_QUAD;
			typeMap[fea_io::ElementType::C3D8N] = VTK_HEXAHEDRON;
			// Quadratic
			typeMap[fea_io::ElementType::T3D10N] = VTK_QUADRATIC_TETRA;
			typeMap[fea_io::ElementType::P3D13N] = VTK_QUADRATIC_PYRAMID;
			typeMap[fea_io::ElementType::W3D15N] = VTK_QUADRATIC_WEDGE; // Needs renumbering
			typeMap[fea_io::ElementType::H3D20N] = VTK_QUADRATIC_HEXAHEDRON; // Needs renumbering
			typeMap[fea_io::ElementType::Q3D8N] = VTK_QUADRATIC_QUAD;  // Needs renumbering
			// To to figure out how to handle quadratic cohesive elements
			//typeMap[fea_io::ElementType::C3D16N] = ;
			// Other
			typeMap[fea_io::ElementType::POLYGON] = VTK_POLYGON;
			typeMap[fea_io::ElementType::POLYHEDRON] = VTK_POLYHEDRON;
		}
		else
		{
			throw _MSG_EXCEPT("Connectivity scheme not recognized.");
		}
		return typeMap;
	};

	std::vector<vtkIdType> get_to_vtk_reordering(
		const fea_io::ConnectivityScheme scheme,
		const fea_io::ElementType &geomType,
		const std::vector<vtkIdType> &pfecNodeIndices)
	{
		std::vector<vtkIdType> vtkNodeIndices(pfecNodeIndices.begin(), pfecNodeIndices.end());
		if (scheme == fea_io::ConnectivityScheme::FlexMesh)
		{
			// 2D reorders
			if (geomType == fea_io::ElementType::T2D6N)
			{
				int VTKMap_2_2[] = { 0,2,4,1,3,5 };
				for (unsigned int i = 0; i < pfecNodeIndices.size(); i++)
				{
					vtkNodeIndices[i] = pfecNodeIndices[VTKMap_2_2[i]];
				}
			}
			else if (geomType == fea_io::ElementType::Q2D8N || geomType == fea_io::ElementType::Q3D8N)
			{
				int VTKMap_2_3[] = { 0,2,4,6,1,3,5,7 };
				for (unsigned int i = 0; i < pfecNodeIndices.size(); i++)
				{
					vtkNodeIndices[i] = pfecNodeIndices[VTKMap_2_3[i]];
				}
			}
			else if (geomType == fea_io::ElementType::C2D6N)
			{
				int VTKMapCZ_2_6[] = { 0, 1, 2, 5, 4, 3 };
				for (unsigned int i = 0; i < pfecNodeIndices.size(); i++)
				{
					vtkNodeIndices[i] = pfecNodeIndices[VTKMapCZ_2_6[i]];
				}
			}
			// 3D reorders
			else if (geomType == fea_io::ElementType::W3D6N)
			{
				int VTKMap_3_6[] = { 0, 2, 1, 3, 5, 4 };
				for (unsigned int i = 0; i < pfecNodeIndices.size(); i++)
				{
					vtkNodeIndices[i] = pfecNodeIndices[VTKMap_3_6[i]];
				}
			}
			else if (geomType == fea_io::ElementType::W3D15N)
			{
				int VTKMap_3_15[] = { 0, 4, 2, 9, 13, 11, 5, 3, 1, 14, 12, 10, 6, 8, 7 };
				for (unsigned int i = 0; i < pfecNodeIndices.size(); i++)
				{
					vtkNodeIndices[i] = pfecNodeIndices[VTKMap_3_15[i]];
				}
			}
			else if (geomType == fea_io::ElementType::H3D20N)
			{
				int VTKMap_3_20[] = { 0, 2, 4, 6, 12, 14, 16, 18, 1, 3, 5, 7, 13, 15, 17, 19, 8, 9, 10, 11 };
				for (unsigned int i = 0; i < pfecNodeIndices.size(); i++)
				{
					vtkNodeIndices[i] = pfecNodeIndices[VTKMap_3_20[i]];
				}
			}
		}
		return vtkNodeIndices;
	}
};