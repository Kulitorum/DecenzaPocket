import Foundation

enum PowerCommand: String {
    case wake, sleep
}

struct WebSocketCommand {
    static let wsURL = URL(string: "wss://ws.decenza.coffee")!

    static func send(_ command: PowerCommand, completion: @escaping (Bool) -> Void) {
        guard SharedState.isPaired else {
            completion(false)
            return
        }

        let deviceId = SharedState.deviceId
        let pairingToken = SharedState.pairingToken
        let session = URLSession(configuration: .default)
        let task = session.webSocketTask(with: wsURL)
        task.resume()

        // Register as controller
        let register: [String: Any] = [
            "action": "register",
            "device_id": deviceId,
            "role": "controller",
            "pairing_token": pairingToken
        ]

        guard let registerData = try? JSONSerialization.data(withJSONObject: register),
              let registerStr = String(data: registerData, encoding: .utf8) else {
            completion(false)
            return
        }

        task.send(.string(registerStr)) { error in
            if error != nil {
                completion(false)
                return
            }

            // Wait for "registered" response, then send command
            task.receive { result in
                guard case .success(let message) = result,
                      case .string(let text) = message,
                      let json = try? JSONSerialization.jsonObject(with: Data(text.utf8)) as? [String: Any],
                      json["type"] as? String == "registered" else {
                    task.cancel(with: .normalClosure, reason: nil)
                    completion(false)
                    return
                }

                // Send the command
                let cmd: [String: Any] = [
                    "action": "command",
                    "device_id": deviceId,
                    "pairing_token": pairingToken,
                    "command": command.rawValue
                ]

                guard let cmdData = try? JSONSerialization.data(withJSONObject: cmd),
                      let cmdStr = String(data: cmdData, encoding: .utf8) else {
                    task.cancel(with: .normalClosure, reason: nil)
                    completion(false)
                    return
                }

                task.send(.string(cmdStr)) { error in
                    if error != nil {
                        task.cancel(with: .normalClosure, reason: nil)
                        completion(false)
                        return
                    }

                    // Wait for command_sent
                    task.receive { result in
                        var success = false
                        if case .success(let msg) = result,
                           case .string(let txt) = msg,
                           let js = try? JSONSerialization.jsonObject(with: Data(txt.utf8)) as? [String: Any],
                           js["type"] as? String == "command_sent" {
                            success = true
                        }
                        task.cancel(with: .normalClosure, reason: nil)
                        completion(success)
                    }
                }
            }
        }
    }
}
