import Foundation

struct Nachricht: Identifiable {
    let id = UUID()
    let text: String
    let vonMir: Bool
    let zeit = Date()
    
    var zeitFormatiert: String {
        let f = DateFormatter()
        f.timeStyle = .short
        return f.string(from: zeit)
    }
}
