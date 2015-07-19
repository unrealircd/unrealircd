//
//  ConfigurationModel.swift
//  UnrealIRCd
//
//  Created by Travis McArthur on 7/18/15.
//  Copyright (c) 2015 UnrealIRCd Team. All rights reserved.
//

import Foundation
class ConfigurationModel {

    let defaults = NSUserDefaults.standardUserDefaults()
    var changeDelegates : Set<ConfigurationModelChangeDelegate> = []
    static let autoStartDaemonKey = "IRCD_AUTOSTART"
    static let autoStartAgentKey = "AGENT_AUTOSTART"
    
    init()
    {
        
    }
    
    func attachChangeDelegate(delegate: ConfigurationModelChangeDelegate)
    {
        changeDelegates.insert(delegate)
    }
    
    func dettachChangeDelegate(delegate: ConfigurationModelChangeDelegate)
    {
        changeDelegates.remove(delegate)
    }
    
    var shouldAutoStartAgent : Bool {
        set(value)
        {
            defaults.setBool(value, forKey: ConfigurationModel.autoStartAgentKey)
            notifyListeners();
        }
        get
        {
            return defaults.boolForKey(ConfigurationModel.autoStartAgentKey)
        }
    }
    
    var shouldAutoStartDaemon : Bool {
        set(value)
        {
            defaults.setBool(value, forKey: ConfigurationModel.autoStartDaemonKey)
            notifyListeners();
        }
        get
        {
            return defaults.boolForKey(ConfigurationModel.autoStartDaemonKey)
        }
    }
    
    func notifyListeners()
    {
        for listener in changeDelegates
        {
            listener.configurationModelChanged(self)
        }
    }
}

protocol ConfigurationModelChangeDelegate : Hashable
{
    func configurationModelChanged(model: ConfigurationModel);
}