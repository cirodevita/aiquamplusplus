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

double Area::J() const {
    return _data.j;
}

double Area::I() const {
    return _data.i;
}

void Area::addTimeSeriesValue(float value) {
    _data.timeseries.push_back(value);
}

std::vector<float>& Area::getTimeSeries() {
    return _data.timeseries;
}