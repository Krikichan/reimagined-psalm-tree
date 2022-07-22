//
//  ConsoleIO.swift
//  BDUNK
//
//  Created by Nick Krahé on 17.05.22.
//

import Foundation
enum OutputType{
    case error
    case standard
}
class ConsoleIO{
    func writeMessage(_ message: String, to:OutputType = .standard){
        switch to{
        case .standard:
          print("\(message)")
        case .error:
          fputs("Error: \(message)\n", stderr)
        }
    }
    func printUsage() {

      let executableName = (CommandLine.arguments[0] as NSString).lastPathComponent
            
      writeMessage("usage:")
      writeMessage("\(executableName) -i FW Update File Path -u Update Procedure")
        writeMessage(" 0 52 1 91 2 Both")
    }
}
