You will need to install NVIDIA CUDA toolkit (CUDA 5.0 Production) for Linux to build and run
this sample application.

Once the CUDA toolkit is installed, update environment variable PATH to
correctly point to a location where CUDA binaries are installed.
    e.g. export PATH=$PATH:/usr/local/cuda/bin

The application will not run unless it can find a valid license. This 
dependence on a binary license file is limited to this sample application.
Nvidia will provide the GUID to be used as the license key on acquiring the
usage license.

To run the application ensure CUDA library path is included in the
environment variable LD_LIBRARY_PATH. e.g.

For 32-bit system:
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/cuda/lib
For 64-bit system:
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/cuda/lib64

