import SwiftUI

struct ChatView: View {
    @ObservedObject var bleManager: BLEManager
    @State private var messages: [Message] = []
    @State private var inputText: String = ""
    @FocusState private var inputFocused: Bool

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                // MARK: Message list
                ScrollViewReader { proxy in
                    ScrollView {
                        LazyVStack(spacing: 8) {
                            ForEach(messages) { msg in
                                MessageBubble(message: msg)
                                    .id(msg.id)
                            }
                        }
                        .padding(.vertical, 8)
                        .padding(.horizontal, 12)
                    }
                    .onAppear {
                        bleManager.onMessageReceived = { text in
                            append(Message(text: text, isSent: false, timestamp: .now))
                            scrollToLast(proxy: proxy)
                        }
                    }
                    .onChange(of: messages.count) {
                        scrollToLast(proxy: proxy)
                    }
                }

                Divider()

                // MARK: Input bar
                HStack(spacing: 10) {
                    TextField("Nachricht…", text: $inputText, axis: .vertical)
                        .textFieldStyle(.roundedBorder)
                        .lineLimit(1...4)
                        .submitLabel(.send)
                        .focused($inputFocused)
                        .onSubmit(sendMessage)

                    Button(action: sendMessage) {
                        Image(systemName: "paperplane.fill")
                            .imageScale(.large)
                    }
                    .disabled(inputText.trimmingCharacters(in: .whitespaces).isEmpty)
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
            }
            .navigationTitle(bleManager.connectedDevice?.name ?? "Chat")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Trennen", role: .destructive) {
                        bleManager.disconnect()
                    }
                }
            }
        }
    }

    // MARK: - Helpers

    private func sendMessage() {
        let text = inputText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.isEmpty else { return }
        inputText = ""
        bleManager.sendMessage(text)
        append(Message(text: text, isSent: true, timestamp: .now))
    }

    private func append(_ message: Message) {
        messages.append(message)
    }

    private func scrollToLast(proxy: ScrollViewProxy) {
        guard let last = messages.last else { return }
        withAnimation(.easeOut(duration: 0.25)) {
            proxy.scrollTo(last.id, anchor: .bottom)
        }
    }
}

// MARK: - Message bubble

struct MessageBubble: View {
    let message: Message

    var body: some View {
        HStack(alignment: .bottom, spacing: 4) {
            if message.isSent { Spacer(minLength: 60) }

            VStack(alignment: message.isSent ? .trailing : .leading, spacing: 3) {
                Text(message.text)
                    .padding(.horizontal, 12)
                    .padding(.vertical, 8)
                    .background(message.isSent ? Color.accentColor : Color(.systemGray5))
                    .foregroundStyle(message.isSent ? Color.white : Color.primary)
                    .clipShape(RoundedRectangle(cornerRadius: 16))

                Text(message.timestamp, style: .time)
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .padding(.horizontal, 4)
            }

            if !message.isSent { Spacer(minLength: 60) }
        }
    }
}
