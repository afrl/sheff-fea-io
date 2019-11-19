#pragma once

#include <memory>
#include <string>
#include <vector>

#include "fea_io/common/Exceptions.h"
#include "fea_io/meta/MetaEnums.h"

#include <nlohmann/json.hpp>

namespace fea_io
{
    using json = nlohmann::json;

    struct MeshObect
    {
        MeshObjectType type;
        std::string mesh_path;
        std::vector<std::string> data_paths;
        std::vector<std::string> group_paths;
    };

    static void to_json(json& j, const MeshObect& i)
    {
        j = json{ {"type", to_string(i.type)},
                  {"mesh_path", i.mesh_path}
        };
        if (not i.data_paths.empty()) {
            j["data_paths"] = i.data_paths;
        }
        if (not i.group_paths.empty()) {
            j["group_paths"] = i.group_paths;
        }
    }

    static void from_json(const json& j, MeshObect& i)
    {
        i.type = mesh_object_type_from_name(j.at("type").get<std::string>());
        j.at("mesh_path").get_to(i.mesh_path);
        if (j.count("data_paths") > 0) {
            j.at("data_paths").get_to(i.data_paths);
        }
        if (j.count("group_paths") > 0) {
            j.at("group_paths").get_to(i.group_paths);
        }
    }

    struct LevelOfDetail
    {
        int level;
        LODParentRelation relationship_with_parent;
        std::vector<MeshObect> mesh_objects;
    };

    static void to_json(json& j, const LevelOfDetail& i)
    {
        j = json{ {"level", i.level},
                  {"mesh_objects", i.mesh_objects}
        };
        if (i.level != 0) {
            j["relationship_with_parent"] = to_string(i.relationship_with_parent);
        }
    }

    static void from_json(const json& j, LevelOfDetail& i)
    {
        j.at("level").get_to(i.level);
        j.at("mesh_objects").get_to(i.mesh_objects);
        if (i.level != 0) {
            i.relationship_with_parent = lod_parent_relation_from_name(
                j.at("relationship_with_parent").get<std::string>());
        }
        else
        {
            i.relationship_with_parent = LODParentRelation::None;
        }
    }

    struct InputInfo
    {
        std::string input_script_path;
        std::vector<LevelOfDetail> levels_of_detail;
    };

    static void to_json(json& j, const InputInfo& i)
    {
        j = json{ {"input_script_path", i.input_script_path},
                  {"levels_of_detail", i.levels_of_detail}
        };
    }

    static void from_json(const json& j, InputInfo& i)
    {
        j.at("input_script_path").get_to(i.input_script_path);
        j.at("levels_of_detail").get_to(i.levels_of_detail);
    }

    struct TimeStepInfo
    {
        double time;
        int64_t time_step_id;
        std::vector<LevelOfDetail> levels_of_detail;
    };

    static void to_json(json& j, const TimeStepInfo& i)
    {
        j = json{ {"time", i.time},
                  {"time_step_id", i.time_step_id},
                  {"levels_of_detail", i.levels_of_detail}
        };
    }

    static void from_json(const json& j, TimeStepInfo& i)
    {
        j.at("time").get_to(i.time);
        j.at("time_step_id").get_to(i.time_step_id);
        j.at("levels_of_detail").get_to(i.levels_of_detail);
    }

    struct AnalysisMetaInfo
    {
        std::string name;
        std::vector<std::string> levels_of_detail_names;
        InputInfo input_info;
        std::vector<TimeStepInfo> timesteps;
        std::vector<std::string> attributes;
    };

    static void to_json(json& j, const AnalysisMetaInfo& i)
    {
        j = json{ {"name", i.name},
                  {"levels_of_detail_names", i.levels_of_detail_names},
                  {"input_info", i.input_info},
                  {"attributes", i.attributes}
        };
        if (!i.timesteps.empty())
        {
            j["timesteps"] = i.timesteps;
        }
    }

    static void from_json(const json& j, AnalysisMetaInfo& i)
    {
        j.at("name").get_to(i.name);
        j.at("levels_of_detail_names").get_to(i.levels_of_detail_names);
        j.at("input_info").get_to(i.input_info);
        if (j.count("timesteps") > 0)
        {
            j.at("timesteps").get_to(i.timesteps);
        }
        j.at("attributes").get_to(i.attributes);
    }

}
