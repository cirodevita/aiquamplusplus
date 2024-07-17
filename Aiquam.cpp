//
// Created by Ciro De Vita on 09/07/24.
//

#include "Aiquam.hpp"

Aiquam::~Aiquam() = default;

Aiquam::Aiquam(std::shared_ptr<Config> config, int gpu_id = -1): config(config), gpu_id(gpu_id) {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));

    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

    if (gpu_id >= 0) {
      Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_CUDA(
          session_options, gpu_id));
    }

    for (const auto& model : config->Models()) {
        const char* model_path_cstr = model.name.c_str();
        sessions.emplace_back(env, model_path_cstr, session_options);
    }
}

int Aiquam::majority_vote() {
    std::unordered_map<int, int> frequencyMap;

    for (int num : predictions) {
        frequencyMap[num]++;
    }

    int mostFrequent = predictions[0];
    int maxCount = 0;

    for (const auto& pair : frequencyMap) {
        if (pair.second > maxCount) {
            maxCount = pair.second;
            mostFrequent = pair.first;
        }
    }

    return mostFrequent;
}

template <typename T>
void Aiquam::softmax(T& input) {
    float rowmax = *std::max_element(input.begin(), input.end());
    std::vector<float> y(input.size());
    float sum = 0.0f;
    for (size_t i = 0; i != input.size(); ++i) {
        sum += y[i] = std::exp(input[i] - rowmax);
    }
    for (size_t i = 0; i != input.size(); ++i) {
        input[i] = y[i] / sum;
    }
}

template <typename T>
void Aiquam::processOutputTensor(Ort::Session& session, std::vector<float> input_data, config_model model) {
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    const char* input_name = model.input.c_str();
    std::vector<int64_t> input_shape = model.input_shape;
    const char* output_name = model.output.c_str();
    const char* output_type = model.output_type.c_str();
    std::vector<int64_t> output_shape = model.output_shape;

    std::vector<T> results(output_shape.back());
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(memory_info, input_data.data(), input_data.size(), input_shape.data(), input_shape.size());
    Ort::Value output_tensors = Ort::Value::CreateTensor<T>(memory_info, results.data(), results.size(), output_shape.data(), output_shape.size());

    session.Run(Ort::RunOptions{nullptr}, &input_name, &input_tensor, 1, &output_name, &output_tensors, 1);

    int64_t predicted_class;
    if (results.size() > 1) {
        softmax(results);
        predicted_class = std::distance(results.begin(), std::max_element(results.begin(), results.end()));
    } else {
        predicted_class = results[0];
    }

    predictions.push_back(predicted_class);
}

void Aiquam::runInference(Ort::Session& session, std::vector<float> input_data, config_model model) {
    if (model.output_type == "float") {
        processOutputTensor<float>(session, input_data, model);
    } else if (model.output_type == "int64_t") {
        processOutputTensor<int64_t>(session, input_data, model);
    } else {
        throw std::runtime_error("Unsupported output type");
    }
}

int Aiquam::inference(std::vector<float> input_data) {
    predictions.clear();
    size_t model_index = 0;
    for (auto& model : config->Models()) {
        LOG4CPLUS_DEBUG(logger, "Running inference with model: " + model.name);

        Ort::Session& session = sessions[model_index++];
        runInference(session, input_data, model);

        LOG4CPLUS_DEBUG(logger, model.name << ": local predicted class: " << predictions.back());
    }

    int predicted_class = majority_vote();
    return predicted_class;
}