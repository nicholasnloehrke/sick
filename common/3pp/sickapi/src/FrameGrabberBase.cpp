//
// Copyright note: Redistribution and use in source, with or without modification, are permitted.
// 
// Created: November 2019
// 
// SICK AG, Waldkirch
// email: TechSupport0905@sick.de

#include "FrameGrabberBase.h"
#include <chrono>
#include <iostream>
#ifdef SICKAPI_USE_SPDLOG
#include <spdlog/spdlog.h>
#endif

namespace visionary
{
    FrameGrabberBase::FrameGrabberBase(const std::string& hostname, std::uint16_t port, std::uint64_t timeoutMs)
        : m_isRunning(false)
        , m_FrameAvailable(false)
	    , m_connected(false)
        , m_hostname(hostname)
        , m_port(port)
        , m_timeoutMs(timeoutMs)
    {
    }

    void FrameGrabberBase::start(std::shared_ptr<VisionaryData> inactiveDataHandler, std::shared_ptr<VisionaryData> activeDataHandler)
    {
        if(m_isRunning)
        {
#ifdef SICKAPI_USE_SPDLOG
            spdlog::get("sickapi")->info("FrameGrabberBase is already running");
#else
            std::cout << "FrameGrabberBase is already running\n";
#endif
            return;
        }
        m_isRunning = true;
        m_pDataStream = std::unique_ptr<VisionaryDataStream>(new VisionaryDataStream(std::move(activeDataHandler)));
        m_pDataHandler = std::move(inactiveDataHandler);
        m_connected = m_pDataStream->open(m_hostname, m_port, m_timeoutMs);
        if (!m_connected)
        {
#ifdef SICKAPI_USE_SPDLOG
            spdlog::get("sickapi")->error("Failed to connect");
#else
            std::cerr << "Failed to connect\n";
#endif
        }
        m_grabberThread = std::thread(&FrameGrabberBase::run, this);
    }

    FrameGrabberBase::~FrameGrabberBase()
    {
        m_isRunning = false;
        m_grabberThread.join();
    }

    void FrameGrabberBase::run()
    {
        while(m_isRunning)
        {
            if (!m_connected)
            {
                if (!m_pDataStream->open(m_hostname, m_port, m_timeoutMs))
                {
#ifdef SICKAPI_USE_SPDLOG
                    spdlog::get("sickapi")->error("Failed to connect");
#else
                    std::cerr << "Failed to connect\n";
#endif
                    m_connected = false;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
                m_connected = true;
            }
            if (m_pDataStream->getNextFrame())
            {
                {
                    std::lock_guard<std::mutex> guard(m_dataHandler_mutex);
                    m_FrameAvailable = true;
                    auto pOldDataHandler = std::move(m_pDataHandler);
                    m_pDataHandler = std::move(m_pDataStream->getDataHandler());
                    m_pDataStream->setDataHandler(pOldDataHandler);
                }
                m_frameAvailableCv.notify_one();
            }
            else
            {
                if(!m_pDataStream->isConnected())
                {
#ifdef SICKAPI_USE_SPDLOG
                    spdlog::get("sickapi")->error("Connection lost -> Reconnecting");
#else
                    std::cerr << "Connection lost -> Reconnecting\n";
#endif
                    m_pDataStream->close();
                    m_connected = m_pDataStream->open(m_hostname, m_port, m_timeoutMs);

                }
            }
        }
    }

    bool FrameGrabberBase::getNextFrame(std::shared_ptr<VisionaryData>& pDataHandler, std::uint64_t timeoutMs)
    {
        std::unique_lock<std::mutex> guard(m_dataHandler_mutex);
        m_FrameAvailable = false;
        m_frameAvailableCv.wait_for(guard, std::chrono::milliseconds(timeoutMs), [this] { return this->m_FrameAvailable;  });
        if (m_FrameAvailable)
        {
            m_FrameAvailable = false;
            const auto tmp = std::move(pDataHandler);
            pDataHandler = std::move(m_pDataHandler);
            m_pDataHandler = tmp;
            return true;
        }
        return false;
    }

    bool FrameGrabberBase::getCurrentFrame(std::shared_ptr<VisionaryData>& pDataHandler)
    {
        if(m_FrameAvailable)
        {
            std::lock_guard<std::mutex> guard(m_dataHandler_mutex);
            m_FrameAvailable = false;
            const auto tmp = std::move(pDataHandler);
            pDataHandler = std::move(m_pDataHandler);
            m_pDataHandler = tmp;
            return true;
        }
        return false;
    }
}
