#include "StreamDataManager.h"
#include <iostream>

StreamData globalDataBcFuckIt;

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
    if (data.data == nullptr) return;
    if (globalDataBcFuckIt.data) delete[] globalDataBcFuckIt.data;
    globalDataBcFuckIt = data;
    
    std::cout << data.data[0] << std::endl;

    return;

    auto it = m_Streams.find(id);
    if (it != m_Streams.end()) {
        if (it->second.data) delete[] it->second.data; //delete existing image if any
        it->second = data;
    }
    else {
        m_Streams.insert(std::make_pair(id, data));
    }
}

const StreamData* StreamDataManager::GetDataForStream(unsigned int id) const {
    return &globalDataBcFuckIt;
    auto it = m_Streams.find(id);
    if (it != m_Streams.cend()) {
        return &it->second;
    }

    return nullptr;
}
