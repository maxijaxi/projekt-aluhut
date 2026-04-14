import SwiftUI

struct AluhutChatRootView: View {
    @StateObject private var bleManager = BLEManager()

    var body: some View {
        switch bleManager.state {
        case .connected:
            ChatView(bleManager: bleManager)
        default:
            DeviceListView(bleManager: bleManager)
        }
    }
}
