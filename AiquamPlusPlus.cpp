//
// Created by Ciro De Vita on 29/01/24.
//

#include "AiquamPlusPlus.hpp"

AiquamPlusPlus::~AiquamPlusPlus() = default;

AiquamPlusPlus::AiquamPlusPlus(std::shared_ptr<Config> config): config(config) {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));

    areas = std::make_shared<Areas>();
}

// Function to serialize the struct into a buffer
void AiquamPlusPlus::serialize(const area_data& data, std::vector<char>& buffer) {
    size_t total_size = sizeof(data.i) + sizeof(data.j) + sizeof(size_t) + data.values.size() * sizeof(float);
    buffer.resize(total_size);
    char* ptr = buffer.data();

    memcpy(ptr, &data.i, sizeof(data.i));
    ptr += sizeof(data.i);
    
    memcpy(ptr, &data.j, sizeof(data.j));
    ptr += sizeof(data.j);
    
    size_t vec_size = data.values.size();
    memcpy(ptr, &vec_size, sizeof(vec_size));
    ptr += sizeof(vec_size);

    memcpy(ptr, data.values.data(), vec_size * sizeof(float));
}

// Function to deserialize the buffer back into the struct
void AiquamPlusPlus::deserialize(const std::vector<char>& buffer, area_data& data) {
    const char* ptr = buffer.data();

    memcpy(&data.i, ptr, sizeof(data.i));
    ptr += sizeof(data.i);

    memcpy(&data.j, ptr, sizeof(data.j));
    ptr += sizeof(data.j);

    size_t vec_size;
    memcpy(&vec_size, ptr, sizeof(vec_size));
    ptr += sizeof(vec_size);

    data.values.resize(vec_size);
    memcpy(data.values.data(), ptr, vec_size * sizeof(float));
}

