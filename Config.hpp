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
    std::vector<int64_t> output_shape;
};

class Config {
public:
    Config();
    explicit Config(string &fileName);
    ~Config();

    config_model *dataptr();

    string &ConfigFile();

    string Name() const;
    void Name(string value);
    string Date() const;
    void Date(string value);

    vector<string> &NcInputs();
    string NcOutputRoot() const;
    void NcOutputRoot(string value);

    vector<struct config_model> &Models();

    string AreasFile() const;
    void AreasFile(string value);

    void loadFromJson(const string &fileName);

private:
    log4cplus::Logger logger;

    string configFile;

    string name;
    string date;

    string ncBasePath;
    vector<string> ncInputs;
    string ncOutputRoot;

    string modelsBasePath;
    vector<struct config_model> models;

    string areasFile;

    config_model _data;

    void setDefault();
};

#endif //AIQUAMPLUSPLUS_CONFIG_HPP