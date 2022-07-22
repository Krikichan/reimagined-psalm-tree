//
//  BD.swift
//  BDUNK
//
//  Created by Nick Krahé on 17.05.22.
//

import Foundation
import CoreBluetooth

//IMAGE TYPES
// 0 NRF52
// 1 NRF91
// 2 BOTH
extension Data {
    var uint32:UInt32 {
        return UInt32(littleEndian: self.withUnsafeBytes { bytes in
            bytes.load(as: UInt32.self)
        })
    }
}

class BD: NSObject{

    private var centralManager: CBCentralManager!
    private var dkPeripheral: CBPeripheral!
    private var txCharacteristic: CBCharacteristic!
    private var rxCharacteristic: CBCharacteristic!
    
    private var adata : Data!
    private var whattoupdate: UInt8!
    private var mtusize : Int!
    private var lastopcode :UInt8 = 0
    
    var receivedData : Data!
    var file_size = 0
    var cur_majorfw52:UInt8 = 0
    var cur_majorfw92:UInt8 = 0
    var cur_minorfw52:UInt8 = 0
    var cur_minorfw92:UInt8 = 0
    var m_majorfw:UInt8 = 0
    var m_minorfw:UInt8 = 0
    var avsize52:UInt32 = 0
    var avsize91:UInt32 = 0
    
    var lastidx: Int = 0
    var running = true
    var updating = false
    var autoconnect = true
    
    var filename:String = " "
    var filename91:String = " "
    var filename52:String = " "
    //Construtor
    override public required init(){
        super.init()
        centralManager = CBCentralManager.init(delegate: self, queue: nil)
        
    }
    //"Main" Run Loop Catches the Program and enables Eventbased BT Stack Events
    func run(){
        let runLoop = RunLoop.current
        let distantFuture = Date.distantFuture

        while running == true && runLoop.run(mode: RunLoop.Mode.default, before: distantFuture) {}
        
    }
    
    //Scanns Arguments given to Program on Terminal Call
    func getArguments(){
        whattoupdate = UInt8(CommandLine.arguments[2])! //0 nrf52 1 nrf91 2 Both

        switch whattoupdate{
        case 0:
            filename = CommandLine.arguments[4]
            break
        case 1:
            filename = CommandLine.arguments[4]
            break
        case 2:
            filename91 = CommandLine.arguments[4] //name -o 3 -i name -b name
            filename52 = CommandLine.arguments[6]
            break
        default:
            break
        }
    }
    
    func usage(){
        print("-u x -b y -i z")
    }
    
