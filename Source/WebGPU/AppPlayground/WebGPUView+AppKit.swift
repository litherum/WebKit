//
//  WebGPUView+AppKit.swift
//  AppPlayground
//
//  Created by Myles C. Maxfield on 1/14/23.
//

import SwiftUI
import Foundation
import MetalKit

#if os(macOS)
struct WebGPUView: NSViewRepresentable {
    typealias NSViewType = WebGPUViewInner.ViewType
    typealias Coordinator = WebGPUViewInner.Coordinator

    private let inner = WebGPUViewInner()

    func makeNSView(context: Context) -> MTKView {
        return inner.makeView(coordinator: context.coordinator)
    }
    
    func updateNSView(_ nsView: MTKView, context: Context) {
        inner.updateView(nsView)
    }

    func makeCoordinator() -> Coordinator {
        return Coordinator()
    }
}
#endif
