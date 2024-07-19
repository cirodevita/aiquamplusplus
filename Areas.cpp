//
// Created by Ciro De Vita on 11/07/24.
//

#include "Areas.hpp"

Areas::Areas() {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));
}

bool Areas::isPointInPolygon(const Area& p, const vector<Area>& polygon) {
    bool inside = false;
    size_t n = polygon.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if (((polygon[i].I() > p.I()) != (polygon[j].I() > p.I())) &&
            (p.J() < (polygon[j].J() - polygon[i].J()) * (p.I() - polygon[i].I()) / (polygon[j].I() - polygon[i].I()) + polygon[i].J())) {
            inside = !inside;
        }
    }
    return inside;
}

void Areas::loadFromJson(const string &fileName, std::shared_ptr<WacommAdapter> wacommAdapter) {
    LOG4CPLUS_INFO(logger, "Reading from json:" << fileName);

    std::ifstream infile(fileName);

    Array::Array2 mask = wacommAdapter->Mask();

    try {
        json featureCollection;
        infile >> featureCollection;

        if (featureCollection.contains("features") && featureCollection["features"].is_array()) {
            for (auto feature:featureCollection["features"]) {
                double minLon, minLat, maxLon, maxLat;
                double minJ = 1e37, minI = 1e37, maxJ = 1e37, maxI = 1e37;
                vector<Area> polygon;
                bool bboxAvailable = false;

                if (feature.contains("bbox") && feature["bbox"].is_array() && feature["bbox"].size() == 4) {
                    bboxAvailable = true;

                    minLon = feature["bbox"][0];
                    minLat = feature["bbox"][1];
                    maxLon = feature["bbox"][2];
                    maxLat = feature["bbox"][3];

                    wacommAdapter->latlon2ji(minLat, minLon, minJ, minI);
                    wacommAdapter->latlon2ji(maxLat, maxLon, maxJ, maxI);

                    LOG4CPLUS_DEBUG(logger, "Bounding box from bbox: [" << minLon << ", " << minLat << ", " << maxLon << ", " << maxLat << "]");
                }
                
                if (feature.contains("geometry")) {
                    auto geometry = feature["geometry"];
                    if (geometry.contains("type") && geometry["type"] == "MultiPolygon") {
                        if (geometry.contains("coordinates") && geometry["coordinates"].is_array()) {
                            auto coordinates = geometry["coordinates"];
                            for (auto coordinate:coordinates) {
                                if (coordinate.is_array() && !coordinate.empty()) {
                                    auto points = coordinate.at(0);
                                    for (auto point:points) {
                                        if (point.is_array() && point.size() >= 2) {
                                            double lon = point.at(0);
                                            double lat = point.at(1);

                                            double j, i;
                                            wacommAdapter->latlon2ji(lat, lon, j, i);
                                            polygon.push_back({j, i});
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                if (polygon.empty()) {
                    continue;
                }

                if (!bboxAvailable) {
                    minI = numeric_limits<double>::max();
                    minJ = numeric_limits<double>::max();
                    maxI = numeric_limits<double>::lowest();
                    maxJ = numeric_limits<double>::lowest();

                    for (const auto& point : polygon) {
                        if (point.J() < minI) minI = point.J();
                        if (point.J() > maxI) maxI = point.J();
                        if (point.I() < minJ) minJ = point.I();
                        if (point.I() > maxJ) maxJ = point.I();
                    }

                    LOG4CPLUS_DEBUG(logger, "Bounding box calculated: [" << minI << ", " << minJ << ", " << maxI << ", " << maxJ << "]");
                }

                for (int j = int(minJ); j <= int(maxJ); j++) {
                    for (int i = int(minI); i <= int(maxI); i++) {
                        if (mask(j, i) == 1) {
                            if (isPointInPolygon({static_cast<double>(j), static_cast<double>(i)}, polygon)) {
                                this->push_back(Area(j, i));
                            }
                        }
                    }
                }
            }
        }

    } catch (const nlohmann::json::parse_error& e) {
        LOG4CPLUS_ERROR(logger,e.what());
    }
}

Areas::~Areas() = default;