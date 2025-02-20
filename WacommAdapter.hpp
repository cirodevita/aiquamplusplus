//
// Created by Ciro De Vita on 09/07/24.
//

#ifndef AIQUAMPLUSPLUS_WACOMMADAPTER_HPP
#define AIQUAMPLUSPLUS_WACOMMADAPTER_HPP

// log4cplus - https://github.com/log4cplus/log4cplus
#include "log4cplus/configurator.h"
#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"

#include <math.h>

#include "Array.h"
#include "netcdf"
#include <nanoflann.hpp>

struct wacomm_data {
    Array::Array1<double> time;
    Array::Array1<double> depth;
    Array::Array1<double> lat;
    Array::Array1<double> lon;
    Array::Array1<double> latRad;
    Array::Array1<double> lonRad;
    Array::Array4<double> conc;
    Array::Array3<double> sfconc;
    Array::Array2<double> mask;
    double fillValue;
};

struct PointCloud {
    std::vector<std::array<double, 2>> points; // {lat, lon}

    inline size_t kdtree_get_point_count() const { return points.size(); }

    inline double kdtree_get_pt(const size_t idx, const size_t dim) const {
        return points[idx][dim];
    }

    template <class BBOX>
    bool kdtree_get_bbox(BBOX&) const { return false; }
};

typedef nanoflann::KDTreeSingleIndexAdaptor<
    nanoflann::L2_Simple_Adaptor<double, PointCloud>,
    PointCloud,
    2
> KDTree;

class WacommAdapter {
    public:
        WacommAdapter(std::string &fileName);
        ~WacommAdapter();

        void process();
        void initializeKDTree();

        void latlon2ji(double lat, double lon, double &j, double &i);

        float calculateConc(double j, double i);

        wacomm_data *dataptr();

        Array::Array1<double> &Time();
        Array::Array1<double> &Depth();
        Array::Array1<double> &Lat();
        Array::Array1<double> &Lon();
        Array::Array1<double> &LatRad();
        Array::Array1<double> &LonRad();
        Array::Array4<double> &Conc();
        Array::Array3<double> &Sfconc();
        Array::Array2<double> &Mask();
        double &FillValue();

    private:
        log4cplus::Logger logger;
        wacomm_data _data;
        std::string &fileName;

        PointCloud cloud;
        KDTree* kdTree = nullptr;

        double sgn(double a);
};

#endif //AIQUAMPLUSPLUS_WACOMMADAPTER_HPP