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

config_model *Config::dataptr() {
    return &_data;
}

string Config::Name() const {
    return name;
}

void Config::Name(string value) {
    name=value;
}

string Config::Date() const {
    return date;
}

void Config::Date(string value) {
    date=value;
}

vector<string> &Config::NcInputs() {
    return ncInputs;
}

string Config::NcOutputRoot() const {
    return ncOutputRoot;
}

void Config::NcOutputRoot(string value) {
    ncOutputRoot=value;
}

vector<struct config_model> &Config::Models() {
    return models;
}

string Config::AreasFile() const {
    return areasFile;
}

void Config::AreasFile(string value) {
    areasFile=value;
}

void Config::loadFromJson(const string &fileName) {
    setDefault();
    json config;
    std::ifstream i(fileName);
    i >> config;

    if (config.contains("prediction")) {
        json prediction=config["prediction"];
        if (prediction.contains("name")) { name = prediction["name"]; }
        if (prediction.contains("date")) { date = prediction["date"]; }
    }

    if (config.contains("areas")) {
        json areas=config["areas"];
        if (areas.contains("areas_file")) { areasFile = areas["areas_file"]; }
    }

    if (config.contains("io")) {
        json io=config["io"];
        if (io.contains("base_path")) { ncBasePath = io["base_path"]; }
        if (io.contains("nc_inputs") && io["nc_inputs"].is_array()) {
            for (auto ncInput:io["nc_inputs"]) {
                string file=ncInput;
                this->ncInputs.push_back(ncBasePath+"/"+file);
            }
        }
        if (io.contains("nc_output_root")) { ncOutputRoot = io["nc_output_root"]; }
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
                if (model.contains("output_shape") && model["output_shape"].is_array()) {
                    for (auto& dim : model["output_shape"]) {
                        m.output_shape.push_back(dim.get<int64_t>());
                    }
                }
                models.push_back(m);
            }
        }
    }
}