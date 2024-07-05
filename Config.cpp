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

    LOG4CPLUS_INFO(logger, "Reading config file:" + fileName);

    loadFromJson(fileName);
}

Config::~Config() {}

void Config::setDefault() {
    url = "http://193.205.230.6:8080/opendap/wcm3/d03/archive/";
}

string &Config::ConfigFile() {
    return configFile;
}

string Config::Url() {
    return url;
}

void Config::loadFromJson(const string &fileName) {
    setDefault();
    json config;
    std::ifstream i(fileName);
    i >> config;

    if (config.contains("url")) {
        url = config["url"];
    }
}