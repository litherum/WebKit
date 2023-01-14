//
//  MetalView.swift
//  AppPlayground
//
//  Created by Myles C. Maxfield on 1/14/23.
//

import SwiftUI
import MetalKit
import WebGPU

struct WebGPUViewInner {
    typealias ViewType = MTKView

    func makeView(coordinator: Coordinator) -> MTKView {
        let mtkView = MTKView()
        coordinator.set(view: mtkView)
        mtkView.delegate = coordinator
        return mtkView
    }
    
    func updateView(_ nsView: MTKView) {
    }

    func makeCoordinator() -> Coordinator {
        return Coordinator()
    }

    class Coordinator : NSObject, MTKViewDelegate {
        private class Scheduler {
            private var timers = [Timer]()
            func schedule(_ work: @escaping () -> ()) {
                timers.append(Timer.scheduledTimer(withTimeInterval: 0, repeats: false) { [self] timer in
                    work()
                    timers.removeAll(where: { $0 == timer })
                })
            }
        }
        private let scheduler = Scheduler()

        private let instance: WGPUInstance
        private var adapter: WGPUAdapter? = nil
        private var device: WGPUDevice? = nil
        private var surface: WGPUSurface? = nil
        private var swapChain: WGPUSwapChain? = nil

        private var view: MTKView? = nil

        override init() {
            var instanceCocoaDescriptor = WGPUInstanceCocoaDescriptor(chain: WGPUChainedStruct(next: nil, sType: WGPUSType(WGPUSTypeExtended_InstanceCocoaDescriptor.rawValue))) { [scheduler] workItem in
                guard let workItem else {
                    return
                }
                scheduler.schedule(workItem)
            }
            instance = withUnsafePointer(to: &instanceCocoaDescriptor) { pointer in
                return pointer.withMemoryRebound(to: WGPUChainedStruct.self, capacity: 1) { pointer in
                    var instanceDescriptor = WGPUInstanceDescriptor(nextInChain: pointer)
                    return wgpuCreateInstance(&instanceDescriptor)
                }
            }

            super.init()

            var requestAdapterOptions = WGPURequestAdapterOptions(nextInChain: nil, compatibleSurface: nil, powerPreference: WGPUPowerPreference_Undefined, forceFallbackAdapter: false)
            // FIXME: Make a Swift overlay so this can use async/await
            wgpuInstanceRequestAdapterWithBlock(instance, &requestAdapterOptions) { [self] (status: WGPURequestAdapterStatus, localAdapter: Optional<WGPUAdapter>, message: Optional<UnsafePointer<Int8>>) in
                assert(localAdapter != nil)
                adapter = localAdapter

                var deviceDescriptor = WGPUDeviceDescriptor(nextInChain: nil, label: nil, requiredFeaturesCount: 0, requiredFeatures: nil, requiredLimits: nil)
                wgpuAdapterRequestDeviceWithBlock(adapter, &deviceDescriptor) { [self] (status: WGPURequestDeviceStatus, localDevice: Optional<WGPUDevice>, message: Optional<UnsafePointer<Int8>>) in
                    assert(localDevice != nil)
                    device = localDevice

                    if let view, let delegate = view.delegate {
                        // We lost the race
                        delegate.mtkView(view, drawableSizeWillChange: view.drawableSize)
                    }
                }
            }
        }

        deinit {
            if swapChain != nil {
                wgpuSwapChainRelease(device)
            }
            if surface != nil {
                wgpuSurfaceRelease(device)
            }
            if device != nil {
                wgpuDeviceRelease(device)
            }
            if adapter != nil {
                wgpuAdapterRelease(adapter)
            }
            wgpuInstanceRelease(instance)
        }

        func set(view: MTKView) {
            assert(self.view == nil)
            self.view = view

            var surfaceDescriptorFromMetalLayer = WGPUSurfaceDescriptorFromMetalLayer(chain: WGPUChainedStruct(next: nil, sType: WGPUSType_SurfaceDescriptorFromMetalLayer), layer: cast(view.layer as? CAMetalLayer))
            surface = withUnsafePointer(to: &surfaceDescriptorFromMetalLayer) { pointer in
                return pointer.withMemoryRebound(to: WGPUChainedStruct.self, capacity: 1) { pointer in
                    var surfaceDescriptor = WGPUSurfaceDescriptor(nextInChain: pointer, label: nil)
                    return wgpuInstanceCreateSurface(instance, &surfaceDescriptor)
                }
            }
        }

        func draw(in view: MTKView) {
            guard let device, let swapChain else {
                return
            }
            var commandEncoderDescriptor = WGPUCommandEncoderDescriptor(nextInChain: nil, label: nil)
            let commandEncoder = wgpuDeviceCreateCommandEncoder(device, &commandEncoderDescriptor)
            defer {
                wgpuCommandEncoderRelease(commandEncoder)
            }

            let textureView = wgpuSwapChainGetCurrentTextureView(swapChain)
            let colorAttachments = [WGPURenderPassColorAttachment(view: textureView, resolveTarget: nil, loadOp: WGPULoadOp_Clear, storeOp: WGPUStoreOp_Store, clearColor: WGPUColor(r: 0, g: 1, b: 0, a: 1))]
            let renderPassEncoder = colorAttachments.withUnsafeBufferPointer { pointer in
                var renderPassDescriptor = WGPURenderPassDescriptor(nextInChain: nil, label: nil, colorAttachmentCount: UInt32(colorAttachments.count), colorAttachments: pointer.baseAddress, depthStencilAttachment: nil, occlusionQuerySet: nil, timestampWriteCount: 0, timestampWrites: nil)
                return wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDescriptor)
            }
            defer {
                wgpuRenderPassEncoderRelease(renderPassEncoder)
            }

            wgpuRenderPassEncoderEndPass(renderPassEncoder)

            var commandBufferDescriptor = WGPUCommandBufferDescriptor(nextInChain: nil, label: nil)
            let commandBuffer = wgpuCommandEncoderFinish(commandEncoder, &commandBufferDescriptor)
            defer {
                wgpuCommandBufferRelease(commandBuffer)
            }

            let commands: [WGPUCommandBuffer?] = [commandBuffer]
            wgpuQueueSubmit(wgpuDeviceGetQueue(device), UInt32(commands.count), commands)

            wgpuSwapChainPresent(swapChain)
        }

        func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {
            guard let device, let surface else {
                // The device is still being brought up. No biggie; this function will be called again when it's done.
                return
            }
            if swapChain != nil {
                wgpuSwapChainRelease(device)
            }
            var swapChainDescriptor = WGPUSwapChainDescriptor(nextInChain: nil, label: nil, usage: WGPUTextureUsage_RenderAttachment.rawValue, format: WGPUTextureFormat_BGRA8Unorm, width: UInt32(size.width), height: UInt32(size.height), presentMode: WGPUPresentMode_Immediate)
            swapChain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDescriptor)
        }
    }
}
