//
//  WebGPUView+UIKit.swift
//  AppPlayground
//
//  Created by Myles C. Maxfield on 1/14/23.
//

import SwiftUI
import Foundation
import MetalKit

#if os(iOS)
struct WebGPUView: UIViewRepresentable {
    typealias UIViewType = WebGPUViewInner.ViewType
    typealias Coordinator = WebGPUViewInner.Coordinator

    private let inner = WebGPUViewInner()

    func makeUIView(context: Context) -> MTKView {
        return inner.makeView(coordinator: context.coordinator)
    }
    
    func updateUIView(_ uiView: MTKView, context: Context) {
        inner.updateView(uiView)
    }

    func makeCoordinator() -> Coordinator {
        return Coordinator()
    }
}
#endif