void AiquamPlusPlus::run() {
    int ompMaxThreads=1, ompThreadNum=0;
    int world_size=1, world_rank=0, nAreas=0;
    int ncInputs = config->NcInputs().size();

    Aiquam aiquam(config);

#ifdef USE_OMP
    // Get the number of threads available for computation
    ompMaxThreads=omp_get_max_threads();
#endif

#ifdef USE_MPI
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
#endif

    // Define a vector of integers hosting the number of areas for each processor
    std::unique_ptr<int[]> send_counts = std::make_unique<int[]>(world_size);

    // Define a vector of integers hosting the displacement 0
    std::unique_ptr<int[]> displs = std::make_unique<int[]>(world_size);

    // Define the sending buffer
    std::vector<char> sendbuf;

    // Define the receiving buffer
    std::vector<char> recvbuf;

    std::unique_ptr<Areas> localAreas;

    Areas *pLocalAreas;

    for (int fileIdx = 0; fileIdx < ncInputs; ++fileIdx) {
        std::string& ncInput = config->NcInputs()[fileIdx];

        if (world_rank == 0) {
            LOG4CPLUS_INFO(logger, world_rank << ": Input from Ocean Model: " << ncInput);
        }

        shared_ptr<WacommAdapter> wacommAdapter = make_shared<WacommAdapter>(ncInput);
        wacommAdapter->process();

        if (world_rank == 0 && fileIdx == 0) {
            string fileName = config->AreasFile();

            if (fileName.substr(fileName.find_last_of('.') + 1) == "json") {
                areas->loadFromJson(fileName, wacommAdapter);
                nAreas = areas->size();
            }
        }

        size_t time = wacommAdapter->Conc().Nx();
        size_t depth = wacommAdapter->Conc().Ny();
        size_t lat = wacommAdapter->Conc().Nz();
        size_t lon = wacommAdapter->Conc().N4();

        if (world_rank == 0) {
            // Calculate the number of areas for each process
            size_t areasPerProcess = nAreas / world_size;
            size_t spare = nAreas % world_size;

            // Calculate send counts and displacements
            for (int i = 0; i < world_size; i++) {
                send_counts[i] = areasPerProcess + (i < spare ? 1 : 0);
                displs[i] = (i > 0) ? (displs[i - 1] + send_counts[i - 1]) : 0;
                send_counts[i] *= sizeof(double) * 2 + sizeof(size_t) + (fileIdx + 1) * sizeof(float);

                LOG4CPLUS_DEBUG(logger, world_rank << ": send_counts[0]=" << send_counts.get()[0] << " displ[0]=" << displs.get()[0]);
            }

            // Serialize the data
            sendbuf.clear();
            for (int idx = 0; idx < nAreas; idx++) {
                Area& area = areas->at(idx);

                float current_conc = 0.0;
                for (int z_idx = 0; z_idx < depth; z_idx++) {
                    // TODO: add check for NAN values
                    // current_conc += wacommAdapter->Conc()(0, z_idx, area.J(), area.I());
                    current_conc += 0;
                }
                area.addValue(current_conc);

                area_data data;
                data.i = area.I();
                data.j = area.J();
                data.values = area.Values();

                std::vector<char> serialized_data;
                serialize(data, serialized_data);
                sendbuf.insert(sendbuf.end(), serialized_data.begin(), serialized_data.end());
            }

            LOG4CPLUS_DEBUG(logger, "sendbuf size: " << sendbuf.size());
        }

#ifdef USE_MPI
        // Broadcast the number of areas for each process
        MPI_Bcast(send_counts.get(), world_size, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(displs.get(), world_size, MPI_INT, 0, MPI_COMM_WORLD);

        // Calculate the size of data each process will receive
        int recv_count = send_counts[world_rank];
        recvbuf.resize(recv_count);

        // Distribute the data
        MPI_Scatterv(sendbuf.data(), send_counts.get(), displs.get(), MPI_CHAR, recvbuf.data(), recv_count, MPI_CHAR, 0, MPI_COMM_WORLD);

        // Deserialize the data
        localAreas = std::make_unique<Areas>();
        int num_local_areas = send_counts[world_rank] / (sizeof(double) * 2 + sizeof(size_t) + (fileIdx + 1) * sizeof(float));
        size_t offset = 0;
        for (int i = 0; i < num_local_areas; i++) {
            std::vector<char> buffer(recvbuf.begin() + offset, recvbuf.begin() + offset + sizeof(double) * 2 + sizeof(size_t) + (fileIdx + 1) * sizeof(float));
            area_data local_area;
            deserialize(buffer, local_area);
            Area area(local_area);
            localAreas->push_back(area);
            offset += sizeof(double) * 2 + sizeof(size_t) + local_area.values.size() * sizeof(float);
        }

        pLocalAreas = localAreas.get();
#else
        pLocalAreas = areas.get();
        
#endif
    }

    LOG4CPLUS_INFO(logger, world_rank << ": Local areas:" << pLocalAreas->size());

    int areasToProcess = pLocalAreas->size();

    // Get the number of areas to be processed by each thread
    size_t areasPerThread = areasToProcess / ompMaxThreads;

    // Get the number of spare areas for the thread with tidx==0
    size_t sparePerThread = areasToProcess % ompMaxThreads;
    
    // Dafine an array with the number of areas to be processed by each thread
    size_t thread_counts[ompMaxThreads];

    // Define an array with the displacement of areas to be processed by each thread
    size_t thread_displs[ompMaxThreads];

    // The first thread get the spare
    thread_counts[0] = (int)((areasPerThread + sparePerThread));

    // The first tread starts from 0
    thread_displs[0] = 0;

    // For each available thread...
    for (int tidx = 1; tidx < ompMaxThreads; tidx++) {

        // Set the number of particles per thread
        thread_counts[tidx] = (int)(areasPerThread);

        // Set the displaement
        thread_displs[tidx]=thread_counts[0]+areasPerThread*(tidx-1);
    }

    #pragma omp parallel default(none) private(ompThreadNum) shared(world_rank, thread_counts, thread_displs, pLocalAreas, areasPerThread, aiquam)
    {
#ifdef USE_OMP
        // Get the number of the current thread
        ompThreadNum = omp_get_thread_num();
#endif
        // Get the index (array pLocalParticles) of the first particle the thread must process
        size_t first = thread_displs[ompThreadNum];

        // Get the index (array pLocalParticles) of the last particle the thread must process
        size_t last = first + thread_counts[ompThreadNum];

        LOG4CPLUS_DEBUG(logger, world_rank << ": ompThreadNum: " << ompThreadNum << ", first: " << first << ", last: " << last);

        for (size_t idx = first; idx < last; idx++) {
            LOG4CPLUS_DEBUG(logger, world_rank << ": ompThreadNum: " << ompThreadNum << ": idx: " << idx << ": i:" << pLocalAreas->at(idx).data().i << ", j: " << pLocalAreas->at(idx).data().j << ", values: " << pLocalAreas->at(idx).data().values.size());
            int predicted_class = aiquam.inference(pLocalAreas->at(idx).data().values);
            LOG4CPLUS_DEBUG(logger, world_rank << ": Predicted class: " << predicted_class << std::endl);
        }
    }

    if (world_rank == 0) {
        LOG4CPLUS_INFO(logger, "Finish");
    }

    /*
    auto start_whole = std::chrono::high_resolution_clock::now();
    for (int idx=0; idx<pLocalAreas->size(); idx++) {
        LOG4CPLUS_INFO(logger, world_rank << ": idx: " << idx << ": i:" << pLocalAreas->at(idx).data().i << ", j: " << pLocalAreas->at(idx).data().j << ", values: " << pLocalAreas->at(idx).data().values.size());

        // int predicted_class = aiquam.inference(pLocalAreas->at(idx).data().values);
        // LOG4CPLUS_DEBUG(logger, world_rank <<": Predicted class: " << predicted_class << std::endl);
    }
    auto end_whole = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsedLocal = end_whole - start_whole;

    LOG4CPLUS_INFO(logger, world_rank << " Global Execution Time (sec): " << elapsedLocal.count());
    */
}