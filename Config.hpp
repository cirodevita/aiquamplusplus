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

class Config {
public:
    Config();
    explicit Config(string &fileName);
    ~Config();

    string &ConfigFile();

    void Url(string url);
    string Url();

    void loadFromJson(const string &fileName);

private:
    log4cplus::Logger logger;

    std::map<std::string, int> dictionary;
    string configFile;

    string url;

    void setDefault();
};

#endif //AIQUAMPLUSPLUS_CONFIG_HPP