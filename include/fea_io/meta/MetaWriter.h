#pragma once

#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "fea_io/common/Exceptions.h"
#include "fea_io/common/Filesystem.h"
#include "fea_io/meta/MetaStructs.h"

namespace fea_io
{
	using json = nlohmann::json;

	class MetaWriter
	{
	public:
		MetaWriter() {};
		~MetaWriter();

		void open(const std::string meta_file_path);
        AnalysisMetaInfo& get_analysis_meta();

        /// Returns information about a time step, allowing it to be populated.
        /// If a time step does not exists, one will be created, but an error will be thrown
        /// if the time step id and time only partially match an existing time step.
        /// The JSON file will be updated via the next call to update_meta_file().
        TimeStepInfo& get_time_step(const int64_t time_step_id, const double time);
		/// Adjusts a relative path to resolve the path to a form suitable to be stored
        /// in the meta file. TODO document this more.
		const std::string adjust_path(const std::string& relative_path) const;
        /// Updates the meta JSON file with the current information in the data structures.
		void update_meta_file() const;

	private:
		fs::path meta_file_dir_;
		std::string meta_file_name_;
		AnalysisMetaInfo analysis_meta_info_;
		std::map<const int64_t, int64_t> time_id_to_index_map;
		const std::string json_ext = ".x.json";
	};

	inline MetaWriter::~MetaWriter()
	{
		update_meta_file();
	}

	inline void MetaWriter::open(const std::string meta_file_path)
	{
		auto meta_path = fs::path(meta_file_path);
		if (!meta_path.has_extension())
			meta_path = fs::path(meta_file_path + json_ext);

		meta_file_dir_ = meta_path.parent_path();
		meta_file_name_ = meta_path.filename().string();
	}

	inline AnalysisMetaInfo& MetaWriter::get_analysis_meta()
	{
        return analysis_meta_info_;
	}

    inline TimeStepInfo& fea_io::MetaWriter::get_time_step(const int64_t time_step_id, const double time)
    {
        auto find_iter = time_id_to_index_map.find(time_step_id);
        if (find_iter == time_id_to_index_map.end()) {
            analysis_meta_info_.timesteps.push_back(TimeStepInfo());
            analysis_meta_info_.timesteps.back().time_step_id = time_step_id;
            analysis_meta_info_.timesteps.back().time = time;
            time_id_to_index_map[time_step_id] = analysis_meta_info_.timesteps.size() - 1;
            return analysis_meta_info_.timesteps.back();
        }

        if (analysis_meta_info_.timesteps[find_iter->second].time != time) {
            throw _MSG_EXCEPT_C("Time step " + std::to_string(time_step_id) + " and time "
                + std::to_string(time) + " only partially matched an existing time step.");
        }

        return analysis_meta_info_.timesteps[find_iter->second];
    }

    inline const std::string MetaWriter::adjust_path(const std::string& relative_path) const
    {
        return (meta_file_dir_ / relative_path).string();
    }

	inline void MetaWriter::update_meta_file() const
	{
		json j;
		j["analysis"] = analysis_meta_info_;

		std::ofstream json_component((meta_file_dir_ / meta_file_name_).string());
		json_component << std::setw(4) << j << std::endl;
		json_component.close();
	}

}
