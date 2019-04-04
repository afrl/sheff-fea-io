#pragma once

#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>
#include <experimental/filesystem>

#include <nlohmann/json.hpp>

#include "fea_io/common/Exceptions.h"

#include "MetaStructs.h"

namespace fea_io
{
	using json = nlohmann::json;
	namespace fs = std::experimental::filesystem;

	class MetaWriter
	{
	public:
		MetaWriter() {};
		~MetaWriter();

		void open(std::string meta_file_path, std::string name);
		void add_attribute(std::string attribute);

		/// Adds a mesh path for the indicated time step and returns the full path
		/// If the time step does not exist, one is created.
		std::string add_mesh_path(const int64_t time_step_id, const double time, const std::string relative_path);
		/// Adds a mesh path for the indicated time step and returns the full path
		/// If the time step does not exist, one is created, and if the iteration does not exist, one is created.
		std::string add_mesh_path(const int64_t time_step_id, const double time, const int iteration_number, const std::string relative_path);

		std::string add_data_path(const int64_t time_step_id, const double time, const std::string relative_path);
		std::string add_data_path(const int64_t time_step_id, const double time, const int iteration_number, const std::string relative_path);

		std::string add_group_path(const int64_t time_step_id, const double time, const std::string relative_path);
		std::string add_group_path(const int64_t time_step_id, const double time, const int iteration_number, const std::string relative_path);

		void update_meta_file() const;

	private:
		fs::path meta_file_dir_;
		std::string meta_file_name_;
		AnalysisMetaInfo analysis_meta_info_;
		std::map<const int64_t, int64_t> time_id_to_index_map;
		const std::string json_ext = ".x.json";

		TimeStepInfo* get_time_step(const int64_t time_step_id, const double time);
		IterationInfo* get_iteration(TimeStepInfo& time_step, const int iteration_number);
	};

	inline MetaWriter::~MetaWriter()
	{
		update_meta_file();
	}

	inline void MetaWriter::open(std::string meta_file_path, std::string name)
	{
		auto meta_path = fs::path(meta_file_path);
		if (!meta_path.has_extension())
			meta_path = fs::path(meta_file_path + json_ext);

		meta_file_dir_ = meta_path.parent_path();
		meta_file_name_ = meta_path.filename().string();

		analysis_meta_info_.name = name;
	}

	inline void MetaWriter::add_attribute(std::string attribute)
	{
		analysis_meta_info_.attributes.push_back(attribute);
	}

	inline std::string fea_io::MetaWriter::add_mesh_path(const int64_t time_step_id, const double time, const std::string relative_path)
	{
		auto time_step = get_time_step(time_step_id, time);

		if (time_step->mesh_path != "")
			throw _MSG_EXCEPT_C("Time step " + std::to_string(time_step_id) + " already had a mesh path.");

		time_step->mesh_path = relative_path;

		return (meta_file_dir_ / relative_path).string();
	}

	inline std::string MetaWriter::add_mesh_path(const int64_t time_step_id, const double time, const int iteration_number, const std::string relative_path)
	{
		auto time_step = get_time_step(time_step_id, time);
		auto iteration = get_iteration(*time_step, iteration_number);

		if (iteration->mesh_path != "")
			throw _MSG_EXCEPT_C("Iteration " + std::to_string(iteration_number) + " in time step " + std::to_string(time_step_id) + " already had a mesh path.");

		iteration->mesh_path = relative_path;

		return (meta_file_dir_ / relative_path).string();
	}

	inline std::string MetaWriter::add_data_path(const int64_t time_step_id, const double time, const std::string relative_path)
	{
		auto time_step = get_time_step(time_step_id, time);

		time_step->data_paths.push_back(relative_path);

		return (meta_file_dir_ / relative_path).string();
	}

	inline std::string MetaWriter::add_data_path(const int64_t time_step_id, const double time, const int iteration_number, const std::string relative_path)
	{
		auto time_step = get_time_step(time_step_id, time);
		auto iteration = get_iteration(*time_step, iteration_number);

		iteration->data_paths.push_back(relative_path);

		return (meta_file_dir_ / relative_path).string();
	}

	inline std::string MetaWriter::add_group_path(const int64_t time_step_id, const double time, const std::string relative_path)
	{
		auto time_step = get_time_step(time_step_id, time);

		time_step->group_paths.push_back(relative_path);

		return (meta_file_dir_ / relative_path).string();
	}

	inline std::string MetaWriter::add_group_path(const int64_t time_step_id, const double time, const int iteration_number, const std::string relative_path)
	{
		auto time_step = get_time_step(time_step_id, time);
		auto iteration = get_iteration(*time_step, iteration_number);

		iteration->group_paths.push_back(relative_path);

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

	inline TimeStepInfo * fea_io::MetaWriter::get_time_step(const int64_t time_step_id, const double time)
	{
		auto find_iter = time_id_to_index_map.find(time_step_id);
		if (find_iter == time_id_to_index_map.end()) {
			analysis_meta_info_.timesteps.push_back(TimeStepInfo());
			analysis_meta_info_.timesteps.back().time_step_id = time_step_id;
			analysis_meta_info_.timesteps.back().time = time;
			time_id_to_index_map[time_step_id] = analysis_meta_info_.timesteps.size() - 1;
			return &analysis_meta_info_.timesteps.back();
		}
		return &analysis_meta_info_.timesteps[find_iter->second];
	}

	inline IterationInfo * MetaWriter::get_iteration(TimeStepInfo & time_step, const int iteration_number)
	{
		auto& iterations = time_step.iterations;
		auto find_iter = std::find_if(iterations.begin(), iterations.end(), [&iteration_number](const IterationInfo& iteration) -> bool {
			return iteration.iteration_number == iteration_number;
		});
		if (find_iter == iterations.end()) {
			time_step.iterations.push_back(IterationInfo());
			time_step.iterations.back().iteration_number = iteration_number;
			return &time_step.iterations.back();
		}
		return &(*find_iter);
	}

}