#pragma once

#include <map>
#include "Singleton.h"

#include "BitStream.h"

struct StreamData {
	unsigned int size;
	unsigned char* data;
	unsigned int width;
	unsigned int height;

	void Deserialize(RakNet::BitStream& bitStream) {
		bitStream.Read(width);
		bitStream.Read(height);
		bitStream.Read(size);

		data = new unsigned char[size];

		for (unsigned int i = 0; i < size; i++) {
			unsigned char temp;
			bitStream.Read(temp);
			data[i] = temp;
		}
	}
};

class StreamDataManager : public Singleton<StreamDataManager> {
public:
	void Initialize();
	void Cleanup();

	void AddStream(unsigned int id);
	void StopStream(unsigned int id);

	void SetDataForStream(unsigned int id, StreamData data);
	const StreamData* GetDataForStream(unsigned int id) const;

private:
	std::map<unsigned int, StreamData> m_Streams;
};

