//
// Created by Ciro De Vita on 09/07/24.
//

#ifndef AIQUAMPLUSPLUS_AIQUAMM_HPP
#define AIQUAMPLUSPLUS_AIQUAMM_HPP

// log4cplus - https://github.com/log4cplus/log4cplus
#include "log4cplus/configurator.h"
#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"
#include "onnxruntime_cxx_api.h"

#include "Config.hpp"

class Aiquam {
public:
    Aiquam(std::shared_ptr<Config>, int);
    ~Aiquam();

    int inference(std::vector<float>);

private:
    log4cplus::Logger logger;
    std::shared_ptr<Config> config;
    int gpu_id;

    Ort::Env env;
    Ort::SessionOptions session_options;
    std::vector<Ort::Session> sessions;

    std::vector<int64_t> predictions;

    int majority_vote();
    template <typename T> void softmax(T& input);
    template <typename T> void processOutputTensor(Ort::Session&, std::vector<float>, config_model);
    void runInference(Ort::Session&, std::vector<float>, config_model);
};

#endif //AIQUAMPLUSPLUS_AIQUAMM_HPP