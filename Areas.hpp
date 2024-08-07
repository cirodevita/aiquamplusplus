//
// Created by Ciro De Vita on 11/07/24.
//

#ifndef AIQUAMPLUSPLUS_AREAS_HPP
#define AIQUAMPLUSPLUS_AREAS_HPP

// log4cplus - https://github.com/log4cplus/log4cplus
#include "log4cplus/configurator.h"
#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"
#include <nlohmann/json.hpp>
#include <fstream>

#include "shapefil.h"

#include "Area.hpp"
#include "WacommAdapter.hpp"

using namespace std;
using json = nlohmann::json;

class Areas : private vector<Area> {
public:
    Areas();
    ~Areas();

    using vector::push_back;
    using vector::operator[];
    using vector::size;
    using vector::at;
    using vector::empty;
    using vector::begin;
    using vector::end;
    using vector::erase;

    void loadFromJson(const string &fileName, std::shared_ptr<WacommAdapter> wacommAdapter);
    void loadFromShp(const string &fileName, std::shared_ptr<WacommAdapter> wacommAdapter);

private:
    log4cplus::Logger logger;

    bool isPointInPolygon(const area_data& p, const vector<area_data>& polygon);
    void calculateBoundingBox(const vector<area_data>& polygon, double& minI, double& minJ, double& maxI, double& maxJ);
};

#endif //AIQUAMPLUSPLUS_AREAS_HPP