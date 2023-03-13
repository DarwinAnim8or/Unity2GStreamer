#include "StreamDataManager.h"

void StreamDataManager::Initialize() {
}

void StreamDataManager::Cleanup() {
    for (auto it : m_Streams) {
        if (it.second.data)
            delete[] it.second.data;
    }
}

void StreamDataManager::AddStream(unsigned int id) {
    m_Streams.insert(std::make_pair(id, StreamData()));
}

void StreamDataManager::StopStream(unsigned int id) {
    m_Streams.erase(id);
}

void StreamDataManager::SetDataForStream(unsigned int id, StreamData data) {
    auto it = m_Streams.find(id);
    if (it != m_Streams.end()) {
        if (it->second.data) delete[] it->second.data; //delete existing image if any
        it->second = data;
    }
}

const StreamData* StreamDataManager::GetDataForStream(unsigned int id) const {
    auto it = m_Streams.find(id);
    if (it != m_Streams.cend()) {
        return &it->second;
    }

    return nullptr;
}
