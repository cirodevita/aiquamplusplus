//
// Created by Ciro De Vita on 29/01/24.
//

#include "AiquammPlusPlus.hpp"

AiquamPlusPlus::~AiquamPlusPlus() = default;

AiquamPlusPlus::AiquamPlusPlus(std::shared_ptr<Config> config): config(config) {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Aiquam"));
}

void AiquamPlusPlus::run() {
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

    // for (auto &ncInput : config->NcInputs()) {
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

    // TODO: add MPI and OpenMP to apply inference
    for (int idx = 0; idx < send_counts[world_rank]; idx++) {
        int global_idx = displs[world_rank] + idx;
        std::vector<float> input_data(ncInputs);

        for (int file_idx = 0; file_idx < ncInputs; file_idx++) {
            input_data[file_idx] = timeseries[global_idx * ncInputs + file_idx];
        }

        LOG4CPLUS_INFO(logger, "Idx: " + std::to_string(input_data.size()));

        break;
    }

    /*
    std::vector<string> model_paths = {
        "/home/hpsc-simulator/aiquamplusplus/checkpoints/AIQUAM_CNN/model.onnx",
        "/home/hpsc-simulator/aiquamplusplus/checkpoints/AIQUAM_DLinear/model.onnx",
        // "/home/hpsc-simulator/aiquamplusplus/checkpoints/AIQUAM_Informer/model.onnx",
        "/home/hpsc-simulator/aiquamplusplus/checkpoints/AIQUAM_Reformer/model.onnx",
        "/home/hpsc-simulator/aiquamplusplus/checkpoints/AIQUAM_TimesNet/model.onnx",
        "/home/hpsc-simulator/aiquamplusplus/checkpoints/AIQUAM_Transformer/model.onnx",
        "/home/hpsc-simulator/aiquamplusplus/checkpoints/AIQUAM_KNN/model.onnx"
    };

    Ort::Env env;
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

    std::vector<int64_t> predictions;

    for (string& model_path : model_paths) {
        LOG4CPLUS_INFO(logger, "Running inference with model: " + model_path);

        const char* model_path_cstr = model_path.c_str();
        Ort::Session session(env, model_path_cstr, session_options);

        const char* input_name = "input";
        const char* output_name;

        std::vector<float> input_array = this->input_list;
        std::vector<int64_t> input_shape;
        if (model_path.find("KNN") != std::string::npos) {
            input_shape = {1, static_cast<int64_t>(input_list.size())};
            output_name = "output_label";
        } else {
            input_shape = {1, static_cast<int64_t>(input_list.size()), 1};
            output_name = "output";
        }

        size_t input_tensor_size = input_list.size();
        std::vector<float> input_tensor_values(input_tensor_size);
        std::copy(input_array.begin(), input_array.end(), input_tensor_values.begin());

        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(memory_info, input_tensor_values.data(), input_tensor_size, input_shape.data(), input_shape.size());

        auto output_tensors = session.Run(Ort::RunOptions{nullptr}, &input_name, &input_tensor, 1, &output_name, 1);

        if (model_path.find("KNN") != std::string::npos) {
            Ort::Value& output_tensor = output_tensors.front();
            int64_t* output_data = output_tensor.GetTensorMutableData<int64_t>();
            predictions.push_back(output_data[0]);
        } else {
            float* float_array = output_tensors.front().GetTensorMutableData<float>();
            std::vector<float> output_vector(float_array, float_array + input_tensor_size);
            auto max_element_iter = std::max_element(output_vector.begin(), output_vector.end());
            int64_t predicted_class = std::distance(output_vector.begin(), max_element_iter);
            predictions.push_back(predicted_class);
        }

        std::cout << "Predicted class: " << predictions.back() << std::endl;
    }

    std::vector<int64_t> counts(4, 0);  // Supponendo 10 classi come esempio
    for (const auto& pred : predictions) {
        counts[pred]++;
    }
    int64_t final_prediction = std::max_element(counts.begin(), counts.end()) - counts.begin();

    std::cout << "Final prediction (majority vote): " << final_prediction << std::endl;
    */
}