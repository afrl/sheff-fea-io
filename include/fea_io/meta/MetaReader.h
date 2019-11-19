#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "fea_io/common/Exceptions.h"
#include "fea_io/common/Filesystem.h"

#include "MetaStructs.h"

namespace fea_io
{
    using json = nlohmann::json;

    class MetaReader
    {
    public:
        MetaReader() {}

        void open(const std::string& meta_file_path);

        const AnalysisMetaInfo& get_analysis_meta() const;
        const TimeStepInfo& get_time_step(const double time) const;
        const TimeStepInfo& get_closest_time_step(const double time) const;

        /// Return the vector of times that has output in the meta file
        //std::vector<double> get_times() const;

    private:
        fs::path meta_file_dir_;
        std::string meta_file_name_;
        AnalysisMetaInfo analysis_meta_info_;
        std::map<const int64_t, int64_t> time_id_to_index_map;
        std::map<const double, int64_t> time_to_index_map;
        const std::string json_ext = ".x.json";
    };

    inline void MetaReader::open( const std::string & meta_file_path )
    {
        fs::path meta_path = fs::path(meta_file_path);
        if (!meta_path.has_extension()) {
            meta_path = fs::path(meta_file_path + json_ext);
        }

        meta_file_dir_ = meta_path.parent_path();
        meta_file_name_ = meta_path.filename().string();

        std::ifstream json_component( meta_path.string() );
        json j;
        json_component >> j;
        json_component.close();

        analysis_meta_info_ = j["analysis"];

        for (auto i = 0; i < analysis_meta_info_.timesteps.size(); i++) {
            time_id_to_index_map[analysis_meta_info_.timesteps[i].time_step_id] = i;
            time_to_index_map[analysis_meta_info_.timesteps[i].time] = i;
        }
    }

    inline const AnalysisMetaInfo& MetaReader::get_analysis_meta() const
    {
        return analysis_meta_info_;
    }

    inline const TimeStepInfo& MetaReader::get_time_step(const double time) const
    {
        auto find_iter = time_to_index_map.find(time);
        if (find_iter == time_to_index_map.end()) {
            throw _MSG_EXCEPT_C("Time " + std::to_string(time) + " did not exist for the analysis.");
        }
        return analysis_meta_info_.timesteps[find_iter->second];
    }

    inline const TimeStepInfo& MetaReader::get_closest_time_step(const double time) const
    {
        auto result_iter = std::min_element(
            time_to_index_map.begin(), time_to_index_map.end(),
            // lambda providing if distance(time, lhs) < distance(time, rhs)
            [&time](const std::pair<const double, int64_t>& lhs,
                const std::pair<const double, int64_t>& rhs) -> bool
            {
                return std::abs(time - lhs.first) < std::abs(time - rhs.first);
            });
        return analysis_meta_info_.timesteps[result_iter->second];
    }
}
