//
// Created by Ciro De Vita on 09/07/24.
//

#include "WacommAdapter.hpp"

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::milliseconds;

WacommAdapter::WacommAdapter(std::string &fileName): fileName(fileName) {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));
}

WacommAdapter::~WacommAdapter() = default;

Array::Array1<double> &WacommAdapter::Time() { return _data.time; }
Array::Array1<double> &WacommAdapter::Depth() { return _data.depth; }
Array::Array1<double> &WacommAdapter::Lat() { return _data.lat; }
Array::Array1<double> &WacommAdapter::Lon() { return _data.lon; }
Array::Array1<double> &WacommAdapter::LatRad() { return _data.latRad; }
Array::Array1<double> &WacommAdapter::LonRad() { return _data.lonRad; }
Array::Array4<double> &WacommAdapter::Conc() { return _data.conc; }
Array::Array3<double> &WacommAdapter::Sfconc() { return _data.sfconc; }
Array::Array2<double> &WacommAdapter::Mask() { return _data.mask; }
double &WacommAdapter::FillValue() { return _data.fillValue; }

wacomm_data *WacommAdapter::dataptr() {
    return &_data;
}

void WacommAdapter::process() {
    LOG4CPLUS_DEBUG(logger,"Wacomm file loading:"+fileName);

    // Open the file for read access
    netCDF::NcFile dataFile(fileName, netCDF::NcFile::read);

    // Retrieve the variable named "time"
    netCDF::NcVar varTime=dataFile.getVar("time");
    size_t dimTime = varTime.getDim(0).getSize();
    Array::Array1<double> time(dimTime);
    varTime.getVar(time());

    // Retrieve the variable named "depth"
    netCDF::NcVar varDepth = dataFile.getVar("depth");
    size_t totalDepth = varDepth.getDim(0).getSize();
    Array::Array1<double> fullDepth(totalDepth);
    varDepth.getVar(fullDepth());

    // Determine number of depth levels up to 30 meters
    size_t dimDepth = 0;
    for (size_t i = 0; i < totalDepth; i++) {
        if (fullDepth(i) <= 30.0) {
            dimDepth++;
        } else {
            break;
        }
    }
    Array::Array1<double> depth(dimDepth);
    for (size_t i = 0; i < dimDepth; i++) {
        depth[i] = fullDepth[i];
    }


    // Retrieve the variable named "latitude"
    netCDF::NcVar varLat=dataFile.getVar("latitude");
    size_t dimLat = varLat.getDim(0).getSize();
    Array::Array1<double> lat(dimLat);
    varLat.getVar(lat());

    // Retrieve the variable named "longitude"
    netCDF::NcVar varLon=dataFile.getVar("longitude");
    size_t dimLon = varLon.getDim(0).getSize();
    Array::Array1<double> lon(dimLon);
    varLon.getVar(lon());

    // Retrieve the variable named "conc"
    netCDF::NcVar varConc=dataFile.getVar("conc");
    Array::Array4<double> conc(dimTime,dimDepth,dimLat,dimLon);
    netCDF::NcVarAtt fillValueAtt = varConc.getAtt("_FillValue");
    fillValueAtt.getValues(&_data.fillValue);
    std::vector<size_t> start = {0, 0, 0, 0};
    std::vector<size_t> count = {dimTime, dimDepth, dimLat, dimLon};
    varConc.getVar(start, count, conc());

    // Retrieve the variable named "sfconc"
    netCDF::NcVar varSfconc=dataFile.getVar("sfconc");
    Array::Array3<double> sfconc(dimTime,dimLat,dimLon);
    varSfconc.getVar(sfconc());

    // Retrieve the variable named "mask"
    netCDF::NcVar varmask=dataFile.getVar("mask");
    Array::Array2<double> mask(dimLat,dimLon);
    varmask.getVar(mask());

    this->Time().Allocate(dimTime);
    this->Depth().Allocate(dimDepth);
    this->Lat().Allocate(dimLat);
    this->Lon().Allocate(dimLon);
    this->LatRad().Allocate(dimLat);
    this->LonRad().Allocate(dimLon);
    this->Conc().Allocate(dimTime,dimDepth,dimLat,dimLon);
    this->Sfconc().Allocate(dimTime,dimLat,dimLon);
    this->Mask().Allocate(dimLat,dimLon);

    this->Time().Load(time());
    this->Depth().Load(depth());
    this->Lat().Load(lat());
    this->Lon().Load(lon());
    this->Conc().Load(conc());
    this->Sfconc().Load(sfconc());
    this->Mask().Load(mask());

    #pragma omp parallel for collapse(1) default(none) shared(dimLat)
    for (int i=0; i<dimLat;i++) {
        this->LatRad().operator()(i)=0.0174533*this->Lat()(i);
    }

    #pragma omp parallel for collapse(1) default(none) shared(dimLon)
    for (int i=0; i<dimLon;i++) {
        this->LonRad().operator()(i)=0.0174533*this->Lon()(i);
    }
}

