//
//  DaemonModel.swift
//  UnrealIRCd
//
//  Created by Travis McArthur on 7/18/15.
//  Copyright (c) 2015 UnrealIRCd Team. All rights reserved.
//

import Foundation

class DaemonModel : ChangeNotifier
{
    
    
    override init()
    {
        isRunning = false
        super.init()
    }

    
    func start()
    {
        isRunning = true
    }
    
    func stop()
    {
        isRunning = false
    }
    
    var isRunning : Bool
    {
        didSet {
            notifyListeners()
        }
    }
    
    
}