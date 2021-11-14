/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteGPU.h"

#include "GPUConnectionToWebProcess.h"
#include "RemoteGPUMessages.h"
#include "RemoteGPUProxyMessages.h"

#if ENABLE(GPU_PROCESS)

namespace WebKit {

RemoteGPU::RemoteGPU(GPUConnectionToWebProcess& gpuConnectionToWebProcess, WebKit::GPUIdentifier gpuIdentifier, IPC::StreamConnectionBuffer&& stream)
    : m_remoteGPUStreamWorkQueue("WebGPU")
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_streamConnection(IPC::StreamServerConnection::create(gpuConnectionToWebProcess.connection(), WTFMove(stream), m_remoteGPUStreamWorkQueue))
    , m_gpuIdentifier(gpuIdentifier)
    , m_webProcessIdentifier(gpuConnectionToWebProcess.webProcessIdentifier())
{
}

RemoteGPU::~RemoteGPU()
{

}

void RemoteGPU::stopListeningForIPC(Ref<RemoteGPU>&& refFromConnection)
{
    assertIsMainRunLoop();
    m_streamConnection->stopReceivingMessages(Messages::RemoteGPU::messageReceiverName(), m_gpuIdentifier.toUInt64());
    remoteGPUStreamWorkQueue().dispatch([protectedThis = WTFMove(refFromConnection)]() {
        protectedThis->workQueueUninitialize();
    });
}

void RemoteGPU::initialize()
{
    assertIsMainRunLoop();
    remoteGPUStreamWorkQueue().dispatch([protectedThis = Ref { *this }]() mutable {
        protectedThis->workQueueInitialize();
    });
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteGPU::messageReceiverName(), m_gpuIdentifier.toUInt64());
}

void RemoteGPU::workQueueInitialize()
{
    m_streamThread.reset();
    assertIsCurrent(m_streamThread);
    send(Messages::RemoteGPUProxy::WasCreated(remoteGPUStreamWorkQueue().wakeUpSemaphore()));
}

void RemoteGPU::workQueueUninitialize()
{
    assertIsCurrent(m_streamThread);
    m_streamConnection = nullptr;
}

void RemoteGPU::requestAdapter(CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(m_streamThread);
    m_gpu->requestAdapter({ }, [completionHandler = std::make_shared<CompletionHandler<void()>>(WTFMove(completionHandler))] (RefPtr<PAL::WebGPU::Adapter>&&) mutable {
        (*completionHandler)();
    });
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
