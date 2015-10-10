//
//  AppModel.swift
//  UnrealIRCd
//
//  Created by Travis McArthur on 7/18/15.
//  Copyright (c) 2015 UnrealIRCd Team. All rights reserved.
//

import Foundation
import AppKit

class AppModel : ChangeNotifierDelegate
{
    var menuItem : NSStatusItem
    static let logoName = "logo.png"
    static let helpURL = "https://www.unrealircd.org/docs/UnrealIRCd_4_documentation"
    var daemonModel : DaemonModel
    var configurationModel : ConfigurationModel
    var windowController : NSWindowController?
    var mainMenu : NSMenu
    
    init(menu: NSMenu)
    {
        menuItem = NSStatusBar.systemStatusBar().statusItemWithLength(-1/*NSVariableStatusItemLength*/)
        mainMenu = menu
        menuItem.image = NSImage(named: AppModel.logoName)
        menuItem.menu = menu
        
        daemonModel = DaemonModel()
        configurationModel = ConfigurationModel()

        daemonModel.attachChangeDelegate(self)
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
    
    func startup()
    {
        if configurationModel.shouldAutoStartDaemon
        {
            daemonModel.start()
        }
        
        
        let storyboard = NSStoryboard(name: "Main", bundle:nil)
        let controller = storyboard!.instantiateControllerWithIdentifier("Configuration") as! WindowController?
        assert(controller != nil, "Unable to load window from XIB")
        controller?.setupModels(daemonModel, configurationModel: configurationModel)
        windowController = controller
    }
    
    func shutdown()
    {
        daemonModel.stop()
        exit(0)
    }
    
    func startDaemon()
    {
        daemonModel.start()
    }
    
    func stopDaemon()
    {
        daemonModel.stop()
    }
    
    func modelChanged(model: ChangeNotifier)
    {
        let daemonStatus = daemonModel.isRunning
        mainMenu.itemWithTitle("Start UnrealIRCd")?.enabled = !daemonStatus
        mainMenu.itemWithTitle("Stop UnrealIRCd")?.enabled = daemonStatus
    }
    
}