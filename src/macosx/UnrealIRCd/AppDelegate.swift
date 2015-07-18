//
//  AppDelegate.swift
//  UnrealIRCd
//
//  Created by Travis McArthur on 6/26/15.
//  Copyright (c) 2015 UnrealIRCd Team. All rights reserved.
//

import Cocoa

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {

    
    @IBOutlet var mainMenu : NSMenu?
    var menuItem : NSStatusItem!
    
    func applicationDidFinishLaunching(aNotification: NSNotification) {
        // Compiler error - hack below
        menuItem = NSStatusBar.systemStatusBar().statusItemWithLength(-1/*NSVariableStatusItemLength*/)
        menuItem.image = NSImage(named: "logo.png")
        menuItem.menu = mainMenu!
    }

    func applicationWillTerminate(aNotification: NSNotification) {

    }
    
    @IBAction func startDaemon(sender: AnyObject) {
        
    }
    
    @IBAction func stopDaemon(sender: AnyObject) {
        
    }
    
    @IBAction func configureDaemon(sender: AnyObject) {
        let storyboard = NSStoryboard(name: "Main", bundle:nil)
        let controller = storyboard!.instantiateControllerWithIdentifier("Configuration")
        controller!.showWindow(self)
    }
    
    @IBAction func help(sender: AnyObject) {
        
    }


}