    func getDataObject(name:String) -> Data? {
        let filename = name
        let documentsDirectory = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).last
        let fileURL = documentsDirectory!.appendingPathComponent(filename)
        let contents: Data?
        do{
            contents = try Data(contentsOf: fileURL, options: .mappedIfSafe)
            
        }catch let error as NSError{
            print("Failed to read from Project")
            print(error)
            return nil
        }
        return contents
    }
    //Extracts MTU-1 Sized Chunks of Data
    func extract() -> Data? {
    guard adata!.count > 0 else {
        return nil
    }
    var range:Range<Data.Index>
    // Create a range based on the length of data to return
        var start = adata.index(adata.startIndex,offsetBy: lastidx)
        var end:Data.Index

    if file_size - lastidx >= mtusize{
        end = adata.index(start,offsetBy: mtusize)
    }
    else{// Check if we reached the End
        end = adata.index(start,offsetBy: file_size - lastidx)
    }
        //print("start",start)
        //print("end",end)
        range = start ..< end
        
        lastidx = lastidx + mtusize
        if((lastidx % 1024) == 0){
            print("Progress:    \(lastidx)/\(file_size)")
        }
    // Get a new copy of data
        let subData = adata?.subdata(in: range)

    // Return the new copy of data
        return subData
    }
    
    //Processes Received BT Data from DEVICE INFO Call
    func process_rec_Data() -> Data? {
        guard receivedData!.count > 0 else {
            return nil
        }

        var data = receivedData!

         cur_majorfw52 = data[1]
         cur_minorfw52 = data[3]
         cur_majorfw92 = data[2]
         cur_minorfw52 = data[4]
        
        var range:Range<Data.Index>
        range = 5 ..< 9
        // Get a new copy of data
        var subData = data.subdata(in: range)
        data.removeSubrange(range)
        avsize52 = subData.uint32
        subData = data.subdata(in: range)
        data.removeSubrange(range)
        avsize91 = subData.uint32
        print("Just out of Curiosity FW Vers 52\(cur_majorfw52) \(cur_minorfw52) 91 \(cur_majorfw92) \(cur_minorfw92) Sizes 52 \(avsize52) 91 \(avsize91)")
        return subData
    }
    func pre_logic(){
        var _ = process_rec_Data() // Processes what to Update
        getArguments() // Gets Filename and what to Update

        if(whattoupdate == 0){//52
            //Todo Make Receive Data usefull
            print("Updating nRF 52 Using File: ",filename)
            adata = getDataObject(name: filename)
            file_size = adata.count
            print("File Size ",file_size)
            
            if(avsize52 < adata.count){
                print("Available Size on 52 too Small")
                //Todo Stop Connection and Tell to Abort
                return
            }
            
            if(cur_majorfw52 > m_majorfw ){
                print("Major FW 52 not upto date")
            }
        }
        if(whattoupdate == 1){//92
            print("Updating nRF91 Using File",filename)
            adata = getDataObject(name: filename)
            file_size = adata.count
            print("File Size ",file_size)

            if(avsize91 < adata.count){
                print("Available Size on 92 too Small")
                //Todo Stop Connection and Tell to Abort
                return
            }

            //Only Compare Major but allow Downgrading
            if(cur_majorfw92 > m_majorfw ){
                print("Major FW 92 not upto date")
            }
            //Go ahead and reqeuest Update
            
        }
        if(whattoupdate == 2){//first

            if(avsize91 < adata.count){
                print("Available Size on 92 too Small")
                //Todo Stop Connection and Tell to Abort
                return
            }

            if(cur_majorfw92 > m_majorfw ){
                print("Major FW 92 not upto date")
                return
            }
            adata = getDataObject(name: filename91)
            //Go ahead and reqeuest Update
        }
        lastopcode = 1
        logic()
    }
    
    //Does Logik based on OpCodes (These OPCodes are in the Header file of the bt dfumodule)
    func logic(){
        // I like your funny words Music man
        var bufferData = Data()
        
        switch lastopcode{
        case 0:// Start DFU Process (basically tell Chip to init everything)
            bufferData.append(0)
            print("-----------------------------------------------------")
            print("Want to Start Procedure ?")
            
            if(readLine() != "y"){
                print("Aborted")
                return
            }

            updating = true
            break
        case 1:// Send Info (for Info Request) Sends File Size and Int indicating what to update

            print("File Size in Bytes:\(adata.count)")
            print("What to Update: ",whattoupdate!)

            bufferData.append(1)

            withUnsafeBytes(of: adata.count.littleEndian) { bufferData.append(contentsOf: $0) }//Append Size of File
            withUnsafeBytes(of: whattoupdate.littleEndian) { bufferData.append(contentsOf: $0) }//Append Update Type
            print("Data Info send")
            print("-----------------------------------------------------")
            break
        case 2:// Get Data
            bufferData.append(2)
            bufferData.append(extract()!)//extract gets next bytes
            break
        case 6: //Stop Updating (not impl) should stop everything
            updating = false
            break
        case 7: // Mark Tx as Done
            print("Update Transmitted")
            print("-----------------------------------------------------")
            //Reinitialise
            lastidx = 0

            //Check if another Update has to be done
            if(whattoupdate == 2){
                //Means Update Self
                print("Starting Self Update")
                whattoupdate = 0
                lastopcode = 8
                adata = getDataObject(name: filename52)
                file_size = adata.count
                
                print("File Size in Bytes:\(adata.count)")
                bufferData.append(lastopcode)

            }else{
                autoconnect = false
                stopScanning()
                bufferData.append(254)//Mark Update as Done
            }

            break

        case 10://Marks Start of Self Update after NRF91 Update
            lastopcode = 1
            bufferData.append(1)
            withUnsafeBytes(of: adata.count.littleEndian) { bufferData.append(contentsOf: $0) }//Append Size of File
            withUnsafeBytes(of: whattoupdate.littleEndian) { bufferData.append(contentsOf: $0) }//Append Update Type
            print("Data Info send")
            print("-----------------------------------------------------")
            break
            
        case 32://Retrieved Data ("hex 20")
            //Do logik on what to and if to update
            print("Received Device Information")
            pre_logic() //Invokes Logic handler again
            return
            
        default:
            print("Unknown OP Code: ",lastopcode)
            break
            //Some Cases are Missing
        }
        //Write to NUS
        if(updating){
            writeOutgoingValue(data: bufferData)
        }
    }
    
    //BT Helper Functions
    func disconnectFromDevice () {
        if dkPeripheral != nil {
        centralManager?.cancelPeripheralConnection(dkPeripheral!)
        }
     }
    
    func startScanning() -> Void {
        // Start Scanning
        centralManager?.scanForPeripherals(withServices: [CBUUIDs.BLEService_UUID])

    }
    func stopScanning() -> Void {
        centralManager?.stopScan()
}
    func writeOutgoingValue(data: Data){
          
        if let dkPeripheral = dkPeripheral {
              
          if let rxCharacteristic = rxCharacteristic {
                  
              dkPeripheral.writeValue(data, for: rxCharacteristic, type: CBCharacteristicWriteType.withoutResponse)
            }
        }
    }
}

