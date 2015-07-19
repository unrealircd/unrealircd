//
//  ViewController.swift
//  UnrealIRCd
//
//  Created by Travis McArthur on 6/26/15.
//  Copyright (c) 2015 UnrealIRCd Team. All rights reserved.
//

import Cocoa
import AppKit

class ViewController: NSViewController {
    
    @IBOutlet weak var autoStartAgentCheckbox : NSButton?
    @IBOutlet weak var autoStartDaemonCheckbox : NSButton?
    @IBOutlet weak var startStopButton : NSButton?
    static let stopButtonString = "Stop"
    static let startButtonString = "Start"
    var configModel : ConfigurationModel?
    
    func updateInterface(model: ConfigurationModel, daemonStatus: Bool)
    {
        configModel = model
        autoStartAgentCheckbox?.state = model.shouldAutoStartAgent ? NSOnState : NSOffState
        autoStartDaemonCheckbox?.state = model.shouldAutoStartDaemon ? NSOnState : NSOffState
        startStopButton?.title = daemonStatus ? ViewController.stopButtonString : ViewController.startButtonString
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
    }
    
    override func viewWillDisappear() {
        configModel?.shouldAutoStartAgent = autoStartAgentCheckbox?.state == NSOnState ? true : false
        configModel?.shouldAutoStartDaemon = autoStartAgentCheckbox?.state == NSOnState ? true : false
    }

    override var representedObject: AnyObject? {
        didSet {
        }
    }


}

