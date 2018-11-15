#pragma once

#include <memory>
#include <string>
#include <vector>
#include <experimental/filesystem>

#include <nlohmann/json.hpp>

#include "fea_io/common/Exceptions.h"
#include "fea_io/mesh/MeshReader.h"
#include "fea_io/data/DataReader.h"
#include "fea_io/group/GroupReader.h"

namespace fea_io
{
	using json = nlohmann::json;
	namespace fs = std::experimental::filesystem;

	struct IterationInfo
	{
		int iteration_number;
		std::string mesh_path;
		std::vector<std::string> data_paths;
		std::vector<std::string> group_paths;
	};

	void to_json(json& j, const IterationInfo& i)
	{
		j = json{ {"iteration_number", i.iteration_number},
			{"mesh_path", i.mesh_path}
		};
		if (not i.data_paths.empty())
			j["data_paths"] = i.data_paths;
		if (not i.group_paths.empty())
			j["group_paths"] = i.group_paths;
	}

	void from_json(const json& j, IterationInfo& i)
	{
		j.at("iteration_number").get_to(i.iteration_number);
		j.at("mesh_path").get_to(i.mesh_path);
		if (j.count("data_paths") > 0)
			j.at("data_paths").get_to(i.data_paths);
		if (j.count("group_paths") > 0)
			j.at("group_paths").get_to(i.group_paths);
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

	void to_json(json& j, const TimeStepInfo& i)
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

	void from_json(const json& j, TimeStepInfo& i)
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

	void to_json(json& j, const AnalysisMetaInfo& i)
	{
		j = json{ {"name", i.name},
			{"timesteps", i.timesteps},
			{"attributes", i.attributes}
		};
	}

	void from_json(const json& j, AnalysisMetaInfo& i)
	{
		j.at("name").get_to(i.name);
		j.at("timesteps").get_to(i.timesteps);
		j.at("attributes").get_to(i.attributes);
	}

	class MetaReader
	{
	public:
		MetaReader() {};

		void open(std::string meta_file_path);

		std::string get_mesh_path(const int64_t time_step_id);
		std::string get_mesh_path_at_t(const double time);
		std::string get_iteration_mesh_path(const int64_t time_step_id, const int64_t iteration_number);

		std::vector<std::string> get_data_paths(const int64_t time_step_id);
		std::vector<std::string> get_data_paths_at_t(const double time);
		std::vector<std::string> get_iteration_data_paths(const int64_t time_step_id, const int64_t iteration_number);

		std::vector<std::string> get_group_paths(const int64_t time_step_id);
		std::vector<std::string> get_iteration_group_paths(const int64_t time_step_id, const int64_t iteration_number);

		int64_t get_time_step_id_at_t(const double time);
		int64_t get_closest_time_step_id_to_t(const double time);

		/// Return the vector of times that has output in the meta file
		std::vector<double> get_times() const;

	private:
		fs::path meta_file_dir_;
		std::string meta_file_name_;
		AnalysisMetaInfo analysis_meta_info_;
		std::map<const int64_t, int64_t> time_id_to_index_map;
		std::map<const double, int64_t> time_to_index_map;
		const std::string json_ext = ".x.json";

		TimeStepInfo& get_time_step(const int64_t time_step_id);
		TimeStepInfo& get_time_step_at_t(const double time);
		const IterationInfo& get_iteration(const TimeStepInfo& time_step, const int64_t iteration_number);
	};

	inline void MetaReader::open(std::string meta_file_path)
	{
		auto meta_path = fs::path(meta_file_path);
		if (!meta_path.has_extension())
			meta_path = fs::path(meta_file_path + json_ext);

		meta_file_dir_ = meta_path.parent_path();
		meta_file_name_ = meta_path.filename().string();

		std::ifstream json_component(meta_path);
		json j;
		json_component >> j;
		json_component.close();

		analysis_meta_info_ = j["analysis"];

		for (auto i = 0; i < analysis_meta_info_.timesteps.size(); i++) {
			time_id_to_index_map[analysis_meta_info_.timesteps[i].time_step_id] = i;
			time_to_index_map[analysis_meta_info_.timesteps[i].time] = i;
		}
	}

	inline std::string MetaReader::get_mesh_path(const int64_t time_step_id)
	{
		return (meta_file_dir_ / analysis_meta_info_.timesteps[time_step_id].mesh_path).string();
	}

	inline std::string MetaReader::get_mesh_path_at_t(const double time)
	{
		return get_mesh_path(get_time_step_id_at_t(time));
	}

	inline std::string MetaReader::get_iteration_mesh_path(const int64_t time_step_id, const int64_t iteration_number)
	{
		auto& time_step = get_time_step(time_step_id);
		auto& iteration = get_iteration(time_step, iteration_number);

		return (meta_file_dir_ / iteration.mesh_path).string();
	}
	
	inline std::vector<std::string> MetaReader::get_data_paths(const int64_t time_step_id)
	{
		auto& time_step = get_time_step(time_step_id);
		std::vector<std::string> data_abs_paths = time_step.data_paths;
		for (auto& path : data_abs_paths)
			path = (meta_file_dir_ / path).string();

		return data_abs_paths;
	}

	inline std::vector<std::string> MetaReader::get_data_paths_at_t(const double time)
	{
		return get_data_paths(get_time_step_id_at_t(time));
	}
	
	inline std::vector<std::string> MetaReader::get_iteration_data_paths(const int64_t time_step_id, const int64_t iteration_number)
	{
		auto& time_step = get_time_step(time_step_id);
		auto& iteration = get_iteration(time_step, iteration_number);

		// Convert from relative to absolute paths
		std::vector<std::string> data_abs_paths = iteration.data_paths;
		for (auto& path : data_abs_paths)
			path = (meta_file_dir_ / path).string();

		return data_abs_paths;
	}
	
	inline std::vector<std::string> MetaReader::get_group_paths(const int64_t time_step_id)
	{
		auto& time_step = get_time_step(time_step_id);

		std::vector<std::string> group_abs_paths = time_step.group_paths;
		for (auto& path : group_abs_paths)
			path = (meta_file_dir_ / path).string();

		return group_abs_paths;
	}
	
	inline std::vector<std::string> MetaReader::get_iteration_group_paths(const int64_t time_step_id, const int64_t iteration_number)
	{
		auto& time_step = get_time_step(time_step_id);
		auto& iteration = get_iteration(time_step, iteration_number);

		// Convert from relative to absolute paths
		std::vector<std::string> group_abs_paths = iteration.group_paths;
		for (auto& path : group_abs_paths)
			path = (meta_file_dir_ / path).string();

		return group_abs_paths;
	}
	
	inline int64_t MetaReader::get_time_step_id_at_t(const double time)
	{
		auto& ts = get_time_step_at_t(time);
		return ts.time_step_id;
	}

	inline std::vector<double> MetaReader::get_times() const
	{
		std::vector<double> times;
		for (auto& ts : analysis_meta_info_.timesteps)
			times.push_back(ts.time);
		std::sort(times.begin(), times.end());
		return times;
	}

	inline TimeStepInfo & MetaReader::get_time_step(const int64_t time_step_id)
	{
		auto find_iter = time_id_to_index_map.find(time_step_id);
		if (find_iter == time_id_to_index_map.end())
			throw _MSG_EXCEPT_C("Time step " + std::to_string(time_step_id) + " did not exist for the analysis.");
		return analysis_meta_info_.timesteps[find_iter->second];
	}

	inline TimeStepInfo & MetaReader::get_time_step_at_t(const double time)
	{
		auto find_iter = time_to_index_map.find(time);
		if (find_iter == time_to_index_map.end())
			throw _MSG_EXCEPT_C("Time " + std::to_string(time) + " did not exist for the analysis.");
		return analysis_meta_info_.timesteps[find_iter->second];
	}

	inline const IterationInfo & MetaReader::get_iteration(const TimeStepInfo & time_step, const int64_t iteration_number)
	{
		auto& iterations = time_step.iterations;
		auto find_iter = std::find_if(iterations.begin(), iterations.end(), [&iteration_number](const IterationInfo& iteration) -> bool {
			return iteration.iteration_number == iteration_number;
		});
		if (find_iter == iterations.end())
			throw _MSG_EXCEPT_C("iteration " + std::to_string(iteration_number) + " did not exist for time step " + std::to_string(time_step.time_step_id));
		return (*find_iter);
	}

}