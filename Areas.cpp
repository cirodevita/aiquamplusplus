//
// Created by Ciro De Vita on 11/07/24.
//

#include "Areas.hpp"

Areas::Areas() {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));
}

void Areas::loadFromJson(const string &fileName) {
    std::ifstream infile(fileName);
}

Areas::~Areas() = default;