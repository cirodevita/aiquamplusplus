//
// Created by Ciro De Vita on 11/07/24.
//

#include "Area.hpp"

Area::Area(double j, double i) {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));

    _data.j = j;
    _data.i = i;
}

Area::Area(area_data data) {
    _data = data;
}

Area::~Area() = default;

area_data Area::data() {
    return _data;
}

void Area::data(area_data data) {
    _data = data;
}