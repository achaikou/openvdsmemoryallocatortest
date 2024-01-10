#include <OpenVDS/IJKCoordinateTransformer.h>
#include <OpenVDS/OpenVDS.h>
#include <OpenVDS/VolumeDataAccess.h>
#include <OpenVDS/VolumeDataLayout.h>

#include <chrono>
#include <iostream>

#include "oneapi/tbb.h"

using namespace oneapi::tbb;

/*--------------------Some request to openvds----------------------------------*/

void send_request(const OpenVDS::VDSHandle& handle, int iline_min, int iline_max, int xline_min, int xline_max, int depth_min, int depth_max) {

    OpenVDS::VolumeDataAccessManager accessManager = OpenVDS::GetAccessManager(handle);
    OpenVDS::VolumeDataLayout const* layout = accessManager.GetVolumeDataLayout();
    OpenVDS::IJKCoordinateTransformer ijkCoordinateTransformer = OpenVDS::IJKCoordinateTransformer(layout);

    int sampleCount0 = layout->GetDimensionNumSamples(0);

    int lower[OpenVDS::VolumeDataLayout::Dimensionality_Max]{depth_min, xline_min, iline_min, 0, 0, 0};
    int upper[OpenVDS::VolumeDataLayout::Dimensionality_Max]{depth_max, xline_max, iline_max, 1, 1, 1};

    std::int64_t size = accessManager.GetVolumeSubsetBufferSize(
        lower,
        upper,
        OpenVDS::VolumeDataFormat::Format_R32,
        0,
        0
    );
    std::vector<float> buffer(size / 4); // make this vector with allocator too?

    auto request = accessManager.RequestVolumeSubset(
        buffer.data(),
        size,
        OpenVDS::Dimensions_012,
        0,
        0,
        lower,
        upper,
        OpenVDS::VolumeDataFormat::Format_R32
    );
    request.get()->WaitForCompletion();
}

/*--------------------No additional concurrency----------------------------------*/

void noconcurrency(int nparts, std::string url, std::string connectionString, int iline_min, int iline_max, int xline_min, int xline_max, int depth_min, int depth_max) {
    std::cout << "One handle, no non-openvds concurrency" << std::endl;
    std::cout << "Iline min " << iline_min << ", iline max " << iline_max << std::endl;

    OpenVDS::Error error;
    OpenVDS::VDSHandle handle = OpenVDS::Open(url, connectionString, error);
    if (error.code != 0) {
        throw std::runtime_error("Could not open VDS: " + error.string);
    }
    send_request(handle, iline_min, iline_max, xline_min, xline_max, depth_min, depth_max);
}

/*--------------------Multiple threads, one handle----------------------------------*/

class OneHandleTBBRequest {
    const OpenVDS::VDSHandle& handle;
    int xline_min;
    int xline_max;
    int depth_min;
    int depth_max;

public:
    void operator()(const blocked_range<size_t>& iline_range) const {
        std::chrono::steady_clock::time_point before_call = std::chrono::steady_clock::now();

        send_request(handle, iline_range.begin(), iline_range.end(), xline_min, xline_max, depth_min, depth_max);

        std::chrono::steady_clock::time_point after_call = std::chrono::steady_clock::now();
        std::cout << "Thread (iline min " << iline_range.begin() << ", iline max " << iline_range.end() << ") finished in "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(after_call - before_call).count() << "[ms]" << std::endl;
    }
    OneHandleTBBRequest(
        const OpenVDS::VDSHandle& handle, int xline_min, int xline_max, int depth_min, int depth_max
    ) : handle(handle), xline_min(xline_min), xline_max(xline_max), depth_min(depth_min), depth_max(depth_max) {
    }
};

void onehandle(int nparts, std::string url, std::string connectionString, int iline_min, int iline_max, int xline_min, int xline_max, int depth_min, int depth_max) {
    std::cout << "One handle, approx. " << nparts << " threads used" << std::endl;

    OpenVDS::Error error;
    OpenVDS::VDSHandle handle = OpenVDS::Open(url, connectionString, error);
    if (error.code != 0) {
        throw std::runtime_error("Could not open VDS: " + error.string);
    }

    int chunk_size = (iline_max - iline_min) / nparts;

    parallel_for(blocked_range<size_t>(iline_min, iline_max, chunk_size), OneHandleTBBRequest(handle, xline_min, xline_max, depth_min, depth_max), simple_partitioner());
}

