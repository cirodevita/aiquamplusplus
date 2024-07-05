//
// Created by Ciro De Vita on 29/01/24.
//

#ifndef AIQUAMPLUSPLUS_AIQUAMMPLUSPLUS_HPP
#define AIQUAMPLUSPLUS_AIQUAMMPLUSPLUS_HPP

// log4cplus - https://github.com/log4cplus/log4cplus
#include "log4cplus/configurator.h"
#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"

#include "Config.hpp"

#include <string>

using namespace std;

class AiquamPlusPlus {
public:
    AiquamPlusPlus(std::shared_ptr<Config> config);
    ~AiquamPlusPlus();

    void run();

private:
    log4cplus::Logger logger;

    std::shared_ptr<Config> config;
};

#endif //AIQUAMPLUSPLUS_AIQUAMMPLUSPLUS_HPP
