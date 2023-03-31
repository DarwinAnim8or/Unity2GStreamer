#pragma once

#include <map>
#include "Singleton.h"

#include "BitStream.h"
#include <mutex>

struct StreamData {
	unsigned int size;
	unsigned char* data = nullptr;
	unsigned int width;
	unsigned int height;
	unsigned int channelID;

	void Deserialize(RakNet::BitStream& bitStream) {
		bitStream.Read(width);
		bitStream.Read(height);
		bitStream.Read(channelID);
		bitStream.Read(size);

		if (data != nullptr) {
			delete[] data;
		}

		data = new unsigned char[size];

		/*for (unsigned int i = 0; i < size; i++) {
			unsigned char temp;
			bitStream.Read(temp);
			data[i] = temp;
		}*/

		bitStream.ReadAlignedBytes(data, size);
	}

	~StreamData() {
		delete[] data; //commento ut if using the std::mutex method instead
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

	bool IsTomFoolery() const { return m_RakNetTomfoolery; }
	void SetTomFoolery(bool value) { m_RakNetTomfoolery = value; }

	std::mutex mutex;

private:
	std::map<unsigned int, StreamData> m_Streams;
	bool m_RakNetTomfoolery = false;
	
};

