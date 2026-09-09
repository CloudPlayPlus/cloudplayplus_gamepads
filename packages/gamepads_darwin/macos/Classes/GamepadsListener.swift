import Foundation
import GameController

class GamepadsListener {
    var gamepads: [GCExtendedGamepad] = []
    var listener: ((Int, GCExtendedGamepad, GCControllerElement) -> Void)?

    private var gamepadIds: [ObjectIdentifier: Int] = [:]
    init() {
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(joystickDidConnect),
            name: .GCControllerDidConnect,
            object: nil
        )
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(joystickDidDisconnect),
            name: .GCControllerDidDisconnect,
            object: nil
        )
    }

    func discoverConnectedControllers() {
        for controller in GCController.controllers() {
            joystickDidConnect(notification: NSNotification(name: .GCControllerDidConnect, object: controller))
        }
    }

    deinit {
        NotificationCenter.default.removeObserver(self)
    }
 
    @objc private func joystickDidConnect(notification: NSNotification) {
        if let controller = notification.object as? GCController {
            if let gamepad = controller.extendedGamepad {
                if gamepads.contains(where: { $0 === gamepad }) { return }
                gamepads.append(gamepad)
                let gamepadId = getAndSetPlayerId(of: gamepad)

                gamepad.valueChangedHandler = { gamepad, element in
                    if let listener = self.listener {
                        listener(gamepadId, gamepad, element);
                    }
                }
            }
        }
    }
 
    @objc private func joystickDidDisconnect(notification: NSNotification) {
        if let controller = notification.object as? GCController {
            if let pad = controller.extendedGamepad { gamepadIds.removeValue(forKey: ObjectIdentifier(pad)) }
            gamepads.removeAll(where: { $0 == controller.extendedGamepad })
        }
    }

    func gamepad(for id: Int) -> GCExtendedGamepad? {
        gamepads.first { gamepadIds[ObjectIdentifier($0)] == id }
    }

    func id(for gamepad: GCExtendedGamepad) -> Int {
        gamepadIds[ObjectIdentifier(gamepad)] ?? -1
    }

    private func getAndSetPlayerId(of gamepad: GCExtendedGamepad) -> Int {
        var gamepadId = 0
        while gamepadIds.values.contains(gamepadId) { gamepadId += 1 }
        gamepadIds[ObjectIdentifier(gamepad)] = gamepadId
        gamepad.controller?.playerIndex = toPlayerIndex(index: gamepadId)
        return gamepadId
    }

    private func toPlayerIndex(index: Int) -> GCControllerPlayerIndex {
        switch index {
        case 0:
            return GCControllerPlayerIndex.index1
        case 1:
            return GCControllerPlayerIndex.index2
        case 2:
            return GCControllerPlayerIndex.index3
        case 3:
            return GCControllerPlayerIndex.index4
        default:
            return GCControllerPlayerIndex.indexUnset
        }
    }
}
