//
// Created by Ciro De Vita on 29/01/24.
//

// log4cplus - https://github.com/log4cplus/log4cplus
#include "log4cplus/configurator.h"
#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"
#include "log4cplus/initializer.h"
#include "log4cplus/consoleappender.h"
#include "log4cplus/layout.h"

#include "Config.hpp"
#include "AiquammPlusPlus.hpp"

#include <iostream>

using namespace std;

log4cplus::Logger logger;

int main(int argc, char **argv) {
    std::string configFile = "aiquam.json";

    // Inizitalizer
    log4cplus::Initializer initializer;

    // Basic configuration
    log4cplus::BasicConfigurator basicConfigurator;
    basicConfigurator.configure();

    //Create an appender pointing to the console
    log4cplus::SharedAppenderPtr appender(new log4cplus::ConsoleAppender());

    //Set the layout of the appender
    appender->setName(LOG4CPLUS_TEXT("console"));
    log4cplus::tstring pattern = LOG4CPLUS_TEXT("%D{%y-%m-%d %H:%M:%S,%Q} %-5p %c");
    appender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));
    logger.addAppender(appender);

    LOG4CPLUS_INFO(logger, "Aiquam - C++ Version");

    if (argc==2) {
        configFile=string(argv[1]);
    }

    // Load the configuration file
    auto config = std::make_shared<Config>(configFile);

    AiquamPlusPlus aiquamPlusPlus(config);
    aiquamPlusPlus.run();
}