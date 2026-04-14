import Foundation

/// Mirrors the existing Arduino utility functions:
///   - XOR encryption  (util/encryption.cpp, key = 0xAA)
///   - Checksum        (util/checkSum.cpp,   sum of bytes mod 256)
///   - Packet format   (util/readyForSend.cpp, "{data}\x03{decimal_checksum}\n")
enum Crypto {

    // MARK: - Constants

    /// XOR key used on the Arduino side (encryption.cpp)
    static let xorKey: UInt8 = 0xAA

    /// ETX byte used as separator between payload and checksum (readyForSend.cpp)
    static let etx: UInt8 = 0x03

    // MARK: - XOR (symmetric – same function encrypts and decrypts)

    static func xorTransform(_ bytes: [UInt8], key: UInt8 = xorKey) -> [UInt8] {
        bytes.map { $0 ^ key }
    }

    // MARK: - Checksum

    /// Sum of all bytes, wrapping at 256 – matches the Arduino byte arithmetic
    static func checksum(_ bytes: [UInt8]) -> UInt8 {
        bytes.reduce(0, &+)
    }

    // MARK: - Packet builder (iOS → Arduino)

    /// Builds a packet that the Arduino can receive and verify:
    ///   1. Compute checksum of plaintext bytes
    ///   2. XOR-encrypt the plaintext
    ///   3. Append ETX + decimal checksum string + newline
    ///
    /// Resulting wire format: `{xor_encrypted_payload}\x03{decimal_checksum}\n`
    static func buildPacket(plaintext: String) -> Data? {
        guard let plaintextBytes = plaintext.data(using: .utf8).map({ [UInt8]($0) }) else {
            return nil
        }
        let csum = checksum(plaintextBytes)
        let encrypted = xorTransform(plaintextBytes)

        var packet = encrypted
        packet.append(etx)
        // Decimal checksum followed by newline (matches sprintf "%d\n")
        packet.append(contentsOf: "\(csum)\n".utf8)
        return Data(packet)
    }

    // MARK: - Packet parser (Arduino → iOS)

    /// Parses a packet received from the Arduino:
    ///   1. Split at ETX to get encrypted payload and checksum string
    ///   2. XOR-decrypt the payload
    ///   3. Verify checksum against the decrypted bytes
    ///
    /// Returns the plaintext string if the packet is valid, otherwise `nil`.
    static func parsePacket(_ bytes: [UInt8]) -> String? {
        guard let etxIndex = bytes.firstIndex(of: etx) else { return nil }

        let encryptedPayload = Array(bytes[..<etxIndex])
        let checksumPart = Array(bytes[(etxIndex + 1)...])

        // Strip trailing newline / carriage-return before parsing the decimal number
        let csumBytes = checksumPart.filter { $0 != UInt8(ascii: "\n") && $0 != UInt8(ascii: "\r") }
        guard let csumStr = String(bytes: csumBytes, encoding: .utf8),
              let receivedChecksum = UInt8(csumStr) else { return nil }

        let decrypted = xorTransform(encryptedPayload)
        guard checksum(decrypted) == receivedChecksum else { return nil }

        return String(bytes: decrypted, encoding: .utf8)
    }
}
