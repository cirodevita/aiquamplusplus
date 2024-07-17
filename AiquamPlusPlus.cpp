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

    Array::Array3<int> predictions;

    shared_ptr<WacommAdapter> wacommAdapter;

    for (int fileIdx = 0; fileIdx < ncInputs; ++fileIdx) {
        std::string& ncInput = config->NcInputs()[fileIdx];

        if (world_rank == 0) {
            LOG4CPLUS_INFO(logger, world_rank << ": Input from Ocean Model: " << ncInput);
        }

        wacommAdapter = make_shared<WacommAdapter>(ncInput);
        wacommAdapter->process();

        if (world_rank == 0 && fileIdx == 0) {
            string fileName = config->AreasFile();

            if (fileName.substr(fileName.find_last_of('.') + 1) == "json") {
                areas->loadFromJson(fileName, wacommAdapter);
                nAreas = areas->size();
            }

            // Calculate the number of areas for each process
            size_t areasPerProcess = nAreas / world_size;
            size_t spare = nAreas % world_size;

            // Calculate send counts and displacements
            for (int i = 0; i < world_size; i++) {
                send_counts[i] = areasPerProcess + (i < spare ? 1 : 0);
                displs[i] = (i > 0) ? (displs[i - 1] + send_counts[i - 1]) : 0;
                send_counts[i] *= sizeof(double) * 2 + sizeof(size_t) + ncInputs * sizeof(float);

                LOG4CPLUS_DEBUG(logger, world_rank << ": send_counts[0]=" << send_counts.get()[0] << " displ[0]=" << displs.get()[0]);
            }
        }

        if (fileIdx == 0) {
            size_t time = wacommAdapter->Conc().Nx();
            size_t depth = wacommAdapter->Conc().Ny();
            size_t lat = wacommAdapter->Conc().Nz();
            size_t lon = wacommAdapter->Conc().N4();

            // Define the final predictions matrix
            predictions.Allocate(time, lat, lon);
            
            // Set the predictions matrix to 0
            #pragma omp parallel for collapse(3) default(none) shared(time, lat, lon, predictions)
            for (int t=0; t<time; t++) {
                for (int j=0; j<lat; j++) {
                    for (int i=0; i<lon; i++) {
                        predictions(t,j,i)=100;
                    }
                }
            }
        }

        if (world_rank == 0) {
            // Serialize the data
            sendbuf.clear();
            for (int idx = 0; idx < nAreas; idx++) {
                Area& area = areas->at(idx);
                area.addValue(wacommAdapter->calculateConc(area.J(), area.I()));

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
    int num_local_areas = send_counts[world_rank] / (sizeof(double) * 2 + sizeof(size_t) + ncInputs * sizeof(float));
    size_t offset = 0;
    for (int i = 0; i < num_local_areas; i++) {
        std::vector<char> buffer(recvbuf.begin() + offset, recvbuf.begin() + offset + sizeof(double) * 2 + sizeof(size_t) + ncInputs * sizeof(float));
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

    #pragma omp parallel default(none) private(ompThreadNum) shared(world_rank, thread_counts, thread_displs, pLocalAreas, areasPerThread, predictions)
    {
        Aiquam aiquam(config);

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
            predictions(0, pLocalAreas->at(idx).data().j, pLocalAreas->at(idx).data().i) = predicted_class;
            
            LOG4CPLUS_DEBUG(logger, world_rank << ": ompThreadNum: " << ompThreadNum << ": idx: " << idx << ": i:" << pLocalAreas->at(idx).data().i << ", j: " << pLocalAreas->at(idx).data().j << ", prediction: " << predicted_class << std::endl);
        }
#ifdef USE_OMP
        // Barrier to ensure all threads have finished processing
        #pragma omp barrier
#endif
    }

#ifdef USE_MPI
    // Barrier to ensure all processes have finished
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    if (world_rank == 0) {
        // Create the output filename
        string ncOutputFilename=config->NcOutputRoot()+config->Date()+".nc";
        
        LOG4CPLUS_INFO(logger, "Saving output:" << ncOutputFilename);

        // Save the history
        save(ncOutputFilename, wacommAdapter, predictions);
    }
}

void AiquamPlusPlus::save(const string &fileName, shared_ptr<WacommAdapter> wacommAdapter, Array::Array3<int> &predictions) {
    size_t time = wacommAdapter->Conc().Nx();
    size_t depth = wacommAdapter->Conc().Ny();
    size_t lat = wacommAdapter->Conc().Nz();
    size_t lon = wacommAdapter->Conc().N4();

    LOG4CPLUS_INFO(logger,"Saving in: " << fileName);

    // Open the file for read access
    netCDF::NcFile dataFile(fileName, netCDF::NcFile::replace, netCDF::NcFile::nc4);
    LOG4CPLUS_INFO(logger,"--------------: " << fileName);

    netCDF::NcDim timeDim = dataFile.addDim("time", time);
    netCDF::NcVar timeVar = dataFile.addVar("time", netCDF::ncDouble, timeDim);
    timeVar.putAtt("description","Time since initialization");
    timeVar.putAtt("long_name","time since initialization");
    timeVar.putAtt("units","seconds since 1968-05-23 00:00:00 GMT");
    timeVar.putAtt("calendar","gregorian");
    timeVar.putAtt("field","time, scalar, series");
    timeVar.putAtt("_CoordinateAxisType","Time");
    timeVar.putVar(wacommAdapter->Time()());

    netCDF::NcDim depthDim = dataFile.addDim("depth", depth);
    netCDF::NcVar depthVar = dataFile.addVar("depth", netCDF::ncDouble, depthDim);
    depthVar.putAtt("description","depth");
    depthVar.putAtt("long_name","depth");
    depthVar.putAtt("units","meters");
    depthVar.putVar(wacommAdapter->Depth()());

    netCDF::NcDim lonDim = dataFile.addDim("longitude", lon);
    netCDF::NcVar lonVar = dataFile.addVar("longitude", netCDF::ncDouble, lonDim);
    lonVar.putAtt("description","Longitude");
    lonVar.putAtt("long_name","longitude");
    lonVar.putAtt("units","degrees_east");
    lonVar.putVar(wacommAdapter->Lon()());

    netCDF::NcDim latDim = dataFile.addDim("latitude", lat);
    netCDF::NcVar latVar = dataFile.addVar("latitude", netCDF::ncDouble, latDim);
    latVar.putAtt("description","Latitude");
    latVar.putAtt("long_name","latitude");
    latVar.putAtt("units","degrees_north");
    latVar.putVar(wacommAdapter->Lat()());

    std::vector<netCDF::NcDim> timeDepthLatLon;
    timeDepthLatLon.push_back(timeDim);
    timeDepthLatLon.push_back(depthDim);
    timeDepthLatLon.push_back(latDim);
    timeDepthLatLon.push_back(lonDim);

    netCDF::NcVar concVar = dataFile.addVar("conc", netCDF::ncDouble, timeDepthLatLon);
    concVar.putAtt("description","concentration of suspended matter in sea water");
    concVar.putAtt("units","1");
    concVar.putAtt("long_name","concentration");
    concVar.putAtt("_FillValue", netCDF::ncDouble, 9.99999993e+36);
    concVar.putVar(wacommAdapter->Conc()());

    std::vector<netCDF::NcDim> timeLatLon;
    timeLatLon.push_back(timeDim);
    timeLatLon.push_back(latDim);
    timeLatLon.push_back(lonDim);

    netCDF::NcVar sfconcVar = dataFile.addVar("sfconc", netCDF::ncDouble, timeLatLon);
    sfconcVar.putAtt("description","concentration of suspended matter at the surface");
    sfconcVar.putAtt("units","1");
    sfconcVar.putAtt("long_name","surface_concentration");
    sfconcVar.putAtt("_FillValue", netCDF::ncDouble, 9.99999993e+36);
    sfconcVar.putVar(wacommAdapter->Sfconc()());

    netCDF::NcVar predVar = dataFile.addVar("class_predict", netCDF::ncDouble, timeLatLon);
    predVar.putAtt("description","predicted class of concentration of pollutants in mussels");
    predVar.putAtt("long_name","class_predict");
    predVar.putAtt("_FillValue", netCDF::ncDouble, 100);
    predVar.putVar(predictions());
}