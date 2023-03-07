#This script will compile RakNet, DummyServer and the Streaming client.
cd 3rdparty/RakNet
mkdir build
cd build
cmake ..
make -j2
cd ..
cd ..
cd ..
echo "Built RakNet"
#Compile DummyServer
cd DummyServer
mkdir build
cd build
cmake ..
make
cd ..
cd ..
echo "Built DummyServer"
#Compile client
cd U2G_Client
mkdir build
cd build
cmake ..
make
cd ..
cd ..
echo "Client has been built!"
