//
//  Ble.swift
//  BDUNK
//
//  Created by Nick Krahé on 20.05.22.
//

import Foundation
import CoreBluetooth
class BT:
    private var centralManager: CBCentralManager!
    private var dkPeripheral: CBPeripheral!
    private var txCharacteristic: CBCharacteristic!
    private var rxCharacteristic: CBCharacteristic!
