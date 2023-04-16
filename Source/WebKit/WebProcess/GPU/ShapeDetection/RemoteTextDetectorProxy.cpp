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
#include "RemoteTextDetectorProxy.h"

#if ENABLE(GPU_PROCESS)

#include "MessageSenderInlines.h"
#include "RemoteRenderingBackendProxy.h"
#include "RemoteTextDetectorMessages.h"
#include "WebProcess.h"
#include <WebCore/ImageBuffer.h>

namespace WebKit::ShapeDetection {

Ref<RemoteTextDetectorProxy> RemoteTextDetectorProxy::create(RemoteRenderingBackendProxy& remoteRenderingBackendProxy, ShapeDetectionIdentifier identifier)
{
    remoteRenderingBackendProxy.streamConnection().send(Messages::RemoteRenderingBackend::CreateRemoteTextDetector(identifier), remoteRenderingBackendProxy.renderingBackendIdentifier(), Seconds::infinity());
    return adoptRef(*new RemoteTextDetectorProxy(remoteRenderingBackendProxy, identifier));
}

RemoteTextDetectorProxy::RemoteTextDetectorProxy(RemoteRenderingBackendProxy& remoteRenderingBackendProxy, ShapeDetectionIdentifier identifier)
    : m_backing(identifier)
    , m_remoteRenderingBackendProxy(remoteRenderingBackendProxy)
{
}

RemoteTextDetectorProxy::~RemoteTextDetectorProxy()
{
    if (!m_remoteRenderingBackendProxy)
        return;

    m_remoteRenderingBackendProxy->streamConnection().send(Messages::RemoteRenderingBackend::ReleaseRemoteTextDetector(m_backing), m_remoteRenderingBackendProxy->renderingBackendIdentifier(), Seconds::infinity());
}

void RemoteTextDetectorProxy::detect(Ref<WebCore::ImageBuffer>&& imageBuffer, CompletionHandler<void(Vector<WebCore::ShapeDetection::DetectedText>&&)>&& completionHandler)
{
    if (!m_remoteRenderingBackendProxy) {
        completionHandler({ });
        return;
    }

    m_remoteRenderingBackendProxy->streamConnection().sendWithAsyncReply(Messages::RemoteTextDetector::Detect(imageBuffer->renderingResourceIdentifier()), WTFMove(completionHandler), m_backing, Seconds::infinity());
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
