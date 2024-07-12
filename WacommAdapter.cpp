//
// Created by Ciro De Vita on 09/07/24.
//

#include "WacommAdapter.hpp"

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
    netCDF::NcVar varDepth=dataFile.getVar("depth");
    size_t dimDepth = varDepth.getDim(0).getSize();
    Array::Array1<double> depth(dimDepth);
    varDepth.getVar(depth());

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
    varConc.getVar(conc());

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

    for (int i=0; i<dimLat;i++) {
        this->LatRad().operator()(i)=0.0174533*this->Lat()(i);
    }

    for (int i=0; i<dimLon;i++) {
        this->LonRad().operator()(i)=0.0174533*this->Lon()(i);
    }
}

void WacommAdapter::latlon2ji(double lat, double lon, double &j, double &i) {
    int minJ, minI;
    double d, d1, d2, dd, minD=1e37;

    double latRad=0.0174533*lat;
    double lonRad=0.0174533*lon;

    size_t eta = _data.mask.Nx();
    size_t xi = _data.mask.Ny();

    for (int j=0; j<eta; j++) {
        for (int i=0; i<xi; i++) {
            d1=(latRad-_data.latRad(j));

            d2=(lonRad-_data.lonRad(i));

            dd=pow(sin(0.5*d1),2) +
               pow(sin(0.5*d2),2)*
               cos(latRad)*
               cos(_data.latRad(j));
            d=2.0*atan2(pow(dd,.5),pow(1.0-dd,.5))*6371.0;

            if (d<minD) {
                minD=d;
                minJ=j;
                minI=i;
            }
        }
    }

    double dLat=latRad-_data.latRad(minJ);
    double dLon=lonRad-_data.lonRad(minI);
    int otherJ=minJ+sgn(dLat);
    int otherI=minI+sgn(dLon);
    if (dLat!=0) {
        double aLat = abs(_data.latRad(minJ)-_data.latRad(otherJ));
        double jF=abs(dLat)/aLat;
        j=std::min(minJ,otherJ)+jF;
    } else {
        j=minJ;
    }

    if (dLon!=0) {
        double aLon = abs(_data.lonRad(minI)-_data.lonRad(otherI));
        double iF=abs(dLon)/aLon;
        i=std::min(minI,otherI)+iF;
    } else {
        i=minI;
    }

    LOG4CPLUS_INFO(logger, "lat, lon: " << _data.lat(j) << ", " << _data.lon(i));
}

// Returns -1 if a < 0 and 1 if a > 0
double WacommAdapter::sgn(double a) { return (a > 0) - (a < 0); }