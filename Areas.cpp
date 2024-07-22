//
// Created by Ciro De Vita on 11/07/24.
//

#include "Areas.hpp"

Areas::Areas() {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));
    GDALAllRegister();
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
                break;
            }
        }

    } catch (const nlohmann::json::parse_error& e) {
        LOG4CPLUS_ERROR(logger,e.what());
    }
}

void Areas::loadFromShp(const string& fileName, std::shared_ptr<WacommAdapter> wacommAdapter) {
    LOG4CPLUS_INFO(logger, "Reading from shapefile:" << fileName);

    GDALDataset* poDS = (GDALDataset*) GDALOpenEx(fileName.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
    if (poDS == nullptr) {
        LOG4CPLUS_ERROR(logger, "Unable to open shapefile: " << fileName);
        return;
    }

    OGRLayer* poLayer = poDS->GetLayer(0);
    if (poLayer == nullptr) {
        LOG4CPLUS_ERROR(logger, "Unable to get layer from shapefile: " << fileName);
        GDALClose(poDS);
        return;
    }

    Array::Array2 mask = wacommAdapter->Mask();

    OGRFeature* poFeature;
    poLayer->ResetReading();
    while ((poFeature = poLayer->GetNextFeature()) != nullptr) {
        OGRGeometry* poGeometry = poFeature->GetGeometryRef();
        if (poGeometry == nullptr || wkbFlatten(poGeometry->getGeometryType()) != wkbPolygon) {
            OGRFeature::DestroyFeature(poFeature);
            continue;
        }

        vector<area_data> polygon;
        OGRPolygon* poPolygon = (OGRPolygon*) poGeometry;
        OGRLinearRing* poRing = poPolygon->getExteriorRing();
        for (int i = 0; i < poRing->getNumPoints(); i++) {
            double lat = poRing->getY(i);
            double lon = poRing->getX(i);

            double pJ, pI;
            wacommAdapter->latlon2ji(lat, lon, pJ, pI);
            polygon.push_back({pJ, pI});
        }

        if (polygon.empty()) {
            OGRFeature::DestroyFeature(poFeature);
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

        OGRFeature::DestroyFeature(poFeature);
    }

    GDALClose(poDS);
}

Areas::~Areas() = default;