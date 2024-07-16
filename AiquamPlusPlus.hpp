//
// Created by Ciro De Vita on 29/01/24.
//

#ifndef AIQUAMPLUSPLUS_AIQUAMMPLUSPLUS_HPP
#define AIQUAMPLUSPLUS_AIQUAMMPLUSPLUS_HPP

// log4cplus - https://github.com/log4cplus/log4cplus
#include "log4cplus/configurator.h"
#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"

#include "Config.hpp"
#include "WacommAdapter.hpp"
#include "Aiquam.hpp"
#include "Areas.hpp"

#include <string>

#ifdef USE_OMP
#include <omp.h>
#endif

#ifdef USE_MPI
#define OMPI_SKIP_MPICXX
#include <mpi.h>
#endif

using namespace std;

class AiquamPlusPlus {
public:
    AiquamPlusPlus(std::shared_ptr<Config> config);
    ~AiquamPlusPlus();

    void run();

private:
    log4cplus::Logger logger;
    std::shared_ptr<Config> config;
    std::shared_ptr<Areas> areas;

    void serialize(const area_data& data, std::vector<char>& buffer);
    void deserialize(const std::vector<char>& buffer, area_data& data);
};

#endif //AIQUAMPLUSPLUS_AIQUAMMPLUSPLUS_HPP
