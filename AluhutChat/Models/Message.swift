import Foundation

struct Message: Identifiable {
    let id = UUID()
    let text: String
    let isSent: Bool
    let timestamp: Date
}
