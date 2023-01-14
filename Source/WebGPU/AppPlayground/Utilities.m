//
//  Utilities.m
//  WebGPU
//
//  Created by Myles C. Maxfield on 1/14/23.
//

#import "Utilities.h"

void* cast(CAMetalLayer *layer) {
    // I don't know how to do this in Swift, so I guess we can just do it here.
    return (__bridge void *)(layer);
}
