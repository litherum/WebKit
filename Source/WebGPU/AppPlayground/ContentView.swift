//
//  ContentView.swift
//  AppPlayground
//
//  Created by Myles C. Maxfield on 1/14/23.
//

import SwiftUI

struct ContentView: View {
    var body: some View {
        WebGPUView().padding()
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
