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
}