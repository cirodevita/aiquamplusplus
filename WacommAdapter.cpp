//
// Created by Ciro De Vita on 09/07/24.
//

#include "WacommAdapter.hpp"

WacommAdapter::WacommAdapter(std::string &fileName): fileName(fileName) {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));
}

Array::Array4<float> &WacommAdapter::Conc() { return _data.conc; }
Array::Array3<float> &WacommAdapter::Sfconc() { return _data.sfconc; }

wacomm_data *WacommAdapter::dataptr() {
    return &_data;
}

void WacommAdapter::process() {
    LOG4CPLUS_DEBUG(logger,"Wacomm file loading:"+fileName);

    // Open the file for read access
    netCDF::NcFile dataFile(fileName, netCDF::NcFile::read);

    // Retrieve the variable named "time"
    netCDF::NcVar varTime=dataFile.getVar("time");
    size_t time = varTime.getDim(0).getSize();

    // Retrieve the variable named "depth"
    netCDF::NcVar varDepth=dataFile.getVar("depth");
    size_t depth = varDepth.getDim(0).getSize();

    // Retrieve the variable named "latitude"
    netCDF::NcVar varLat=dataFile.getVar("latitude");
    size_t lat = varLat.getDim(0).getSize();

    // Retrieve the variable named "longitude"
    netCDF::NcVar varLon=dataFile.getVar("longitude");
    size_t lon = varLon.getDim(0).getSize();

    // Retrieve the variable named "conc"
    netCDF::NcVar varConc=dataFile.getVar("conc");
    Array::Array4<float> conc(time,depth,lat,lon);
    varConc.getVar(conc());

    // Retrieve the variable named "sfconc"
    netCDF::NcVar varSfconc=dataFile.getVar("sfconc");
    Array::Array3<float> sfconc(time,lat,lon);
    varSfconc.getVar(sfconc());

    this->Conc().Allocate(time,depth,lat,lon);
    this->Sfconc().Allocate(time,lat,lon);

    this->Conc().Load(conc());
    this->Sfconc().Load(sfconc());
}

WacommAdapter::~WacommAdapter() = default;