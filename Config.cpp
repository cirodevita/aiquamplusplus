//
// Created by Ciro De Vita on 29/01/24.
//

#include "Config.hpp"

using json = nlohmann::json;

Config::Config() {
    setDefault();
}

Config::Config(string &fileName): configFile(fileName) {
    setDefault();

    log4cplus::BasicConfigurator basicConfig;
    basicConfig.configure();
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));

    LOG4CPLUS_DEBUG(logger, "Reading config file:" + fileName);

    loadFromJson(fileName);
}

Config::~Config() {}

void Config::setDefault() {}

string &Config::ConfigFile() {
    return configFile;
}

vector<string> &Config::NcInputs() {
    return ncInputs;
}

vector<struct config_model> &Config::Models() {
    return models;
}

config_model *Config::dataptr() {
    return &_data;
}

void Config::loadFromJson(const string &fileName) {
    setDefault();
    json config;
    std::ifstream i(fileName);
    i >> config;

    if (config.contains("io")) {
        json io=config["io"];
        if (io.contains("base_path")) { ncBasePath = io["base_path"]; }
        if (io.contains("nc_inputs") && io["nc_inputs"].is_array()) {
            for (auto ncInput:io["nc_inputs"]) {
                string file=ncInput;
                this->ncInputs.push_back(ncBasePath+"/"+file);
            }
        }
    }

    if (config.contains("inference")) {
        json inference=config["inference"];
        if (inference.contains("base_path")) { modelsBasePath = inference["base_path"]; }
        if (inference.contains("models") && inference["models"].is_array()) {
            for (auto model:inference["models"]) {
                config_model m;
                if (model.contains("name")) {
                    m.name = modelsBasePath + "/" + model["name"].get<std::string>();
                }
                if (model.contains("input")) {
                    m.input = model["input"];
                }
                if (model.contains("output")) {
                    m.output = model["output"];
                }
                if (model.contains("input_shape") && model["input_shape"].is_array()) {
                    for (auto& dim : model["input_shape"]) {
                        m.input_shape.push_back(dim.get<int64_t>());
                    }
                }
                if (model.contains("output_type")) {
                    m.output_type = model["output_type"];
                }
                models.push_back(m);
            }
        }
    }
}