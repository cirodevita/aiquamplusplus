//
// Created by Ciro De Vita on 29/01/24.
//

#include "AiquamPlusPlus.hpp"

AiquamPlusPlus::~AiquamPlusPlus() = default;

AiquamPlusPlus::AiquamPlusPlus(std::shared_ptr<Config> config): config(config) {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));
}

void AiquamPlusPlus::run() {
    Aiquam aiquam(config);

    int world_size = 1, world_rank = 0;
    size_t ncInputs = config->NcInputs().size();

    std::unique_ptr<float[]> timeseries;

#ifdef USE_MPI
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
#endif

    // Define a vector of integers hosting the number of points for each processor
    std::unique_ptr<int[]> send_counts = std::make_unique<int[]>(world_size);

    // Define a vector of integers hosting the displacement 0
    std::unique_ptr<int[]> displs = std::make_unique<int[]>(world_size);

    // Define the sending buffer
    std::unique_ptr<float[]> sendbuf;

    // Define the receiving buffer
    std::unique_ptr<float[]> recvbuf;

    for (size_t fileIdx = 0; fileIdx < ncInputs; ++fileIdx) {
        std::string& ncInput = config->NcInputs()[fileIdx];

        if (world_rank == 0) {
            LOG4CPLUS_INFO(logger, world_rank << ": Input from Ocean Model: " << ncInput);
        }

        shared_ptr<WacommAdapter> wacommAdapter;
        wacommAdapter = make_shared<WacommAdapter>(ncInput);
        wacommAdapter->process();

        size_t time=wacommAdapter->Conc().Nx();
        size_t depth=wacommAdapter->Conc().Ny();
        size_t lat=wacommAdapter->Conc().Nz();
        size_t lon=wacommAdapter->Conc().N4();

        if (world_rank==0) {
            LOG4CPLUS_DEBUG(logger, "time: " + std::to_string(time));
            LOG4CPLUS_DEBUG(logger, "depth: " + std::to_string(depth));
            LOG4CPLUS_DEBUG(logger, "lat: " + std::to_string(lat));
            LOG4CPLUS_DEBUG(logger, "lon: " + std::to_string(lon));
        }

        size_t nPoints = lat * lon;
        if (fileIdx == 0) {
            timeseries = std::make_unique<float[]>(nPoints * ncInputs);
        }

        if (world_rank == 0) {
            // Get the number of points to be processed for each process
            size_t pointsPerProcess = nPoints / world_size;

            // Get the number of spare points for the process with world_rank==0
            size_t spare = nPoints % world_size;

            // The process with world_rank==0 will calculate extra points
            send_counts[0] = (int)((pointsPerProcess + spare));

            // The process with world_rank==0 will process the first points
            displs[0]=0;

            LOG4CPLUS_DEBUG(logger, world_rank << ": send_counts[0]=" << send_counts.get()[0] << " displ[0]=" << displs.get()[0]);

            // Prepare counts and displacement for data distribution
            // For each process...
            for (int i = 1; i < world_size; i++) {

                // Set the number of points per process
                send_counts.get()[i] = (int)(pointsPerProcess);

                // Set the displacement
                displs.get()[i]=send_counts.get()[0]+pointsPerProcess*(i-1);

                LOG4CPLUS_DEBUG(logger, world_rank << ": send_counts[" << i << "]=" << send_counts.get()[i] << " displ[" << i << "]=" << displs.get()[i]);
            }

            // Prepare a buffer of point data
            sendbuf=std::make_unique<float[]>(nPoints);

            for (int idx = 0; idx < nPoints; idx++) {
                int lat_idx = idx / lon;
                int lon_idx = idx % lon;

                float current_conc = 0.0;
                for (int z_idx = 0; z_idx < depth; z_idx++) {
                    //TODO: check if is a fill value
                    current_conc += wacommAdapter->Conc()(0,z_idx,lat_idx,lon_idx);
                }
                sendbuf[idx]=current_conc;
            }
        }

#ifdef USE_MPI
        // Broadcast the number of points for each processor
        MPI_Bcast(send_counts.get(),world_size,MPI_INT,0,MPI_COMM_WORLD);

        int pointsToProcess=send_counts.get()[world_rank];

        LOG4CPLUS_DEBUG(logger, world_rank << ":" << " pointsToProcess:" << pointsToProcess << std::endl);

        // Allocate the receiving buffer
        recvbuf=std::make_unique<float[]>(pointsToProcess);

        // Define a variable that will contain the mpiError
        int mpiError;

        // Distribute to all processes the send buffer
        mpiError=MPI_Scatterv(sendbuf.get(), send_counts.get(), displs.get(), MPI_FLOAT,
                    recvbuf.get(), pointsToProcess, MPI_FLOAT, 0, MPI_COMM_WORLD);

        for (int idx = 0; idx < pointsToProcess; idx++) {
            int global_idx = displs[world_rank] + idx;
            timeseries[global_idx * ncInputs + fileIdx] = recvbuf[idx];
        }
#else
        for (int idx = 0; idx < nPoints; idx++) {
            timeseries[idx * ncInputs + fileIdx] = sendbuf[idx];
        }
#endif
    }

    auto start_whole = std::chrono::high_resolution_clock::now();
    for (int idx = 0; idx < send_counts[world_rank]; idx++) {
        int global_idx = displs[world_rank] + idx;
        std::vector<float> input_data(ncInputs);

        for (int file_idx = 0; file_idx < ncInputs; file_idx++) {
            input_data[file_idx] = timeseries[global_idx * ncInputs + file_idx];
        }

        int predicted_class = aiquam.inference(input_data);

        LOG4CPLUS_DEBUG(logger, world_rank <<": Predicted class: " << predicted_class << std::endl);
    }
    auto end_whole = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsedLocal = end_whole - start_whole;

    LOG4CPLUS_INFO(logger, world_rank << " Global Execution Time (sec): " << elapsedLocal.count());
}