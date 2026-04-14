import CoreBluetooth
import Combine
import Foundation

// MARK: - HM-10 UUIDs

/// Standard BLE UART service exposed by the HM-10 module
private let hm10ServiceUUID = CBUUID(string: "FFE0")

/// The single TX/RX characteristic of the HM-10 (read, write without response, notify)
private let hm10CharUUID = CBUUID(string: "FFE1")

// MARK: - Supporting types

struct BLEDevice: Identifiable {
    let id: UUID
    let name: String
    let peripheral: CBPeripheral
    let rssi: Int
}

enum BLEState {
    case idle, scanning, connecting, connected, disconnected
}

// MARK: - BLEManager

final class BLEManager: NSObject, ObservableObject {

    @Published var state: BLEState = .idle
    @Published var discoveredDevices: [BLEDevice] = []
    @Published var connectedDevice: BLEDevice?
    @Published var isBluetoothAvailable = false

    /// Called on the main actor whenever a complete, verified message arrives
    var onMessageReceived: ((String) -> Void)?

    // CoreBluetooth objects (accessed only from the CB dispatch queue / main actor)
    private var central: CBCentralManager!
    private var connectedPeripheral: CBPeripheral?
    private var txChar: CBCharacteristic?

    /// Accumulates incoming BLE chunks until a full packet (ending in '\n') is received
    private var receiveBuffer: [UInt8] = []

    override init() {
        super.init()
        // Using DispatchQueue.main keeps all CoreBluetooth callbacks on the main thread.
        central = CBCentralManager(delegate: self, queue: .main)
    }

    // MARK: - Public API

    func startScanning() {
        guard central.state == .poweredOn else { return }
        discoveredDevices.removeAll()
        state = .scanning
        // Scan for any peripheral that advertises the HM-10 service
        central.scanForPeripherals(withServices: [hm10ServiceUUID],
                                   options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }

    func stopScanning() {
        central.stopScan()
        state = .idle
    }

    func connect(to device: BLEDevice) {
        state = .connecting
        central.stopScan()
        central.connect(device.peripheral, options: nil)
    }

    func disconnect() {
        guard let p = connectedPeripheral else { return }
        central.cancelPeripheralConnection(p)
    }

    /// Encrypts and sends a message to the Arduino via the HM-10 characteristic.
    /// The packet is chunked into ≤20-byte pieces to respect the default BLE ATT MTU.
    func sendMessage(_ plaintext: String) {
        guard let char = txChar,
              let peripheral = connectedPeripheral,
              let packet = Crypto.buildPacket(plaintext: plaintext) else { return }

        let chunkSize = 20
        var offset = 0
        while offset < packet.count {
            let end = min(offset + chunkSize, packet.count)
            peripheral.writeValue(packet.subdata(in: offset..<end),
                                  for: char,
                                  type: .withoutResponse)
            offset = end
        }
    }
}

// MARK: - CBCentralManagerDelegate

extension BLEManager: CBCentralManagerDelegate {

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        isBluetoothAvailable = central.state == .poweredOn
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        let name = peripheral.name
            ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String
            ?? "HM-10"
        let device = BLEDevice(id: peripheral.identifier, name: name,
                               peripheral: peripheral, rssi: RSSI.intValue)
        if !discoveredDevices.contains(where: { $0.id == device.id }) {
            discoveredDevices.append(device)
        }
    }

    func centralManager(_ central: CBCentralManager,
                        didConnect peripheral: CBPeripheral) {
        connectedPeripheral = peripheral
        peripheral.delegate = self
        peripheral.discoverServices([hm10ServiceUUID])
        if let device = discoveredDevices.first(where: { $0.id == peripheral.identifier }) {
            connectedDevice = device
        }
        state = .connected
    }

    func centralManager(_ central: CBCentralManager,
                        didFailToConnect peripheral: CBPeripheral,
                        error: Error?) {
        state = .disconnected
        connectedDevice = nil
    }

    func centralManager(_ central: CBCentralManager,
                        didDisconnectPeripheral peripheral: CBPeripheral,
                        error: Error?) {
        state = .disconnected
        connectedDevice = nil
        connectedPeripheral = nil
        txChar = nil
        receiveBuffer.removeAll()
    }
}

// MARK: - CBPeripheralDelegate

extension BLEManager: CBPeripheralDelegate {

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services {
            peripheral.discoverCharacteristics([hm10CharUUID], for: service)
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        guard let characteristics = service.characteristics else { return }
        for char in characteristics where char.uuid == hm10CharUUID {
            peripheral.setNotifyValue(true, for: char)
            txChar = char
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard let data = characteristic.value else { return }
        let bytes = [UInt8](data)
        receiveBuffer.append(contentsOf: bytes)
        // Split buffer on newlines in a single pass to avoid repeated O(n) scans
        let newline = UInt8(ascii: "\n")
        var start = 0
        for i in 0..<receiveBuffer.count where receiveBuffer[i] == newline {
            let packetBytes = Array(receiveBuffer[start...i])
            start = i + 1
            if let message = Crypto.parsePacket(packetBytes) {
                onMessageReceived?(message)
            }
        }
        if start > 0 {
            if start == receiveBuffer.count {
                receiveBuffer.removeAll(keepingCapacity: true)
            } else {
                receiveBuffer.removeFirst(start)
            }
        }
    }
}
