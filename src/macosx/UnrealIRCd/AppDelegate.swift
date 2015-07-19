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

    
    @IBOutlet weak var mainMenu : NSMenu?
    var appModel : AppModel
    
    override init() {
        assert(mainMenu != nil, "Unable to load Menu from XIB")
        appModel = AppModel(menu: mainMenu!)
        super.init()
    }
 
    func applicationDidFinishLaunching(aNotification: NSNotification)
    {
        appModel.startup()
        
    }

    func applicationWillTerminate(aNotification: NSNotification) {
        appModel.shutdown()
    }
    
    @IBAction func startDaemon(sender: NSMenuItem) {
        appModel.startDaemon()
    }
    
    @IBAction func stopDaemon(sender: NSMenuItem) {
        appModel.stopDaemon()
    }
    
    @IBAction func configureDaemon(sender: NSMenuItem) {
        appModel.showPreferences()
    }
    
    @IBAction func help(sender: NSMenuItem) {
        appModel.launchHelp()
    }
    
    @IBAction func quit(sender: NSMenuItem) {
        appModel.shutdown()
    }


}

