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
    var daemonModel : DaemonModel
    var configurationModel : ConfigurationModel
    
    override init() {
        appModel = AppModel()
        daemonModel = DaemonModel()
        configurationModel = ConfigurationModel()
        super.init()
    }
 
    func applicationDidFinishLaunching(aNotification: NSNotification)
    {
        assert(mainMenu != nil, "Unable to load Menu from XIB")
        appModel.setupStatusItem(mainMenu!)
        
        if configurationModel.shouldAutoStart
        {
            daemonModel.start()
        }
        
    }

    func applicationWillTerminate(aNotification: NSNotification) {
        daemonModel.stop()
    }
    
    @IBAction func startDaemon(sender: NSMenuItem) {
        daemonModel.start()
    }
    
    @IBAction func stopDaemon(sender: NSMenuItem) {
        daemonModel.stop()
    }
    
    @IBAction func configureDaemon(sender: NSMenuItem) {
        let storyboard = NSStoryboard(name: "Main", bundle:nil)
        let controller = storyboard!.instantiateControllerWithIdentifier("Configuration")
        controller!.showWindow(self)
    }
    
    @IBAction func help(sender: NSMenuItem) {
        appModel.launchHelp()
    }


}

