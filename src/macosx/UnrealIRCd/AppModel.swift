//
//  AppModel.swift
//  UnrealIRCd
//
//  Created by Travis McArthur on 7/18/15.
//  Copyright (c) 2015 UnrealIRCd Team. All rights reserved.
//

import Foundation
import AppKit

class AppModel
{
    var menuItem : NSStatusItem
    static let logoName = "logo.png"
    static let helpURL = "https://www.unrealircd.org/docs/UnrealIRCd_3.4.x_documentation"
    var daemonModel : DaemonModel
    var configurationModel : ConfigurationModel
    var windowController : NSWindowController?
    var mainMenu : NSMenu
    
    init()
    {
        
        daemonModel = DaemonModel()
        configurationModel = ConfigurationModel()
        menuItem = NSStatusBar.systemStatusBar().statusItemWithLength(-1/*NSVariableStatusItemLength*/)
    }
    
    func setupStatusItem(menu: NSMenu)
    {
        mainMenu = menu
        menuItem.image = NSImage(named: AppModel.logoName)
        menuItem.menu = menu
    }
    
    func launchHelp()
    {
        if let url = NSURL(string: AppModel.helpURL)
        {
            NSWorkspace.sharedWorkspace().openURL(url)
        }
    }
    
    func showPreferences()
    {
        windowController!.showWindow(self)
    }
    
    func startupComplete()
    {
        if configurationModel.shouldAutoStartDaemon
        {
            daemonModel.start()
            updateUIFromDaemon()
        }
        
        
        let storyboard = NSStoryboard(name: "Main", bundle:nil)
        let controller = storyboard!.instantiateControllerWithIdentifier("Configuration") as! NSWindowController?
        assert(controller != nil, "Unable to load window from XIB")
        windowController = controller
    }
    
    func shutdown()
    {
        daemonModel.stop()
        exit(0)
    }
    
    func startDaemon()
    {
        daemonModel.stop()
        updateUIFromDaemon()
    }
    
    func stopDaemon()
    {
        daemonModel.start()
        updateUIFromDaemon()
    }
    
    func updateUIFromDaemon()
    {
        let daemonStatus = daemonModel.isRunning
        mainMenu.itemWithTitle("Start UnrealIRCd")?.enabled = !daemonStatus
        mainMenu.itemWithTitle("Stop UnrealIRCd")?.enabled = !daemonStatus
    }
    
}