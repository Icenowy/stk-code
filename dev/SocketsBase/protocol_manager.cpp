#include "protocol_manager.hpp"

#include "protocol.hpp"

#include <assert.h>
#include <stdio.h>
#include <cstdlib>

#define RAND_MAX 65536

ProtocolManager::ProtocolManager() 
{
    pthread_mutex_init(&m_messagesMutex, NULL);
    pthread_mutex_init(&m_protocolsMutex, NULL);
}

ProtocolManager::~ProtocolManager()
{
}

void ProtocolManager::notifyEvent(Event* event)
{
    if (event->type == EVENT_TYPE_MESSAGE)
    {
        pthread_mutex_lock(&m_messagesMutex);
        m_messagesToProcess.push_back(event->data); 
        pthread_mutex_unlock(&m_messagesMutex);
    }
}

void ProtocolManager::sendMessage(std::string message)
{
    std::string newMessage = " " + message; // add one byte
    newMessage[0] = (char)(0);
}

int ProtocolManager::startProtocol(Protocol* protocol)
{
    ProtocolInfo protocolInfo;
    protocolInfo.state = PROTOCOL_STATE_RUNNING;
    assignProtocolId(protocolInfo);
    protocolInfo.protocol = protocol;
    printf("__ProtocolManager> A new protocol with id=%u has been started. There are %ld protocols running.\n", protocolInfo.id, m_protocols.size()+1);
    
    pthread_mutex_lock(&m_protocolsMutex);
    m_protocols.push_back(protocolInfo);
    pthread_mutex_unlock(&m_protocolsMutex);
    
    protocol->setListener(this);
    protocol->setup();
    
    return protocolInfo.id;
}
void ProtocolManager::stopProtocol(Protocol* protocol)
{
    
}
void ProtocolManager::pauseProtocol(Protocol* protocol)
{
    for (unsigned int i = 0; i < m_protocols.size(); i++)
    {
        if (m_protocols[i].protocol == protocol && m_protocols[i].state == PROTOCOL_STATE_RUNNING)
        {
            pthread_mutex_lock(&m_protocolsMutex);
            m_protocols[i].state = PROTOCOL_STATE_PAUSED;
            m_protocols[i].protocol->pause();
            pthread_mutex_unlock(&m_protocolsMutex);
        }
    }
}
void ProtocolManager::unpauseProtocol(Protocol* protocol)
{
    for (unsigned int i = 0; i < m_protocols.size(); i++)
    {
        if (m_protocols[i].protocol == protocol && m_protocols[i].state == PROTOCOL_STATE_PAUSED)
        {
            pthread_mutex_lock(&m_protocolsMutex);
            m_protocols[i].state = PROTOCOL_STATE_RUNNING;
            m_protocols[i].protocol->unpause();
            pthread_mutex_unlock(&m_protocolsMutex);
        }
    }
}
void ProtocolManager::protocolTerminated(Protocol* protocol)
{
    int offset = 0;
    for (unsigned int i = 0; i < m_protocols.size(); i++)
    {
        if (m_protocols[i-offset].protocol == protocol)
        {
            pthread_mutex_lock(&m_protocolsMutex);
            delete m_protocols[i].protocol;
            m_protocols.erase(m_protocols.begin()+(i-offset), m_protocols.begin()+(i-offset)+1);
            pthread_mutex_unlock(&m_protocolsMutex);
            offset++;
        }
    }
    printf("__ProtocolManager> A protocol has been terminated. There are %ld protocols running.\n", m_protocols.size());
}

void ProtocolManager::update()
{
    // before updating, notice protocols that they have received information
    pthread_mutex_lock(&m_messagesMutex); // secure threads
    int size = m_messagesToProcess.size();
    for (int i = 0; i < size; i++)
    {
        std::string data = m_messagesToProcess.back();
        PROTOCOL_TYPE searchedProtocol = (PROTOCOL_TYPE)(data[0]);
        for (unsigned int i = 0; i < m_protocols.size() ; i++)
        {
            if (m_protocols[i].protocol->getProtocolType() == searchedProtocol) // pass data to them even when paused
                m_protocols[i].protocol->messageReceived((uint8_t*)(&data[1]));
        }
        m_messagesToProcess.pop_back();
    }
    pthread_mutex_unlock(&m_messagesMutex); // release the mutex
    // now update all protocols
    for (unsigned int i = 0; i < m_protocols.size(); i++)
    {
        if (m_protocols[i].state == PROTOCOL_STATE_RUNNING)
            m_protocols[i].protocol->update();
    }
}

int ProtocolManager::runningProtocolsCount()
{
    return m_protocols.size();
}

PROTOCOL_STATE ProtocolManager::getProtocolState(uint32_t id)
{
    for (unsigned int i = 0; i < m_protocols.size(); i++)
    {
        if (m_protocols[i].id == id)
        {
            PROTOCOL_STATE state = m_protocols[i].state;
            return state;
        }
    }
    return PROTOCOL_STATE_TERMINATED;
}

void ProtocolManager::assignProtocolId(ProtocolInfo& protocolInfo)
{
    uint32_t newId;
    bool exists;
    do
    {
        newId = (rand()<<16)+rand();
        exists = false;
        for (unsigned int i = 0; i < m_protocols.size(); i++)
        {
            pthread_mutex_lock(&m_protocolsMutex);
            if (m_protocols[i].id == newId)
                exists = true;
            pthread_mutex_unlock(&m_protocolsMutex);
        }
    } while (exists);
    protocolInfo.id = newId;
}


