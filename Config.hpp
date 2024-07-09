//
// Created by Ciro De Vita on 29/01/24.
//

#ifndef AIQUAMPLUSPLUS_CONFIG_HPP
#define AIQUAMPLUSPLUS_CONFIG_HPP

#include <string>
#include <fstream>

using namespace std;

// log4cplus - https://github.com/log4cplus/log4cplus
#include "log4cplus/configurator.h"
#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"

#include <nlohmann/json.hpp>

struct config_model {
    std::string name;
    std::string input;
    std::string output;
    std::string output_type;
    std::vector<int64_t> input_shape;
};

class Config {
public:
    Config();
    explicit Config(string &fileName);
    ~Config();

    config_model *dataptr();

    string &ConfigFile();
    vector<string> &NcInputs();
    vector<struct config_model> &Models();

    void loadFromJson(const string &fileName);

private:
    log4cplus::Logger logger;

    std::map<std::string, int> dictionary;
    string configFile;

    string ncBasePath;
    vector<string> ncInputs;

    string modelsBasePath;
    vector<struct config_model> models;

    config_model _data;

    void setDefault();
};

#endif //AIQUAMPLUSPLUS_CONFIG_HPP