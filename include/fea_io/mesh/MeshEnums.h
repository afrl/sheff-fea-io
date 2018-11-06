#pragma once

#include <nlohmann/json.hpp>

#include "fea_io/common/Exceptions.h"

namespace fea_io
{
	using json = nlohmann::json;


	/// Dev note: if you add new ones, do not change the number of old types so that old
	/// mesh files remain compatible.  Feel free to add a value where ever you like though,
	/// it does not need to be contiguous.
	/// IMPORTANT NOTE: if you add a new element type that you want to be able to vizualize
	/// using ParaView, you need to update the pfecDefines, any related functions in that
	/// header that does something for all the geom types, and the ParaViewReader project file
	/// vtkPFECReader.cxx methods get_pfec_to_vtk_cell_map and get_pfec_to_vtk_reordering.
	/// The ParaView reader and PFEC does not depend on BetaMesh, so you need to update that separately.
	enum class ElementType : char {
		// Unknown
		UNKNGEOM = 0, // Used if you all else fails, though you will need to fix it before an analysis
		GENERALPOLYGON = 2,// polygon (usually for superelements)
		// 1D types 10-19
		DEF1D = 10, // 1-D Default
		L1D2N = 12, // 1-D Linear 2-node linear element
		L1D3N = 13, // 1-D Linear 3-node linear element
		// 2D types 20-24 is linear and 25-29 is quadratic
		DEF2D = 20, // 2-D Default
		T2D3N = 21, // triangular 2D 3-node element (linear)
		Q2D4N = 22, // quadrilateral 4-node linear element
		C2D4N = 24, // line 4-node cohesive element (linear)
		T2D6N = 25, // triangular 2D 6-node element (quadratic)
		Q2D8N = 26, // quadrilateral 8-node quadratic element (quadratic)
		C2D6N = 28, // line 6-node cohesive element (quadratic)
		// Linear 3D 30-39
		DEF3D = 30, // 3-D Default
		T3D4N = 31, // 3D tetrahedral 4-node  (linear) element
		P3D5N = 35, // 3D pyramid 5-node  (linear) element
		W3D6N = 32, // 3D wedge 6-node  (linear) element
		H3D8N = 33, // 3D hexahedral 8-node (linear) element
		Q3D4N = 36, // quadrilateral shell element  (linear)
		C3D8N = 38, // quadrilateral 8-node cohesive element (linear)
		// Quadratic 3D 41-49
		T3D10N = 41, // 3D tetrahedral 10-node  (quadratic) element
		P3D13N = 45, // 3D pyramid 13-node (quadratic) element
		W3D15N = 42, // 3D wedge 15-node  (quadratic) element
		H3D20N = 43, // 3D hexahedral 20-node (quadratic) element
		Q3D8N = 46, // quadrilateral shell element  (quadratic)
		C3D16N = 48, // quadrilateral 16-node cohesive element (quadratic)
	};

	/// Returns the number of nodes that the geometry type will have.
	static unsigned int get_num_nodes_for_type(const ElementType& geomType)
	{
		// 1D
		if (geomType == ElementType::L1D2N) return 2;
		else if (geomType == ElementType::L1D3N) return 3;
		// 2D
		else if (geomType == ElementType::T2D3N) return 3;
		else if (geomType == ElementType::Q2D4N) return 4;
		else if (geomType == ElementType::C2D4N) return 4;
		else if (geomType == ElementType::T2D6N) return 6;
		else if (geomType == ElementType::Q2D8N) return 8;
		else if (geomType == ElementType::C2D6N) return 6;
		// 3D
		else if (geomType == ElementType::T3D4N) return 4;
		else if (geomType == ElementType::P3D5N) return 5;
		else if (geomType == ElementType::W3D6N) return 6;
		else if (geomType == ElementType::H3D8N) return 8;
		else if (geomType == ElementType::Q3D4N) return 4;
		else if (geomType == ElementType::C3D8N) return 8;
		else if (geomType == ElementType::T3D10N) return 10;
		else if (geomType == ElementType::P3D13N) return 13;
		else if (geomType == ElementType::W3D15N) return 15;
		else if (geomType == ElementType::H3D20N) return 20;
		else if (geomType == ElementType::Q3D8N) return 8;
		else if (geomType == ElementType::C3D16N) return 16;
		// No match
		else return 0;
	}

	/// An enum describing what level the local coordinate system will be
	/// assigned for a component.  This could be:
	/// - None: no LCS for the entire component
	/// - Component: one LCS for the entire component
	/// - Element: one LCS for each element in the component
	/// - Node: one LCS for each node in the component
	enum class LCSAssignment : char
	{
		None,
		Component,
		Element,
		Node
	};

	std::string to_friendly_name(const LCSAssignment& lcs_a)
	{
		if (lcs_a == LCSAssignment::None) return "None";
		else if (lcs_a == LCSAssignment::Component) return "Component";
		else if (lcs_a == LCSAssignment::Element) return "Element";
		else if (lcs_a == LCSAssignment::Node) return "Node";
		else throw _MSG_EXCEPT("LCSAssignment " + std::to_string(static_cast<std::underlying_type<ElementType>::type>(lcs_a)) + " not recognized.");
	}

	LCSAssignment assignment_from_name(const std::string& name)
	{
		if (name == "None") return LCSAssignment::None;
		else if (name == "Component") return LCSAssignment::Component;
		else if (name == "Element") return LCSAssignment::Element;
		else if (name == "Node") return LCSAssignment::Node;
		else throw _MSG_EXCEPT("LCSAssignment " + name + " not recognized.");
	}

	/// An enum describing what parametrization will be used to describe
	/// a local coordinate system as a rotation from the global coordinate
	/// system.  The different options are:
	/// - Quaternion: 4 parameters (b0, b1, b2, b3)
	/// - AxisAngle: the axis (x, y, z) followed by the angle about the axis
	/// If these systems are unfamiliar, this link may be helpful:
	/// www.tu-berlin.de/fileadmin/fg169/miscellaneous/Quaternions.pdf
	enum class LCSParameterization : char
	{
		Quaternion,
		AxisAngle
		// Perhaps add EulerAngleXXX for the different options
	};

	/// Given a type of local coordinate system parameterization this
	/// function will return the number of paramerters needed to
	/// describe a coordinate system.
	static int get_num_parameters(const LCSParameterization& lcs_p)
	{
		if (lcs_p == LCSParameterization::Quaternion ||
			lcs_p == LCSParameterization::AxisAngle)
			return 4;
		// Euler angles would be 3
		else
			throw 0; // TODO throw a real error
	};

	std::string to_friendly_name(const LCSParameterization& lcs_p)
	{
		if (lcs_p == LCSParameterization::Quaternion) return "Quaternion";
		else if (lcs_p == LCSParameterization::AxisAngle) return "AxisAngle";
		else throw _MSG_EXCEPT("LCSParameterization " + std::to_string(static_cast<std::underlying_type<ElementType>::type>(lcs_p)) + " not recognized.");
	}

	LCSParameterization parameterizaiton_from_name(const std::string& name)
	{
		if (name == "Quaternion") return LCSParameterization::Quaternion;
		else if (name == "AxisAngle") return LCSParameterization::AxisAngle;
		else throw _MSG_EXCEPT("LCSParameterization " + name + " not recognized.");
	}
}