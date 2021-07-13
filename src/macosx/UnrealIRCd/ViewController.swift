//
//  ViewController.swift
//  UnrealIRCd
//
//  Created by Travis McArthur on 6/26/15.
//  Copyright (c) 2015 UnrealIRCd Team. All rights reserved.
//

import Cocoa
import AppKit

class ViewController: NSViewController, ChangeNotifierDelegate {
    
    @IBOutlet private weak var autoStartAgentCheckbox : NSButton?
    @IBOutlet private weak var autoStartDaemonCheckbox : NSButton?
    @IBOutlet private weak var startStopButton : NSButton?
    static let stopButtonString = "Stop"
    static let startButtonString = "Start"
    var configModel : ConfigurationModel?
    {
        didSet {
            updateConfigurationOptions()
            configModel?.attachChangeDelegate(self)
        }
    }
    
    var daemonModel : DaemonModel?
    {
        didSet {
            updateDaemonButton()
            daemonModel?.attachChangeDelegate(self)
        }
    }
    
    func updateDaemonButton()
    {
        startStopButton?.title = daemonModel!.isRunning ? ViewController.stopButtonString : ViewController.startButtonString
    }
    
    func updateConfigurationOptions()
    {
        autoStartAgentCheckbox?.state = configModel!.shouldAutoStartAgent ? NSOnState : NSOffState
        autoStartDaemonCheckbox?.state = configModel!.shouldAutoStartDaemon ? NSOnState : NSOffState
    }
    
    override func viewWillDisappear() {
        saveConfigurationOptions()
    }
    
    func saveConfigurationOptions()
    {
        configModel?.shouldAutoStartAgent = autoStartAgentCheckbox?.state == NSOnState ? true : false
        configModel?.shouldAutoStartDaemon = autoStartAgentCheckbox?.state == NSOnState ? true : false
    }
    
    @IBAction func startStopServer(sender: AnyObject)
    {
        if daemonModel!.isRunning
        {
            daemonModel?.stop()
        }
        else
        {
            daemonModel?.start()
        }
    }
    
    func modelChanged(model: ChangeNotifier) {
        if model === daemonModel
        {
            updateDaemonButton()
        }
        
        else if model === configModel
        {
            updateConfigurationOptions()
        }
    }
    
    @IBAction func configure(sender: AnyObject)
    {
        
    }


}

