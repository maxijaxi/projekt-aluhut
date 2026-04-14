import Foundation
import CoreBluetooth

class BluetoothManager: NSObject, ObservableObject {
    
    // HM-10 UUIDs
    let serviceUUID = CBUUID(string: "FFE0")
    let characteristicUUID = CBUUID(string: "FFE1")
    
    var centralManager: CBCentralManager!
    var peripheral: CBPeripheral?
    var characteristic: CBCharacteristic?
    
    @Published var nachrichten: [Nachricht] = []
    @Published var verbunden = false
    @Published var status = "Suche nach HM-10..."
    
    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }
    
    func senden(_ text: String) {
        guard let characteristic = characteristic,
              let peripheral = peripheral,
              let data = (text + "\n").data(using: .utf8) else { return }
        
        peripheral.writeValue(data, for: characteristic, type: .withoutResponse)
        
        let nachricht = Nachricht(text: text, vonMir: true)
        DispatchQueue.main.async {
            self.nachrichten.append(nachricht)
        }
    }
    
    func scannen() {
        guard centralManager.state == .poweredOn else { return }
        status = "Suche nach HM-10..."
        centralManager.scanForPeripherals(withServices: [serviceUUID])
    }
    
    func trennen() {
        guard let peripheral = peripheral else { return }
        centralManager.cancelPeripheralConnection(peripheral)
    }
}

// MARK: - CBCentralManagerDelegate
extension BluetoothManager: CBCentralManagerDelegate {
    
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            scannen()
        case .poweredOff:
            status = "Bluetooth ist aus"
            verbunden = false
        default:
            status = "Bluetooth nicht verfügbar"
        }
    }
    
    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        
        let name = peripheral.name ?? "Unbekannt"
        status = "Gefunden: \(name) – Verbinde..."
        
        self.peripheral = peripheral
        centralManager.stopScan()
        centralManager.connect(peripheral)
    }
    
    func centralManager(_ central: CBCentralManager,
                        didConnect peripheral: CBPeripheral) {
        status = "Verbunden!"
        verbunden = true
        peripheral.delegate = self
        peripheral.discoverServices([serviceUUID])
    }
    
    func centralManager(_ central: CBCentralManager,
                        didDisconnectPeripheral peripheral: CBPeripheral,
                        error: Error?) {
        status = "Getrennt – suche erneut..."
        verbunden = false
        self.peripheral = nil
        self.characteristic = nil
        scannen()
    }
}

// MARK: - CBPeripheralDelegate
extension BluetoothManager: CBPeripheralDelegate {
    
    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services {
            peripheral.discoverCharacteristics([characteristicUUID], for: service)
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        guard let characteristics = service.characteristics else { return }
        for characteristic in characteristics {
            if characteristic.uuid == characteristicUUID {
                self.characteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                status = "Bereit zum Chatten!"
            }
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard let data = characteristic.value,
              let text = String(data: data, encoding: .utf8) else { return }
        
        let bereinigt = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard bereinigt.count > 0 else { return }
        
        let nachricht = Nachricht(text: bereinigt, vonMir: false)
        DispatchQueue.main.async {
            self.nachrichten.append(nachricht)
        }
    }
}
