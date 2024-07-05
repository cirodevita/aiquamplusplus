//
// Created by Ciro De Vita on 29/01/24.
//

#include "AiquammPlusPlus.hpp"

AiquamPlusPlus::~AiquamPlusPlus() = default;

AiquamPlusPlus::AiquamPlusPlus(std::shared_ptr<Config> config) {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));
}

void AiquamPlusPlus::run() {
    LOG4CPLUS_INFO(logger, "Running ...");
}