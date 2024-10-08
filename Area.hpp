//
// Created by Ciro De Vita on 11/07/24.
//

#ifndef WACOMMPLUSPLUS_AREA_HPP
#define WACOMMPLUSPLUS_AREA_HPP

// log4cplus - https://github.com/log4cplus/log4cplus
#include "log4cplus/configurator.h"
#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"

struct area_data {
    double i;
    double j;
    std::vector<float> values;
    int prediction;
};

class Area {
public:
    Area() = default;
    Area(double j, double i);
    Area(area_data);
    ~Area();

    area_data data();
    void data(area_data data);

    double J() const;
    double I() const;

    void addValue(float value);
    std::vector<float>& Values();

    int Prediction() const;
    void Prediction(int prediction);
private:
    log4cplus::Logger logger;
    area_data _data{};
};

#endif //WACOMMPLUSPLUS_AREA_HPP