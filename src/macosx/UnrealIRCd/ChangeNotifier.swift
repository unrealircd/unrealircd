//
//  ChangeNotifier.swift
//  UnrealIRCd
//
//  Created by Travis McArthur on 7/19/15.
//  Copyright (c) 2015 UnrealIRCd Team. All rights reserved.
//

import Foundation
class ChangeNotifier {
    
    var changeDelegates : Array<ChangeNotifierDelegate> = []
    
    init()
    {
        
    }
    
    func attachChangeDelegate(delegate: ChangeNotifierDelegate)
    {
        changeDelegates.append(delegate)
    }
    
    func dettachChangeDelegate(delegate: ChangeNotifierDelegate)
    {
        for i in 0 ... changeDelegates.count
        {
            if changeDelegates[i] === delegate
            {
                changeDelegates.removeAtIndex(i)
            }
        }
    }
        func notifyListeners()
    {
        for listener in changeDelegates
        {
            listener.modelChanged(self)
        }
    }
}

protocol ChangeNotifierDelegate : class
{
    func modelChanged(model: ChangeNotifier);
}