void WacommAdapter::initializeKDTree() {
    size_t eta = _data.mask.Nx();
    size_t xi = _data.mask.Ny();

    for (int j = 0; j < eta; j++) {
        for (int i = 0; i < xi; i++) {
            cloud.points.push_back({_data.latRad(j), _data.lonRad(i)});
        }
    }

    kdTree = new KDTree(2, cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10));
    kdTree->buildIndex();
}


void WacommAdapter::latlon2ji(double lat, double lon, double &j, double &i) {
    if (!kdTree) {
        LOG4CPLUS_ERROR(logger, "KD-Tree not initialized!");
        return;
    }

    double query[2] = {0.0174533 * lat, 0.0174533 * lon};
    size_t nearestIdx;
    double outDistSqr;

    nanoflann::KNNResultSet<double> resultSet(1);
    resultSet.init(&nearestIdx, &outDistSqr);
    kdTree->findNeighbors(resultSet, query, nanoflann::SearchParameters(10));

    size_t eta = _data.mask.Nx();
    size_t xi = _data.mask.Ny();

    if (nearestIdx >= cloud.points.size()) {
        LOG4CPLUS_ERROR(logger, "KD-Tree index out of bounds: " << nearestIdx);
        return;
    }

    double nearestLat = cloud.points[nearestIdx][0];
    double nearestLon = cloud.points[nearestIdx][1];

    int minJ = -1, minI = -1;
    for (size_t idx = 0; idx < eta; ++idx) {
        if (std::abs(_data.latRad(idx) - nearestLat) < 1e-6) {
            minJ = idx;
            break;
        }
    }
    for (size_t idx = 0; idx < xi; ++idx) {
        if (std::abs(_data.lonRad(idx) - nearestLon) < 1e-6) {
            minI = idx;
            break;
        }
    }

    if (minJ == -1 || minI == -1) {
        LOG4CPLUS_ERROR(logger, "Error: Unable to find indices for lat/lon in dataset.");
        return;
    }

    double dLat = query[0] - _data.latRad(minJ);
    double dLon = query[1] - _data.lonRad(minI);

    int otherJ = minJ + sgn(dLat);
    int otherI = minI + sgn(dLon);

    if (dLat != 0 && otherJ >= 0 && otherJ < eta) {
        double aLat = std::abs(_data.latRad(minJ) - _data.latRad(otherJ));
        double jF = std::abs(dLat) / aLat;
        j = std::min(minJ, otherJ) + jF;
    } else {
        j = minJ;
    }

    if (dLon != 0 && otherI >= 0 && otherI < xi) {
        double aLon = std::abs(_data.lonRad(minI) - _data.lonRad(otherI));
        double iF = std::abs(dLon) / aLon;
        i = std::min(minI, otherI) + iF;
    } else {
        i = minI;
    }

    LOG4CPLUS_DEBUG(logger, "Interpolated j: " << j << ", i: " << i);
}

// Returns -1 if a < 0 and 1 if a > 0
double WacommAdapter::sgn(double a) { return (a > 0) - (a < 0); }

float WacommAdapter::calculateConc(double j, double i) {
    float conc = 0.0;
    for (int idx = 0; idx < this->Depth().Size(); idx++) {
        float current_conc = this->Conc()(0, idx, j, i);
        if (current_conc != this->FillValue()) {
            conc += current_conc;
        }
    }
    return conc;
}