/*--------------------Multiple threads, many handles----------------------------------*/

class ManyHandlesTBBRequest {
    std::string url;
    std::string connectionString;
    int xline_min;
    int xline_max;
    int depth_min;
    int depth_max;

public:
    void operator()(const blocked_range<size_t>& iline_range) const {
        std::chrono::steady_clock::time_point before_call = std::chrono::steady_clock::now();

        OpenVDS::Error error;
        OpenVDS::VDSHandle handle = OpenVDS::Open(url, connectionString, error);
        if (error.code != 0) {
            throw std::runtime_error("Could not open VDS: " + error.string);
        }
        send_request(handle, iline_range.begin(), iline_range.end(), xline_min, xline_max, depth_min, depth_max);

        std::chrono::steady_clock::time_point after_call = std::chrono::steady_clock::now();
        std::cout << "Thread (iline min " << iline_range.begin() << ", iline max " << iline_range.end() << ") finished in "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(after_call - before_call).count() << "[ms]" << std::endl;
    }
    ManyHandlesTBBRequest(
        std::string url, std::string connectionString, int xline_min, int xline_max, int depth_min, int depth_max
    ) : url(url), connectionString(connectionString), xline_min(xline_min), xline_max(xline_max), depth_min(depth_min), depth_max(depth_max) {
    }
};

void manyhandles(int nparts, std::string url, std::string connectionString, int iline_min, int iline_max, int xline_min, int xline_max, int depth_min, int depth_max) {
    std::cout << "One handle per thread, approx. " << nparts << " threads used" << std::endl;
    int chunk_size = (iline_max - iline_min) / nparts;

    parallel_for(blocked_range<size_t>(iline_min, iline_max, chunk_size), ManyHandlesTBBRequest(url, connectionString, xline_min, xline_max, depth_min, depth_max), simple_partitioner());
}

/*--------------------Wrappers----------------------------------*/

void measure(
    std::function<void(int, std::string, std::string, int, int, int, int, int, int)> func, int nparts,
    std::string url, std::string connectionString, int iline_min, int iline_max, int xline_min, int xline_max, int depth_min, int depth_max
) {
    std::chrono::steady_clock::time_point before_call = std::chrono::steady_clock::now();
    func(nparts, url, connectionString, iline_min, iline_max, xline_min, xline_max, depth_min, depth_max);
    std::chrono::steady_clock::time_point after_call = std::chrono::steady_clock::now();

    std::cout << "Call took = " << std::chrono::duration_cast<std::chrono::milliseconds>(after_call - before_call).count() << "[ms]" << std::endl;
    std::cout << "\n"
              << std::flush;
}

std::string account = "";
std::string container = "";
std::string vds = "";
std::string sas = "";

std::string url = "azure://" + container + "/" + vds;
std::string connectionString = "BlobEndpoint=https://" + account + ".blob.core.windows.net;SharedAccessSignature=?" + sas;

// could be fetched through metadata. Maybe do it in warm-up?
int iline_min = 0;
int iline_max = 400;
int xline_min = 0;
int xline_max = 300;
int depth_min = 60;
int depth_max = 70;

// before measurements: assure Azure is warmed up
int main(int argc, char* argv[]) {

    if (argc > 1) {
        int iline_min = std::atoi(argv[1]);
        int iline_max = std::atoi(argv[2]);
        std::cout << "Starting process with iline_min " << iline_min << ", iline max " << iline_max << std::endl;
        measure(noconcurrency, -1, url, connectionString, iline_min, iline_max, xline_min, xline_max, depth_min, depth_max);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } else {
        int sleep_seconds = 5;
        int threads = 8;
        measure(noconcurrency, -1, url, connectionString, iline_min, iline_max, xline_min, xline_max, depth_min, depth_max);
        std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));
        measure(onehandle, 1, url, connectionString, iline_min, iline_max, xline_min, xline_max, depth_min, depth_max);
        std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));
        measure(onehandle, threads, url, connectionString, iline_min, iline_max, xline_min, xline_max, depth_min, depth_max);
        std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));
        measure(manyhandles, threads, url, connectionString, iline_min, iline_max, xline_min, xline_max, depth_min, depth_max);
        std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));
    }

    return 0;
}
