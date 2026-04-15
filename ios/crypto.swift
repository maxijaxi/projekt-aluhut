import Foundation

/// Plaintext packet handling (no encryption, no checksum).
/// Messages are sent/received as UTF-8 text terminated by newline.
enum Crypto {

    // MARK: - Packet builder (iOS → Arduino)

    /// Builds a plaintext packet: UTF-8 bytes + newline
    static func buildPacket(plaintext: String) -> Data? {
        guard let data = (plaintext + "\n").data(using: .utf8) else { return nil }
        return data
    }

    // MARK: - Packet parser (Arduino → iOS)

    /// Parses a received packet: strip newline/carriage-return, return as string
    static func parsePacket(_ bytes: [UInt8]) -> String? {
        let trimmed = bytes.filter { $0 != UInt8(ascii: "\n") && $0 != UInt8(ascii: "\r") }
        guard !trimmed.isEmpty else { return nil }
        return String(bytes: trimmed, encoding: .utf8)
    }
}
