#pragma once

#include <memory>
#include <string>
#include <vector>

//#ifdef HAVE_FILESYSTEM
//#  include<experimental/filesystem>
//#else
//#  include "fea_io/common/Filesystem.h"
//#endif

#include <nlohmann/json.hpp>

namespace fea_io
{
    using json = nlohmann::json;
    //namespace fs = std::experimental::filesystem;

    struct IterationInfo
    {
        int iteration_number;
        std::string mesh_path;
        std::vector<std::string> data_paths;
        std::vector<std::string> group_paths;
    };

    static void to_json(json& j, const IterationInfo& i)
    {
        j = json{ {"iteration_number", i.iteration_number},
                  {"mesh_path", i.mesh_path}
                };
        if (not i.data_paths.empty()) {
            j["data_paths"] = i.data_paths;
        }
        if (not i.group_paths.empty()) {
            j["group_paths"] = i.group_paths;
        }
    }

    static void from_json(const json& j, IterationInfo& i)
    {
        j.at("iteration_number").get_to(i.iteration_number);
        j.at("mesh_path").get_to(i.mesh_path);
        if (j.count("data_paths") > 0) {
            j.at("data_paths").get_to(i.data_paths);
        }
        if (j.count("group_paths") > 0) {
            j.at("group_paths").get_to(i.group_paths);
        }
    }

    struct TimeStepInfo
    {
        double time;
        int64_t time_step_id;
        std::string mesh_path;
        std::vector<std::string> data_paths;
        std::vector<std::string> group_paths;
        std::vector<IterationInfo> iterations;
    };

    static void to_json(json& j, const TimeStepInfo& i)
    {
        j = json{ {"time", i.time},
                  {"time_step_id", i.time_step_id},
                  {"mesh_path", i.mesh_path}
        };
        if (not i.data_paths.empty())
            j["data_paths"] = i.data_paths;
        if (not i.group_paths.empty())
            j["group_paths"] = i.group_paths;
        if (not i.iterations.empty())
            j["iterations"] = i.iterations;
    }

    static void from_json(const json& j, TimeStepInfo& i)
    {
        j.at("time").get_to(i.time);
        j.at("time_step_id").get_to(i.time_step_id);
        j.at("mesh_path").get_to(i.mesh_path);
        if (j.count("data_paths") > 0)
            j.at("data_paths").get_to(i.data_paths);
        if (j.count("group_paths") > 0)
            j.at("group_paths").get_to(i.group_paths);
        if (j.count("iterations") > 0)
            j.at("iterations").get_to(i.iterations);
    }

    struct AnalysisMetaInfo
    {
        std::string name;
        std::vector<TimeStepInfo> timesteps;
        std::vector<std::string> attributes;
    };

    static void to_json(json& j, const AnalysisMetaInfo& i)
    {
        j = json{ {"name", i.name},
                  {"timesteps", i.timesteps},
                  {"attributes", i.attributes}
                };
    }

    static void from_json(const json& j, AnalysisMetaInfo& i)
    {
        j.at("name").get_to(i.name);
        j.at("timesteps").get_to(i.timesteps);
        j.at("attributes").get_to(i.attributes);
    }

}
