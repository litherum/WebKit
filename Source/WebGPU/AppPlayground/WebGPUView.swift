//
//  MetalView.swift
//  AppPlayground
//
//  Created by Myles C. Maxfield on 1/14/23.
//

import SwiftUI
import MetalKit
import WebGPU

struct WebGPUView: NSViewRepresentable {
    typealias NSViewType = MTKView

    init() {
    }

    func makeNSView(context: Context) -> MTKView {
        let mtkView = MTKView()
        mtkView.delegate = context.coordinator
        return mtkView
    }
    
    func updateNSView(_ nsView: MTKView, context: Context) {
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
        private var scheduler = Scheduler()

        private var instance: WGPUInstance
        private var adapter: WGPUAdapter? = nil
        private var device: WGPUDevice? = nil

        override init() {
            var instanceCocoaDescriptor = WGPUInstanceCocoaDescriptor(chain: WGPUChainedStruct(next: nil, sType: WGPUSType(WGPUSTypeExtended_InstanceCocoaDescriptor.rawValue))) { [scheduler] workItem in
                guard let workItem else {
                    return
                }
                scheduler.schedule(workItem)
            }
            instance = withUnsafePointer(to: &instanceCocoaDescriptor.chain) { pointer in
                var instanceDescriptor = WGPUInstanceDescriptor(nextInChain: pointer)
                return wgpuCreateInstance(&instanceDescriptor)
            }

            super.init()

            var requestAdapterOptions = WGPURequestAdapterOptions(nextInChain: nil, compatibleSurface: nil, powerPreference: WGPUPowerPreference_Undefined, forceFallbackAdapter: false)
            wgpuInstanceRequestAdapterWithBlock(instance, &requestAdapterOptions) { [self] (status: WGPURequestAdapterStatus, localAdapter: Optional<WGPUAdapter>, message: Optional<UnsafePointer<Int8>>) in
                assert(localAdapter != nil)
                adapter = localAdapter

                var deviceDescriptor = WGPUDeviceDescriptor(nextInChain: nil, label: nil, requiredFeaturesCount: 0, requiredFeatures: nil, requiredLimits: nil)
                wgpuAdapterRequestDeviceWithBlock(adapter, &deviceDescriptor) { [self] (status: WGPURequestDeviceStatus, localDevice: Optional<WGPUDevice>, message: Optional<UnsafePointer<Int8>>) in
                    assert(localDevice != nil)
                    device = localDevice
                }
            }
        }

        deinit {
            if device != nil {
                wgpuDeviceRelease(device)
            }
            if adapter != nil {
                wgpuAdapterRelease(adapter)
            }
            wgpuInstanceRelease(instance)
        }

        func draw(in view: MTKView) {
            guard device != nil, adapter != nil else {
                return
            }
            var commandEncoderDescriptor = WGPUCommandEncoderDescriptor(nextInChain: nil, label: nil)
            let commandEncoder = wgpuDeviceCreateCommandEncoder(device, &commandEncoderDescriptor)
            defer {
                wgpuCommandEncoderRelease(commandEncoder)
            }

            var commandBufferDescriptor = WGPUCommandBufferDescriptor(nextInChain: nil, label: nil)
            let commandBuffer = wgpuCommandEncoderFinish(commandEncoder, &commandBufferDescriptor)
            defer {
                wgpuCommandBufferRelease(commandBuffer)
            }

            let commands: [WGPUCommandBuffer?] = [commandBuffer]
            wgpuQueueSubmit(wgpuDeviceGetQueue(device), UInt32(commands.count), commands)
        }

        func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {
            print("Drawable size will change \(size)")
        }
    }
}