extension BD: CBCentralManagerDelegate {
    
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
    
     switch central.state {
          case .poweredOff:
              print("Is Powered Off.")
                stopScanning()
          case .poweredOn:
              print("Is Powered On.")
                startScanning()
          case .unsupported:
              print("Is Unsupported.")
          case .unauthorized:
              print("Is Unauthorized.")
          case .unknown:
              print("Unknown")
          case .resetting:
              print("Resetting")
                updating = false
          @unknown default:
            print("Error")
            stopScanning()
            running = false
          }
    }
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        dkPeripheral = peripheral
        dkPeripheral.delegate = self
        
        print("Peripheral Discovered: \(peripheral)")
        if(autoconnect){
            centralManager?.connect(dkPeripheral!,options: nil)
        }
    }
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        dkPeripheral.discoverServices([CBUUIDs.BLEService_UUID])
    }

}

extension BD: CBPeripheralDelegate {
    //Called after Connecting
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
            print("*******************************************************")

            if ((error) != nil) {
                print("Error discovering services: \(error!.localizedDescription)")
                return
            }
            guard let services = peripheral.services else {
                return
            }
            //We need to discover the all characteristic
            for service in services {
                peripheral.discoverCharacteristics(nil, for: service)
            }
            print("Discovered Services: \(services)")
        }
    //Called discovering Services
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
           
        guard let characteristics = service.characteristics else {
              return
          }

          print("Found \(characteristics.count) characteristics.")

          for characteristic in characteristics {

            if characteristic.uuid.isEqual(CBUUIDs.BLE_Characteristic_uuid_Rx)  {

                rxCharacteristic = characteristic
                mtusize = dkPeripheral.maximumWriteValueLength(for: .withoutResponse) - 1
                if (mtusize != 128){
                    print("Warning MTU Size is not 128, it is: ",mtusize!)
                }

                print("RX Characteristic: \(rxCharacteristic.uuid)")
            }

            if characteristic.uuid.isEqual(CBUUIDs.BLE_Characteristic_uuid_Tx){
              
              txCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: txCharacteristic!)
                print("TX Characteristic: \(txCharacteristic.uuid)")
            }

          }
        print("MTU Size: ",mtusize!)
        print("Ready to communicate, Starting File Update")
        logic() // Starts Transmission
    }
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {

        guard let data = characteristic.value else {
               // no data transmitted, handle if needed
            print("No Data transmitted")
               return
           }
        guard let firstByte = data.first else {
            // handle unexpected empty data
            print("Empty Data")
            return
        }
        receivedData = data //Includes First Byte
        lastopcode = firstByte
        
        logic()
    }
}
extension BD: CBPeripheralManagerDelegate {

  func peripheralManagerDidUpdateState(_ peripheral: CBPeripheralManager) {
    switch peripheral.state {
    case .poweredOn:
        print("Peripheral Is Powered On.")
    case .unsupported:
        print("Peripheral Is Unsupported.")
    case .unauthorized:
    print("Peripheral Is Unauthorized.")
    case .unknown:
        print("Peripheral Unknown")
    case .resetting:
        print("Peripheral Resetting")
    case .poweredOff:
      print("Peripheral Is Powered Off.")
    @unknown default:
      print("Error")
    }
  }
}
