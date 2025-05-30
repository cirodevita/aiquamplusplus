//
// Created by Ciro De Vita on 11/07/24.
//

#include "Areas.hpp"

Areas::Areas() {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));
}

bool Areas::isPointInPolygon(const area_data& p, const vector<area_data>& polygon) {
    bool inside = false;
    size_t n = polygon.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if (((polygon[i].j > p.j) != (polygon[j].j > p.j)) &&
            (p.i < (polygon[j].i - polygon[i].i) * (p.j - polygon[i].j) / (polygon[j].j - polygon[i].j) + polygon[i].i)) {
            inside = !inside;
        }
    }
    return inside;
}

void Areas::calculateBoundingBox(const vector<area_data>& polygon,  double& minI, double& minJ, double& maxI, double& maxJ) {
    minI = numeric_limits<double>::max();
    minJ = numeric_limits<double>::max();
    maxI = numeric_limits<double>::lowest();
    maxJ = numeric_limits<double>::lowest();

    for (const auto& point : polygon) {
        if (point.i < minI) minI = point.i;
        if (point.i > maxI) maxI = point.i;
        if (point.j < minJ) minJ = point.j;
        if (point.j > maxJ) maxJ = point.j;
    }
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
                vector<area_data> polygon;
                bool bboxAvailable = false;

                if (feature.contains("bbox") && feature["bbox"].is_array() && feature["bbox"].size() == 4) {
                    bboxAvailable = true;

                    minLon = feature["bbox"][0];
                    minLat = feature["bbox"][1];
                    maxLon = feature["bbox"][2];
                    maxLat = feature["bbox"][3];

                    wacommAdapter->latlon2ji(minLat, minLon, minJ, minI);
                    wacommAdapter->latlon2ji(maxLat, maxLon, maxJ, maxI);

                    LOG4CPLUS_INFO(logger, "Bounding box: [" << minI << ", " << minJ << ", " << maxI << ", " << maxJ << "]");
                }
                
                if (feature.contains("geometry")) {
                    auto geometry = feature["geometry"];
                    if (geometry.contains("type") && geometry.contains("coordinates") && geometry["coordinates"].is_array()) {
                        auto coordinates = geometry["coordinates"];
                        
                        std::vector<std::vector<std::vector<double>>> polygons;

                        if (geometry["type"] == "MultiPolygon") {
                            for (auto coordinate : coordinates) {
                                if (coordinate.is_array() && !coordinate.empty()) {
                                    polygons.push_back(coordinate.at(0).get<std::vector<std::vector<double>>>());
                                }
                            }
                        } else if (geometry["type"] == "Polygon") {
                            polygons = coordinates.get<std::vector<std::vector<std::vector<double>>>>();
                        }

                        for (auto points : polygons) {
                            for (auto point : points) {
                                if (point.size() >= 2) {
                                    double lon = point[0];
                                    double lat = point[1];

                                    double j, i;
                                    wacommAdapter->latlon2ji(lat, lon, j, i);
                                    polygon.push_back({j, i});
                                }
                            }
                        }
                    }
                }

                if (polygon.empty()) {
                    continue;
                }

                if (!bboxAvailable) {
                    calculateBoundingBox(polygon, minJ, minI, maxJ, maxI);
                    LOG4CPLUS_INFO(logger, "Bounding box calculated: [" << minI << ", " << minJ << ", " << maxI << ", " << maxJ << "]");
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

void Areas::loadFromShp(const string& fileName, std::shared_ptr<WacommAdapter> wacommAdapter) {
    LOG4CPLUS_INFO(logger, "Reading from shapefile:" << fileName);

    SHPHandle hSHP = SHPOpen(fileName.c_str(), "rb");
    if (hSHP == nullptr) {
        LOG4CPLUS_ERROR(logger, "Unable to open shapefile: " << fileName);
        return;
    }

    int nEntities, nShapeType;
    double adfMinBound[4], adfMaxBound[4];

    SHPGetInfo(hSHP, &nEntities, &nShapeType, adfMinBound, adfMaxBound);

    Array::Array2 mask = wacommAdapter->Mask();

    for (int i = 0; i < nEntities; i++) {
        SHPObject* psShape = SHPReadObject(hSHP, i);
        if (psShape == nullptr || psShape->nSHPType != SHPT_POLYGON) {
            SHPDestroyObject(psShape);
            continue;
        }

        vector<area_data> polygon;
        for (int j = 0; j < psShape->nVertices; j++) {
            double lat = psShape->padfY[j];
            double lon = psShape->padfX[j];

            double pJ, pI;
            wacommAdapter->latlon2ji(lat, lon, pJ, pI);
            polygon.push_back({pJ, pI});
        }

        if (polygon.empty()) {
            SHPDestroyObject(psShape);
            continue;
        }

        double minI, minJ, maxI, maxJ;
        calculateBoundingBox(polygon, minJ, minI, maxJ, maxI);

        for (int j = int(minJ); j <= int(maxJ); j++) {
            for (int i = int(minI); i <= int(maxI); i++) {
                if (mask(j, i) == 1) {
                    if (isPointInPolygon({static_cast<double>(j), static_cast<double>(i)}, polygon)) {
                        this->push_back(Area(j, i));
                    }
                }
            }
        }

        SHPDestroyObject(psShape);
    }

    SHPClose(hSHP);
}

Areas::~Areas() = default;