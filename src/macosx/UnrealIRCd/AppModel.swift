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
    
    init()
    {
        menuItem = NSStatusBar.systemStatusBar().statusItemWithLength(-1/*NSVariableStatusItemLength*/)
    }
    
    func setupStatusItem(menu: NSMenu)
    {
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
    
}