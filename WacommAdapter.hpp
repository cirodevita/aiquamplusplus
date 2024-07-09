//
// Created by Ciro De Vita on 09/07/24.
//

#ifndef AIQUAMPLUSPLUS_WACOMMADAPTER_HPP
#define AIQUAMPLUSPLUS_WACOMMADAPTER_HPP

// log4cplus - https://github.com/log4cplus/log4cplus
#include "log4cplus/configurator.h"
#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"

#include "Array.h"
#include "netcdf"

struct wacomm_data {
    Array::Array4<float> conc;
    Array::Array3<float> sfconc;
};

class WacommAdapter {
    public:
        WacommAdapter(std::string &fileName);
        ~WacommAdapter();

        void process();

        wacomm_data *dataptr();

        Array::Array4<float> &Conc();
        Array::Array3<float> &Sfconc();

    private:
        log4cplus::Logger logger;
        wacomm_data _data;
        std::string &fileName;
};

#endif //AIQUAMPLUSPLUS_WACOMMADAPTER_HPP