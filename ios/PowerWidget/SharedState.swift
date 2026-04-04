import Foundation

struct SharedState {
    static let suiteName = "group.io.github.kulitorum.decenzapocket"

    private static var defaults: UserDefaults? {
        UserDefaults(suiteName: suiteName)
    }

    static var deviceId: String {
        get { defaults?.string(forKey: "device_id") ?? "" }
        set { defaults?.set(newValue, forKey: "device_id") }
    }

    static var pairingToken: String {
        get { defaults?.string(forKey: "pairingToken") ?? "" }
        set { defaults?.set(newValue, forKey: "pairingToken") }
    }

    static var isAwake: Bool {
        get { defaults?.bool(forKey: "isAwake") ?? false }
        set { defaults?.set(newValue, forKey: "isAwake") }
    }

    static var isPaired: Bool {
        !deviceId.isEmpty && !pairingToken.isEmpty
    }
}
