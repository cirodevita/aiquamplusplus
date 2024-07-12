//
// Created by Ciro De Vita on 11/07/24.
//

#include "Areas.hpp"

Areas::Areas() {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));
}

void Areas::loadFromJson(const string &fileName, std::shared_ptr<WacommAdapter> wacommAdapter) {
    LOG4CPLUS_INFO(logger, "Reading from json:" << fileName);

    std::ifstream infile(fileName);

    try {
        json featureCollection;
        infile >> featureCollection;

        if (featureCollection.contains("features") && featureCollection["features"].is_array()) {
            for (auto feature:featureCollection["features"]) {
                double minLon, minLat, maxLon, maxLat;
                double minJ = 1e37, minI = 1e37, maxJ = 1e37, maxI = 1e37;

                if (feature.contains("bbox") && feature["bbox"].is_array() && feature["bbox"].size() == 4) {
                    minLon = feature["bbox"][0];
                    minLat = feature["bbox"][1];
                    maxLon = feature["bbox"][2];
                    maxLat = feature["bbox"][3];

                    LOG4CPLUS_INFO(logger, "Bounding box from bbox: [" << minLon << ", " << minLat << ", " << maxLon << ", " << maxLat << "]");
                } else if (feature.contains("geometry")) {
                    auto geometry = feature["geometry"];
                    if (geometry.contains("type") && geometry["type"] == "MultiPolygon") {
                        if (geometry.contains("coordinates") && geometry["coordinates"].is_array()) {
                            auto coordinates = geometry["coordinates"];
                            for (auto coordinate:coordinates) {
                                double minLon = std::numeric_limits<double>::max();
                                double maxLon = std::numeric_limits<double>::lowest();
                                double minLat = std::numeric_limits<double>::max();
                                double maxLat = std::numeric_limits<double>::lowest();

                                if (coordinate.is_array() && !coordinate.empty()) {
                                    auto points = coordinate.at(0);
                                    for (auto point:points) {
                                        if (point.is_array() && point.size() >= 2) {
                                            double lon = point.at(0);
                                            double lat = point.at(1);

                                            if (lon < minLon) minLon = lon;
                                            if (lon > maxLon) maxLon = lon;
                                            if (lat < minLat) minLat = lat;
                                            if (lat > maxLat) maxLat = lat;
                                        }
                                    }
                                }

                                LOG4CPLUS_INFO(logger, "Bounding box calculated: [" << minLon << ", " << minLat << ", " << maxLon << ", " << maxLat << "]");
                            }
                        }
                    }
                }

                wacommAdapter->latlon2ji(minLat, minLon, minJ, minI);
                LOG4CPLUS_INFO(logger, "i, j: " << minI << ", " << minJ);
                wacommAdapter->latlon2ji(maxLat, maxLon, maxJ, maxI);
                LOG4CPLUS_INFO(logger, "i, j: " << maxI << ", " << maxJ);
            }
        }

    } catch (const nlohmann::json::parse_error& e) {
        LOG4CPLUS_ERROR(logger,e.what());
    }
}

Areas::~Areas() = default;