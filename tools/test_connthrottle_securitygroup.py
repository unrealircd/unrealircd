#!/usr/bin/env python3
"""
Test script for connthrottle and security_group RPC methods.
"""

import requests
import json
import urllib3

# Disable SSL warnings for self-signed certs
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

RPC_URL = "https://localhost:8600/api"
RPC_USER = "adminpanel"
RPC_PASS = "password"

def rpc_call(method, params=None):
    """Make an RPC call and return the result."""
    payload = {
        "jsonrpc": "2.0",
        "method": method,
        "params": params or {},
        "id": 1
    }
    
    response = requests.post(
        RPC_URL,
        json=payload,
        auth=(RPC_USER, RPC_PASS),
        verify=False
    )
    
    result = response.json()
    return result

def test_connthrottle_status():
    """Test connthrottle.status RPC method."""
    print("=" * 60)
    print("Testing: connthrottle.status")
    print("=" * 60)
    
    result = rpc_call("connthrottle.status")
    
    if "error" in result:
        print(f"ERROR: {result['error']}")
        return False
    
    data = result.get("result", {})
    print(f"Enabled: {data.get('enabled')}")
    print(f"State: {data.get('state')}")
    print(f"Throttling this minute: {data.get('throttling_this_minute')}")
    print(f"Throttling previous minute: {data.get('throttling_previous_minute')}")
    print(f"Start delay remaining: {data.get('start_delay_remaining')}")
    print(f"Reputation gathering: {data.get('reputation_gathering')}")
    
    if "counters" in data:
        print(f"Counters: {json.dumps(data['counters'], indent=2)}")
    if "stats_last_minute" in data:
        print(f"Stats (last minute): {json.dumps(data['stats_last_minute'], indent=2)}")
    if "config" in data:
        print(f"Config: {json.dumps(data['config'], indent=2)}")
    
    print("SUCCESS\n")
    return True

def test_connthrottle_set():
    """Test connthrottle.set RPC method (just query current state, don't change)."""
    print("=" * 60)
    print("Testing: connthrottle.set (reset)")
    print("=" * 60)
    
    # Test with 'reset' action which is safe
    result = rpc_call("connthrottle.set", {"action": "reset"})
    
    if "error" in result:
        print(f"ERROR: {result['error']}")
        return False
    
    print(f"Result: {json.dumps(result.get('result', {}), indent=2)}")
    print("SUCCESS\n")
    return True

def test_security_group_list():
    """Test security_group.list RPC method."""
    print("=" * 60)
    print("Testing: security_group.list")
    print("=" * 60)
    
    result = rpc_call("security_group.list")
    
    if "error" in result:
        print(f"ERROR: {result['error']}")
        return False
    
    data = result.get("result", {})
    groups = data.get("list", [])
    print(f"Found {len(groups)} security groups:")
    for group in groups:
        print(f"  - {group.get('name', 'unknown')}")
    
    print("SUCCESS\n")
    return True

def test_security_group_get():
    """Test security_group.get RPC method."""
    print("=" * 60)
    print("Testing: security_group.get")
    print("=" * 60)
    
    # First get the list to find a valid group name
    list_result = rpc_call("security_group.list")
    if "error" in list_result:
        print(f"ERROR getting list: {list_result['error']}")
        return False
    
    groups = list_result.get("result", {}).get("list", [])
    if not groups:
        print("No security groups found to test with")
        return True
    
    # Test getting the first group
    group_name = groups[0].get("name")
    print(f"Getting details for group: {group_name}")
    
    result = rpc_call("security_group.get", {"name": group_name})
    
    if "error" in result:
        print(f"ERROR: {result['error']}")
        return False
    
    print(f"Result: {json.dumps(result.get('result', {}), indent=2)}")
    print("SUCCESS\n")
    return True

def main():
    print("\n" + "=" * 60)
    print("UnrealIRCd RPC Test: connthrottle & security_group")
    print("=" * 60 + "\n")
    
    tests = [
        ("connthrottle.status", test_connthrottle_status),
        ("connthrottle.set", test_connthrottle_set),
        ("security_group.list", test_security_group_list),
        ("security_group.get", test_security_group_get),
    ]
    
    results = []
    for name, test_func in tests:
        try:
            success = test_func()
            results.append((name, success))
        except Exception as e:
            print(f"EXCEPTION in {name}: {e}")
            results.append((name, False))
    
    print("\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)
    
    passed = sum(1 for _, success in results if success)
    total = len(results)
    
    for name, success in results:
        status = "PASS" if success else "FAIL"
        print(f"  {name}: {status}")
    
    print(f"\nTotal: {passed}/{total} passed")
    
    return 0 if passed == total else 1

if __name__ == "__main__":
    exit(main())
