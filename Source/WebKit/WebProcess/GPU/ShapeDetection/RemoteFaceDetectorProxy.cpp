/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "RemoteFaceDetectorProxy.h"

#if ENABLE(GPU_PROCESS)

#include "MessageSenderInlines.h"
#include "RemoteFaceDetectorMessages.h"
#include "RemoteRenderingBackendProxy.h"
#include "WebProcess.h"

namespace WebKit::ShapeDetection {

Ref<RemoteFaceDetectorProxy> RemoteFaceDetectorProxy::create(RemoteRenderingBackendProxy& remoteRenderingBackendProxy, ShapeDetectionIdentifier identifier, const WebCore::ShapeDetection::FaceDetectorOptions& faceDetectorOptions)
{
    remoteRenderingBackendProxy.streamConnection().send(Messages::RemoteRenderingBackend::CreateRemoteFaceDetector(identifier, faceDetectorOptions), remoteRenderingBackendProxy.renderingBackendIdentifier(), Seconds::infinity());
    return adoptRef(*new RemoteFaceDetectorProxy(remoteRenderingBackendProxy, identifier));
}

RemoteFaceDetectorProxy::RemoteFaceDetectorProxy(RemoteRenderingBackendProxy& remoteRenderingBackendProxy, ShapeDetectionIdentifier identifier)
    : m_backing(identifier)
    , m_remoteRenderingBackendProxy(remoteRenderingBackendProxy)
{
}

RemoteFaceDetectorProxy::~RemoteFaceDetectorProxy()
{
    if (!m_remoteRenderingBackendProxy)
        return;

    m_remoteRenderingBackendProxy->streamConnection().send(Messages::RemoteRenderingBackend::ReleaseRemoteFaceDetector(m_backing), m_remoteRenderingBackendProxy->renderingBackendIdentifier(), Seconds::infinity());
}

void RemoteFaceDetectorProxy::detect(CompletionHandler<void(Vector<WebCore::ShapeDetection::DetectedFace>&&)>&& completionHandler)
{
    if (!m_remoteRenderingBackendProxy) {
        completionHandler({ });
        return;
    }

    m_remoteRenderingBackendProxy->streamConnection().sendWithAsyncReply(Messages::RemoteFaceDetector::Detect(), WTFMove(completionHandler), m_backing, Seconds::infinity());
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
