#pragma once

#include <memory>
#include <string>
#include <vector>

#include "fea_io/common/Exceptions.h"

namespace fea_io
{
    enum class MeshObjectType
    {
        Volume,
        Surface
    };

    static std::string to_string(const MeshObjectType& t)
    {
        switch (t)
        {
        case MeshObjectType::Volume:
            return "Volume";
        case MeshObjectType::Surface:
            return "Surface";
        default:
            return "";
        }
    }

    static MeshObjectType mesh_object_type_from_name(const std::string& name)
    {
        if (name == "Volume") return MeshObjectType::Volume;
        else if (name == "Surface") return MeshObjectType::Surface;
        else throw _MSG_EXCEPT("MeshObjectType " + name + " not recognized.");
    }

    enum class LODParentRelation
    {
        ConformingOverlapping,
        None
    };

    static std::string to_string(const LODParentRelation& t)
    {
        switch (t)
        {
        case LODParentRelation::ConformingOverlapping:
            return "ConformingOverlapping";
        case LODParentRelation::None:
            return "None";
        default:
            return "";
        }
    }

    static LODParentRelation lod_parent_relation_from_name(const std::string& name)
    {
        if (name == "ConformingOverlapping") return LODParentRelation::ConformingOverlapping;
        else if (name == "None") return LODParentRelation::None;
        else throw _MSG_EXCEPT("MeshObjectType " + name + " not recognized.");
    }

}
