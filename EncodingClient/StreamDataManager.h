#pragma once

#include <map>
#include "Singleton.h"

struct StreamData {
	unsigned int size;
	unsigned char* data;
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

