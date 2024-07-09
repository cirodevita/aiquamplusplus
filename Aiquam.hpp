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
    Aiquam(std::shared_ptr<Config>);
    ~Aiquam();

    int inference(std::vector<float>);

private:
    log4cplus::Logger logger;
    std::shared_ptr<Config> config;

    Ort::Env env;
    Ort::SessionOptions session_options;

    std::vector<int64_t> predictions;

    int majority_vote();
    template <typename T> void processOutputTensor(Ort::Value&, vector<int64_t>&, size_t);
    void getClass(Ort::Value&, const string&, vector<int64_t>&, size_t);
};

#endif //AIQUAMPLUSPLUS_AIQUAMM_HPP