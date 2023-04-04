#include <iostream>
#include "CCTVServer.h"
#include "RakSleep.h"
#include "snappy-c.h"
#include <chrono>

static CCTVServer* g_Server = nullptr;

int32_t GetMaxCompressedLength(int32_t nLenSrc) {
	int32_t n16kBlocks = (nLenSrc + 16383) / 16384; // round up any fraction of a block
	return (nLenSrc + 6 + (n16kBlocks * 5));
}

int32_t Compress(const uint8_t* abSrc, int32_t nLenSrc, uint8_t* abDst, size_t* nLenDst) {
	return snappy_compress((const char*)abSrc, nLenSrc, (char*)abDst, nLenDst);
}

int main() {
	//Print a big fat warning:
	std::cout << "######  ####### ######  #     #  #####     ####### #     # #       #     #" << std::endl;
	std::cout << "#     # #       #     # #     # #     #    #     # ##    # #        #   #  " << std::endl;
	std::cout << "#     # #       #     # #     # #          #     # # #   # #         # #   " << std::endl;
	std::cout << "#     # #####   ######  #     # #  ####    #     # #  #  # #          #    " << std::endl;
	std::cout << "#     # #       #     # #     # #     #    #     # #   # # #          #    " << std::endl;
	std::cout << "#     # #       #     # #     # #     #    #     # #    ## #          #    " << std::endl;
	std::cout << "######  ####### ######   #####   #####     ####### #     # #######    #   " << std::endl << std::endl;

	std::cout << "THIS SERVER IS ONLY MEANT FOR DEBUGGING PURPOSES. IT WILL ONLY SEND *GENERATED* IMAGE DATA." << std::endl;

	//Create server:
	g_Server = new CCTVServer(3001);

	while (true) {
		g_Server->Update();

		int width = 800;
		int height = 600;
		auto image = g_Server->GenerateRandomRGBAImage(width, height);

		auto t1 = std::chrono::high_resolution_clock::now();

		//compress our random image:
		size_t maxSize = snappy_max_compressed_length(image.size);
		uint8_t* compressed = new uint8_t[maxSize];
		Compress((const uint8_t*)image.data.c_str(), image.size, compressed, &maxSize);
		size_t cSize = maxSize;
		
		auto t2 = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

		std::cout << "Size: " << image.size << " -> " << cSize << "; took " << duration << "ms" << std::endl;

		g_Server->SendNewFrameToEveryone((unsigned char*)image.data.c_str(), image.size, width, height, 1);
		//delete[] image.data;
		delete[] compressed;

		RakSleep(30);
	}

	return 0;
}