//
//  WindowController.swift
//  UnrealIRCd
//
//  Created by Travis McArthur on 7/19/15.
//  Copyright (c) 2015 UnrealIRCd Team. All rights reserved.
//

import Foundation
import AppKit

class WindowController : NSWindowController
{

    func setupModels(daemonModel: DaemonModel, configurationModel: ConfigurationModel) {
        let viewController = contentViewController as! ViewController
        viewController.daemonModel = daemonModel
        viewController.configModel = configurationModel
        super.windowWillLoad()
    }
    
    
}