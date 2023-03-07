#This script will compile RakNet, DummyServer and the Streaming client.
cd 3rdparty/RakNet
mkdir build
cd build
cmake ..
make -j2
cd ..
cd ..
cd ..
#Compile DummyServer
cd DummyServer
mkdir build
cd build
cmake ..
make
cd ..
cd ..
#Compile client
