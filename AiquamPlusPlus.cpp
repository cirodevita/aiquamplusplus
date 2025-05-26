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
    size_t total_size = sizeof(data.i) + sizeof(data.j) + sizeof(size_t) + data.values.size() * sizeof(float) + sizeof(data.prediction);
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
    ptr += vec_size * sizeof(float);

    memcpy(ptr, &data.prediction, sizeof(data.prediction));
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
    ptr += vec_size * sizeof(float);

    memcpy(&data.prediction, ptr, sizeof(data.prediction));
}

void AiquamPlusPlus::run() {
    int ompMaxThreads=1, ompThreadNum=0, world_size=1, world_rank=0, nAreas=0, num_gpus=0;
    int ncInputs = config->NcInputs().size();

#ifdef USE_OMP
    // Get the number of threads available for computation
    ompMaxThreads=omp_get_max_threads();
#endif

#ifdef USE_MPI
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
#endif

#ifdef USE_CUDA
    // Get the number of available GPU devices
    cudaError_t err = cudaGetDeviceCount(&num_gpus);

    // Check if a GPU is available
    if (num_gpus <= 0 || err != cudaSuccess) {

        // If a GPU is not present, or a problem occurred, set the number of GPUs as 0;
        num_gpus = 0;
    }
#endif

    LOG4CPLUS_DEBUG(logger, "num_gpus: " << num_gpus);

    // Define a vector of integers hosting the number of areas for each processor
    std::unique_ptr<int[]> send_counts = std::make_unique<int[]>(world_size);

    // Define a vector of integers hosting the displacement 0
    std::unique_ptr<int[]> displs = std::make_unique<int[]>(world_size);

    // Define the sending buffer
    std::vector<char> sendbuf;

    // Define the receiving buffer
    std::vector<char> recvbuf;

    Areas *pLocalAreas;

    shared_ptr<WacommAdapter> wacommAdapter;

    Array::Array3<double> predictions;

    size_t serialized_size = sizeof(double) * 2 + sizeof(size_t) + ncInputs * sizeof(float) + sizeof(int);

    for (int fileIdx = 0; fileIdx < ncInputs; ++fileIdx) {
        std::string& ncInput = config->NcInputs()[fileIdx];

        if (world_rank == 0) {
            LOG4CPLUS_INFO(logger, world_rank << ": Input from Ocean Model: " << ncInput);

            wacommAdapter = make_shared<WacommAdapter>(ncInput);
            wacommAdapter->process();

            if (fileIdx == 0) {
                wacommAdapter->initializeKDTree();
                string fileName = config->AreasFile();

                if (fileName.substr(fileName.find_last_of('.') + 1) == "json") {
                    areas->loadFromJson(fileName, wacommAdapter);
                } else if (fileName.substr(fileName.find_last_of('.') + 1) == "shp"){
                    areas->loadFromShp(fileName, wacommAdapter);
                }
                nAreas = areas->size();
                LOG4CPLUS_INFO(logger, "nAreas: " << nAreas);

                // Calculate the number of areas for each process
                size_t areasPerProcess = nAreas / world_size;
                size_t spare = nAreas % world_size;

                // Calculate send counts and displacements
                for (int i = 0; i < world_size; i++) {
                    send_counts[i] = areasPerProcess + (i < spare ? 1 : 0);
                    displs[i] = (i > 0) ? (displs[i - 1] + send_counts[i - 1]) : 0;
                    send_counts[i] *= serialized_size;

                    LOG4CPLUS_DEBUG(logger, world_rank << ": send_counts[0]=" << send_counts.get()[0] << " displ[0]=" << displs.get()[0]);
                }

                size_t time = wacommAdapter->Conc().Nx();
                size_t lat = wacommAdapter->Conc().Nz();
                size_t lon = wacommAdapter->Conc().N4();

                // Define the final predictions matrix
                predictions.Allocate(time, lat, lon);
                
                // Set the predictions matrix to 0
                #pragma omp parallel for collapse(3) default(none) shared(time, lat, lon, predictions)
                for (int t=0; t<time; t++) {
                    for (int j=0; j<lat; j++) {
                        for (int i=0; i<lon; i++) {
                            predictions(t,j,i)=9.99999993e+36;
                        }
                    }
                }
            }

            // Serialize the data
            sendbuf.clear();
            for (int idx = 0; idx < nAreas; idx++) {
                Area& area = areas->at(idx);
                area.addValue(wacommAdapter->calculateConc(area.J(), area.I()));

                area_data data;
                data.i = area.I();
                data.j = area.J();
                data.values = area.Values();
                data.prediction = -1;

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
    std::unique_ptr<Areas>  localAreas = std::make_unique<Areas>();
    int num_local_areas = send_counts[world_rank] / serialized_size;
    size_t offset = 0;
    for (int i = 0; i < num_local_areas; i++) {
        std::vector<char> buffer(recvbuf.begin() + offset, recvbuf.begin() + offset + serialized_size);

        area_data local_area;
        deserialize(buffer, local_area);

        Area area(local_area);
        localAreas->push_back(area);

        offset += serialized_size;
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

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
    double comp_t0 = MPI_Wtime();
#else
    auto comp_t0 = std::chrono::high_resolution_clock::now();
#endif

    #pragma omp parallel default(none) private(ompThreadNum) shared(world_rank, thread_counts, thread_displs, pLocalAreas, areasPerThread, num_gpus, serialized_size, recvbuf)
    {
#ifdef USE_CUDA
        Aiquam aiquam(config, (world_rank+ompThreadNum)%num_gpus);
#else
        Aiquam aiquam(config);
#endif

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

            //std::vector<float> values = {494.79931640625,504.8264465332031,454.9320983886719,397.55279541015625,341.72088623046875,349.1586303710937,348.7724914550781,324.2787780761719,333.1670227050781,362.272216796875,396.6249084472656,430.06976318359375,484.9303894042969,453.0697021484375,477.3464965820313,434.7988586425781,531.5578002929688,344.17449951171875,225.81866455078125,314.5093078613281,322.84539794921875,301.417236328125,309.225830078125,282.3187866210937,268.01959228515625,289.224365234375,312.3394775390625,275.701904296875,247.72433471679688,233.9879608154297,235.0616607666016,181.0559539794922,201.79379272460935,225.74411010742188,247.97442626953125,247.18331909179688,274.9723205566406,280.5320739746094,268.0481262207031,194.05596923828125,224.49575805664065,137.5928955078125,101.20760345458984,231.069580078125,375.4364318847656,407.2272644042969,442.3384094238281,413.7304382324219,393.2890625,433.6060791015625,470.7800903320313,514.7785034179688,556.407470703125,598.1444091796875,575.6135864257812,438.1578979492188,337.7201538085937,336.3081970214844,329.26397705078125,323.6751708984375,329.8407592773437,328.40216064453125,327.1003723144531,270.5162658691406,240.3106689453125,253.34133911132807,212.14730834960935,300.3067626953125,396.8921813964844,463.1541137695313,539.9480590820312,608.8101196289062,591.9304809570312};
            std::vector<float> values = pLocalAreas->at(idx).data().values;
            int predicted_class = aiquam.inference(values);
            pLocalAreas->at(idx).Prediction(predicted_class);

#ifdef USE_MPI
            std::vector<char> updated_buffer;
            serialize(pLocalAreas->at(idx).data(), updated_buffer);            
            size_t offset = idx * serialized_size;
            std::copy(updated_buffer.begin(), updated_buffer.end(), recvbuf.begin() + offset);
#endif
            
            LOG4CPLUS_DEBUG(logger, world_rank << ": ompThreadNum: " << ompThreadNum << ": idx: " << idx << ": i:" << pLocalAreas->at(idx).data().i << ", j: " << pLocalAreas->at(idx).data().j << ", prediction: " << predicted_class << std::endl);
        }

#ifdef USE_OMP
        // Barrier to ensure all threads have finished processing
        #pragma omp barrier
#endif
    }

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
    double comp_t1 = MPI_Wtime();
    double comp_elapsed = comp_t1 - comp_t0;

    double comp_max;
    MPI_Reduce(&comp_elapsed, &comp_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if (world_rank == 0) {
        LOG4CPLUS_INFO(logger, "Compute time: " << comp_max << " s");
    }
#else
    auto comp_t1 = std::chrono::high_resolution_clock::now();
    double comp_elapsed = std::chrono::duration<double>(comp_t1 - comp_t0).count();
    LOG4CPLUS_INFO(logger, "Compute time: " << comp_elapsed << " s");
#endif

#ifdef USE_MPI
    std::vector<int> recv_sizes(world_size);
    MPI_Gather(&recv_count, 1, MPI_INT, recv_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (world_rank == 0) {
    int total_size = std::accumulate(recv_sizes.begin(), recv_sizes.end(), 0);
        sendbuf.resize(total_size);
    }

    MPI_Gatherv(recvbuf.data(), recv_count, MPI_CHAR, 
            sendbuf.data(), recv_sizes.data(), displs.get(), MPI_CHAR, 
            0, MPI_COMM_WORLD);

    if (world_rank == 0) {
        size_t offset = 0;
        for (int i = 0; i < world_size; ++i) {
            for (int j = 0; j < recv_sizes[i] / serialized_size; ++j) {
                std::vector<char> buffer(sendbuf.begin() + offset, sendbuf.begin() + offset + serialized_size);
                area_data area;
                deserialize(buffer, area);
                offset += serialized_size;

                predictions(0, area.j, area.i) = area.prediction;
            }
        }
    }
# else
    #pragma omp parallel default(none) private(ompThreadNum) shared(pLocalAreas)
    for (size_t idx = 0; idx < pLocalAreas->size(); idx++) {
        predictions(0, pLocalAreas->at(idx).J(), pLocalAreas->at(idx).I()) = pLocalAreas->at(idx).Prediction();
    }
#endif

    if (world_rank == 0) {
        // Create the output filename
        string ncOutputFilename=config->NcOutputRoot()+config->Date()+".nc";
        
        LOG4CPLUS_INFO(logger, "Saving output:" << ncOutputFilename);

        // Save the history
        save(ncOutputFilename, wacommAdapter, predictions);
    }
}

void AiquamPlusPlus::save(const string &fileName, shared_ptr<WacommAdapter> wacommAdapter, Array::Array3<double> &predictions) {
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
    concVar.setCompression(true, true, 4);
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
    sfconcVar.setCompression(true, true, 4);
    sfconcVar.putVar(wacommAdapter->Sfconc()());

    netCDF::NcVar predVar = dataFile.addVar("class_predict", netCDF::ncDouble, timeLatLon);
    predVar.putAtt("description","predicted class of concentration of pollutants in mussels");
    predVar.putAtt("units","1");
    predVar.putAtt("long_name","class_predict");
    predVar.putAtt("_FillValue", netCDF::ncDouble, 9.99999993e+36);
    predVar.setCompression(true, true, 4);
    predVar.putVar(predictions());
}