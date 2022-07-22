//
//  main.swift
//  BDUNK
//
//  Created by Nick Krahé on 17.05.22.
//

import Foundation

let bdu = BD()
if CommandLine.argc < 3{
    bdu.usage()
}else{
    bdu.run()
}

