//
//  ConfigurationModel.swift
//  UnrealIRCd
//
//  Created by Travis McArthur on 7/18/15.
//  Copyright (c) 2015 UnrealIRCd Team. All rights reserved.
//

import Foundation
class ConfigurationModel : ChangeNotifier {

    let defaults = NSUserDefaults.standardUserDefaults()
    static let autoStartDaemonKey = "IRCD_AUTOSTART"
    static let autoStartAgentKey = "AGENT_AUTOSTART"
    
    var shouldAutoStartAgent : Bool {
        set(value)
        {
            defaults.setBool(value, forKey: ConfigurationModel.autoStartAgentKey)
            defaults.synchronize()
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
            defaults.synchronize()
            notifyListeners();
        }
        get
        {
            return defaults.boolForKey(ConfigurationModel.autoStartDaemonKey)
        }
    }
}