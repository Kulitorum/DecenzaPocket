import WidgetKit
import SwiftUI
import AppIntents

// MARK: - Toggle Intent

struct TogglePowerIntent: AppIntent {
    static var title: LocalizedStringResource = "Toggle Decenza Power"
    static var description = IntentDescription("Toggles the espresso machine on or off")

    func perform() async throws -> some IntentResult {
        guard SharedState.isPaired else {
            return .result()
        }

        let currentState = SharedState.isAwake
        let command: PowerCommand = currentState ? .sleep : .wake

        await withCheckedContinuation { continuation in
            WebSocketCommand.send(command) { success in
                if success {
                    SharedState.isAwake = !currentState
                }
                continuation.resume()
            }
        }

        return .result()
    }
}

// MARK: - Timeline Provider

struct PowerTimelineProvider: TimelineProvider {
    func placeholder(in context: Context) -> PowerEntry {
        PowerEntry(date: Date(), isAwake: false)
    }

    func getSnapshot(in context: Context, completion: @escaping (PowerEntry) -> Void) {
        completion(PowerEntry(date: Date(), isAwake: SharedState.isAwake))
    }

    func getTimeline(in context: Context, completion: @escaping (Timeline<PowerEntry>) -> Void) {
        let entry = PowerEntry(date: Date(), isAwake: SharedState.isAwake)
        let timeline = Timeline(entries: [entry], policy: .never)
        completion(timeline)
    }
}

// MARK: - Timeline Entry

struct PowerEntry: TimelineEntry {
    let date: Date
    let isAwake: Bool
}

// MARK: - Widget View

struct PowerWidgetView: View {
    var entry: PowerEntry

    var body: some View {
        Button(intent: TogglePowerIntent()) {
            Image(entry.isAwake ? "WidgetOn" : "WidgetOff")
                .resizable()
                .aspectRatio(contentMode: .fit)
        }
        .buttonStyle(.plain)
    }
}

// MARK: - Widget Configuration

struct PowerWidget: Widget {
    let kind = "PowerWidget"

    var body: some WidgetConfiguration {
        StaticConfiguration(kind: kind, provider: PowerTimelineProvider()) { entry in
            PowerWidgetView(entry: entry)
                .containerBackground(.clear, for: .widget)
        }
        .configurationDisplayName("Decenza Power")
        .description("Toggle your espresso machine on or off")
        .supportedFamilies([.systemSmall])
    }
}
