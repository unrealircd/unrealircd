//
//  AppDelegate.swift
//  UnrealIRCd
//
//  Created by Travis McArthur on 6/26/15.
//  Copyright (c) 2015 UnrealIRCd Team. All rights reserved.
//

import Cocoa
import AppKit

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {

    

    var menuItem : NSStatusItem!;
    
    func applicationDidFinishLaunching(aNotification: NSNotification) {
        println("App did finish launching!")
        // Compiler error - hack below
        menuItem = NSStatusBar.systemStatusBar().statusItemWithLength(-1/*NSVariableStatusItemLength*/)
        menuItem.image = NSImage(named: "bat23.png")
    }

    func applicationWillTerminate(aNotification: NSNotification) {

    }


}

