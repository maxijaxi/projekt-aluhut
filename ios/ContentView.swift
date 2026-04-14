import SwiftUI

struct ContentView: View {
    @StateObject var bt = BluetoothManager()
    @State var eingabe = ""
    @FocusState var tastaturAktiv: Bool
    
    var body: some View {
        VStack(spacing: 0) {
            
            // Header
            HStack {
                Circle()
                    .fill(bt.verbunden ? Color.green : Color.red)
                    .frame(width: 10, height: 10)
                Text(bt.status)
                    .font(.caption)
                    .foregroundColor(.secondary)
                Spacer()
                if bt.verbunden {
                    Button("Trennen") {
                        bt.trennen()
                    }
                    .font(.caption)
                    .foregroundColor(.red)
                }
            }
            .padding(.horizontal)
            .padding(.vertical, 8)
            .background(Color(.systemGray6))
            
            Divider()
            
            // Nachrichten
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(spacing: 8) {
                        ForEach(bt.nachrichten) { nachricht in
                            NachrichtView(nachricht: nachricht)
                                .id(nachricht.id)
                        }
                    }
                    .padding()
                }
                .onChange(of: bt.nachrichten.count) { _ in
                    if let letzte = bt.nachrichten.last {
                        withAnimation {
                            proxy.scrollTo(letzte.id, anchor: .bottom)
                        }
                    }
                }
            }
            
            Divider()
            
            // Eingabe
            HStack(spacing: 8) {
                TextField("Nachricht...", text: $eingabe)
                    .textFieldStyle(.roundedBorder)
                    .focused($tastaturAktiv)
                    .onSubmit { senden() }
                
                Button(action: senden) {
                    Image(systemName: "paperplane.fill")
                        .foregroundColor(eingabe.isEmpty || !bt.verbunden ? .gray : .blue)
                }
                .disabled(eingabe.isEmpty || !bt.verbunden)
            }
            .padding()
        }
        .navigationTitle("BT Messenger")
    }
    
    func senden() {
        guard !eingabe.isEmpty else { return }
        bt.senden(eingabe)
        eingabe = ""
    }
}

struct NachrichtView: View {
    let nachricht: Nachricht
    
    var body: some View {
        HStack {
            if nachricht.vonMir { Spacer() }
            
            VStack(alignment: nachricht.vonMir ? .trailing : .leading, spacing: 2) {
                Text(nachricht.text)
                    .padding(.horizontal, 12)
                    .padding(.vertical, 8)
                    .background(nachricht.vonMir ? Color.blue : Color(.systemGray5))
                    .foregroundColor(nachricht.vonMir ? .white : .primary)
                    .clipShape(RoundedRectangle(cornerRadius: 16))
                
                Text(nachricht.zeitFormatiert)
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
            
            if !nachricht.vonMir { Spacer() }
        }
    }
}
