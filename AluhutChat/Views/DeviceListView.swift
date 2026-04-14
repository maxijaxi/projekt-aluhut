import SwiftUI

struct DeviceListView: View {
    @ObservedObject var bleManager: BLEManager

    var body: some View {
        NavigationStack {
            Group {
                if !bleManager.isBluetoothAvailable {
                    placeholder(
                        icon: "bluetooth",
                        title: "Bluetooth nicht verfügbar",
                        message: "Bitte aktiviere Bluetooth in den Einstellungen."
                    )
                } else if bleManager.discoveredDevices.isEmpty {
                    if bleManager.state == .scanning {
                        VStack(spacing: 16) {
                            ProgressView()
                                .scaleEffect(1.4)
                            Text("Suche nach HM-10 Modulen…")
                                .foregroundStyle(.secondary)
                        }
                    } else {
                        placeholder(
                            icon: "antenna.radiowaves.left.and.right",
                            title: "Keine Geräte gefunden",
                            message: "Tippe auf „Suchen", um nach HM-10 Bluetooth-Modulen zu suchen."
                        )
                    }
                } else {
                    List(bleManager.discoveredDevices) { device in
                        Button {
                            bleManager.connect(to: device)
                        } label: {
                            HStack {
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(device.name)
                                        .font(.headline)
                                        .foregroundStyle(.primary)
                                    Text("RSSI: \(device.rssi) dBm")
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                }
                                Spacer()
                                if bleManager.state == .connecting {
                                    ProgressView()
                                } else {
                                    Image(systemName: "chevron.right")
                                        .foregroundStyle(.secondary)
                                }
                            }
                        }
                    }
                }
            }
            .navigationTitle("Aluhut Chat")
            .toolbar {
                ToolbarItem(placement: .primaryAction) {
                    Button(bleManager.state == .scanning ? "Stop" : "Suchen") {
                        if bleManager.state == .scanning {
                            bleManager.stopScanning()
                        } else {
                            bleManager.startScanning()
                        }
                    }
                    .disabled(!bleManager.isBluetoothAvailable || bleManager.state == .connecting)
                }
            }
        }
    }

    @ViewBuilder
    private func placeholder(icon: String, title: String, message: String) -> some View {
        VStack(spacing: 16) {
            Image(systemName: icon)
                .font(.system(size: 52))
                .foregroundStyle(.secondary)
            Text(title)
                .font(.title3.bold())
            Text(message)
                .font(.subheadline)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal, 32)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}
