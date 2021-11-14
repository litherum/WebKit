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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "GPUIdentifier.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamServerConnection.h"
#include <WebCore/ProcessIdentifier.h>
#include <pal/graphics/WebGPU/WebGPU.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadAssertions.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class GPUConnectionToWebProcess;

class RemoteGPU final : public IPC::StreamMessageReceiver {
public:
    static Ref<RemoteGPU> create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, WebKit::GPUIdentifier gpuIdentifier, IPC::StreamConnectionBuffer&& stream)
    {
        return adoptRef(*new RemoteGPU(gpuConnectionToWebProcess, gpuIdentifier, WTFMove(stream)));
    }

    virtual ~RemoteGPU();

    void stopListeningForIPC(Ref<RemoteGPU>&& refFromConnection);

    IPC::StreamConnectionWorkQueue& remoteGPUStreamWorkQueue() { return m_remoteGPUStreamWorkQueue; }

private:
    RemoteGPU(GPUConnectionToWebProcess&, WebKit::GPUIdentifier, IPC::StreamConnectionBuffer&&);

    void initialize();
    void workQueueInitialize();
    void workQueueUninitialize();

    // IPC::StreamMessageReceiver
    void didReceiveStreamMessage(IPC::StreamServerConnectionBase&, IPC::Decoder&) final;

    template<typename T>
    bool send(T&& message) const { return m_streamConnection->connection().send(WTFMove(message), m_gpuIdentifier); }

    // Messages to be received.
    void requestAdapter(CompletionHandler<void()>&&);

    IPC::StreamConnectionWorkQueue m_remoteGPUStreamWorkQueue;
    WeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    RefPtr<IPC::StreamServerConnection> m_streamConnection;
    RefPtr<PAL::WebGPU::GPU> m_gpu WTF_GUARDED_BY_LOCK(m_streamThread);
    GPUIdentifier m_gpuIdentifier;
    NO_UNIQUE_ADDRESS ThreadAssertion m_streamThread;
    WebCore::ProcessIdentifier m_webProcessIdentifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
